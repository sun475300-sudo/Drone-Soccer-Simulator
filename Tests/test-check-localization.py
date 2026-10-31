#!/usr/bin/env python3
"""Synthetic unit tests for Scripts/check-localization.py.

These tests cover only static source parsing and report policy.  They do not
compile Unreal code or exercise an Unreal HUD at runtime.
"""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


REPOSITORY = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPOSITORY / "Scripts" / "check-localization.py"
SPEC = importlib.util.spec_from_file_location("check_localization", SCRIPT_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("could not load check-localization.py")
CHECKER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = CHECKER
SPEC.loader.exec_module(CHECKER)

CHECKS = 0
FAILURES = 0


def Expect(condition: bool, name: str) -> None:
    global CHECKS, FAILURES
    CHECKS += 1
    if not condition:
        FAILURES += 1
        print(f"FAIL {name}", file=sys.stderr)


def write_fixture(root: Path) -> tuple[Path, Path]:
    source_root = root / "Source"
    source_root.mkdir(parents=True)
    table = source_root / "ImperialLocalization.cpp"
    table.write_text(
        """namespace {
struct FTranslation { const TCHAR* English; const TCHAR* Korean; };
const FTranslation Translations[] =
{
    {TEXT("Good %d {name}"), TEXT("정상 %d {name}")},
    {TEXT("Missing Korean"), },
    {, TEXT("영어 없음")},
    {TEXT(""), TEXT("빈 영어")},
    {TEXT("Empty Korean"), TEXT("")},
    {TEXT("Identical wording needs review"), TEXT("Identical wording needs review")},
    {TEXT("Printf %d %s"), TEXT("서식 %d")},
    {TEXT("Brace {0}"), TEXT("중괄호 {1}")},
    {TEXT("Duplicate"), TEXT("중복 하나")},
    {TEXT("Duplicate"), TEXT("중복 둘")},
    {TEXT("extra"), TEXT("추가"), TEXT("third")},
};
}
""",
        encoding="utf-8",
    )
    (source_root / "Hud.cpp").write_text(
        """void Draw()
{
    DrawText(TEXT("This English HUD notice bypasses the localization table."));
    DrawText(ImperialLocalization::Translate(TEXT("This translated English HUD notice is wrapped.")));
    UE_LOG(LogTemp, Display, TEXT("로그 전용 한글"));
    // TEXT("주석 안의 한글은 리터럴이 아닙니다")
}
""",
        encoding="utf-8",
    )
    return source_root, table


def only_count(report: dict, name: str, expected: int, title: str) -> None:
    Expect(report["counts"].get(name) == expected, title)


def main() -> int:
    temporary = Path(tempfile.mkdtemp(prefix="check-localization-tests-"))
    try:
        source_root, table = write_fixture(temporary)
        report = CHECKER.build_report(source_root, table, None)
        Expect(report["translation_entry_count"] == 11, "parses all translation rows")
        only_count(report, "missing_english", 1, "reports a missing English field")
        only_count(report, "missing_korean", 1, "reports a missing Korean field")
        only_count(report, "empty_english", 1, "reports an empty English field")
        only_count(report, "empty_korean", 1, "reports an empty Korean field")
        only_count(report, "identical_probably_untranslated", 1, "reports an identical sentence-like translation")
        only_count(report, "format_placeholder_mismatch", 2, "reports printf and brace placeholder mismatches")
        only_count(report, "duplicate_english_keys", 1, "reports duplicate English lookup keys")
        only_count(report, "malformed_table_entries", 1, "reports a table row with three fields")
        only_count(report, "bypassed_hangul_literals", 1, "reports a Hangul literal outside the table")
        only_count(report, "bypassed_english_hud_literals", 1, "reports a direct English HUD notice")
        Expect(report["counts"]["total_findings"] == 11, "sums all synthetic findings")
        mismatch = report["findings"]["format_placeholder_mismatch"]
        Expect(any(item["english_placeholders"] == ["printf:%d", "printf:%s"] for item in mismatch), "preserves printf placeholder types")
        Expect(any(item["english_placeholders"] == ["brace:{0}"] for item in mismatch), "preserves brace placeholder names")
        Expect("comment" not in " ".join(item["literal"] for item in report["findings"]["bypassed_hangul_literals"]), "ignores comment-only pseudo literals")
        summary = CHECKER.markdown_summary(report)
        Expect("형식 인수 불일치 | 2" in summary, "Markdown summary includes category counts")
        Expect("This English HUD notice" in summary, "Markdown summary includes top bypass finding")

        allowlist = temporary / "allowlist.json"
        allowlist.write_text(
            json.dumps(
                {
                    "entries": [
                        {
                            "path": "Hud.cpp",
                            "literal": "로그 전용 한글",
                            "reason": "UE_LOG diagnostic only",
                        }
                    ]
                },
                ensure_ascii=False,
            ),
            encoding="utf-8",
        )
        allowed_report = CHECKER.build_report(source_root, table, allowlist)
        only_count(allowed_report, "bypassed_hangul_literals", 0, "allowlist suppresses exact log-only Hangul literal")
        Expect(allowed_report["counts"]["total_findings"] == 10, "allowlist changes only the matching finding")

        json_report = temporary / "report.json"
        markdown_report = temporary / "report.md"
        command = [
            sys.executable,
            str(SCRIPT_PATH),
            "--source-root",
            str(source_root),
            "--table",
            str(table),
            "--json-report",
            str(json_report),
            "--markdown-summary",
            str(markdown_report),
        ]
        normal = subprocess.run(command, cwd=REPOSITORY, capture_output=True, text=True, check=False)
        Expect(normal.returncode == 0, "non-strict invocation reports findings without failing")
        Expect(json_report.is_file() and markdown_report.is_file(), "CLI writes requested JSON and Markdown reports")
        parsed_json = json.loads(json_report.read_text(encoding="utf-8"))
        Expect(parsed_json["counts"]["total_findings"] == 11, "CLI JSON preserves finding count")
        strict = subprocess.run(command + ["--strict"], cwd=REPOSITORY, capture_output=True, text=True, check=False)
        Expect(strict.returncode == 1, "strict invocation fails when findings exist")
        Expect("11 findings" in strict.stdout, "CLI prints concise finding count")
    finally:
        shutil.rmtree(temporary, ignore_errors=True)

    print("Static localization checker tests only; Unreal compilation and runtime were not run.")
    print(f"{CHECKS} checks, {FAILURES} failures")
    return 1 if FAILURES else 0


if __name__ == "__main__":
    raise SystemExit(main())
