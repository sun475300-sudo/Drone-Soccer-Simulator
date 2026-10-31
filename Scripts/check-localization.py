#!/usr/bin/env python3
"""Check the current Imperial Wings Korean/English display-string coverage.

The current Unreal implementation stores FTranslation rows in
ImperialLocalization.cpp.  English source strings are both lookup keys and the
English display values; Korean is the translated value.  This checker is a
static aid only: it does not compile Unreal code or prove that a HUD path is
rendered at runtime.

Examples:
  python3 Scripts/check-localization.py \
    --source-root Source/ImperialWingsSimulator \
    --json-report /tmp/localization.json \
    --markdown-summary /tmp/localization.md

  # Make any reported finding fail a CI invocation.
  python3 Scripts/check-localization.py ... --strict

Optional allowlist JSON is deliberately narrow and auditable.  It accepts an
"entries" list, for example:
{
  "entries": [
    {"path": "SomeFile.cpp", "literal": "log-only Korean text", "reason": "UE_LOG diagnostic"}
  ]
}
"""

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
import json
import re
import sys
from typing import Any, Iterable, Optional


SCHEMA_VERSION = 1
TABLE_START = re.compile(r"const\s+FTranslation\s+Translations\s*\[\s*\]\s*=")
TEXT_LITERAL = re.compile(r'\bTEXT\s*\(\s*"((?:\\.|[^"\\])*)"\s*\)', re.DOTALL)
NORMAL_LITERAL = re.compile(r'(?:u8|u|U|L)?"((?:\\.|[^"\\])*)"', re.DOTALL)
PRINTF_PLACEHOLDER = re.compile(
    r"%(?:\d+\$)?[-+0#]*(?:\d+)?(?:\.\d+)?(?:hh|ll|h|l|j|z|t|L)?[diuoxXfFeEgGaAcsp]"
)
BRACE_PLACEHOLDER = re.compile(r"(?<!\{)\{[A-Za-z0-9_]+\}(?!\})")
HANGUL = re.compile(r"[\uac00-\ud7a3]")
WORD = re.compile(r"[A-Za-z]{2,}")
HUD_OR_NOTICE_CALL = re.compile(
    r"\b(?:DrawText|AddOnScreenDebugMessage|SetText|SetNotice|ShowNotice|"
    r"ShowMessage|AddNotice|AddMessage|Toast|Notify(?:User|[A-Za-z0-9_]*)?)\s*\("
)


@dataclass(frozen=True)
class SourceLiteral:
    path: Path
    relative_path: str
    line: int
    value: str
    context: str


@dataclass(frozen=True)
class TableRow:
    index: int
    line: int
    english: Optional[str]
    korean: Optional[str]


def _line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def _cpp_unescape(value: str) -> str:
    """Decode enough ordinary C++ string escapes for deterministic comparisons."""
    result: list[str] = []
    index = 0
    simple = {"n": "\n", "r": "\r", "t": "\t", "\\": "\\", '"': '"', "'": "'", "?": "?"}
    while index < len(value):
        character = value[index]
        if character != "\\" or index + 1 >= len(value):
            result.append(character)
            index += 1
            continue
        marker = value[index + 1]
        if marker in simple:
            result.append(simple[marker])
            index += 2
        elif marker == "x":
            match = re.match(r"[0-9A-Fa-f]+", value[index + 2 :])
            if match is None:
                result.append("x")
                index += 2
            else:
                result.append(chr(int(match.group(0), 16)))
                index += 2 + len(match.group(0))
        elif marker in {"u", "U"}:
            digits = 4 if marker == "u" else 8
            candidate = value[index + 2 : index + 2 + digits]
            if len(candidate) == digits and re.fullmatch(r"[0-9A-Fa-f]+", candidate):
                result.append(chr(int(candidate, 16)))
                index += 2 + digits
            else:
                result.append(marker)
                index += 2
        elif marker in "01234567":
            match = re.match(r"[0-7]{1,3}", value[index + 1 :])
            assert match is not None
            result.append(chr(int(match.group(0), 8)))
            index += 1 + len(match.group(0))
        else:
            # Preserve an unfamiliar escape as its escaped character.  It remains
            # useful in the report without pretending the checker compiled C++.
            result.append(marker)
            index += 2
    return "".join(result)


def _matching_brace(text: str, opening: int) -> Optional[int]:
    """Return the matching brace while ignoring C++ comments and quoted text."""
    depth = 0
    index = opening
    state = "code"
    while index < len(text):
        character = text[index]
        following = text[index + 1] if index + 1 < len(text) else ""
        if state == "code":
            if character == "/" and following == "/":
                state = "line_comment"
                index += 2
                continue
            if character == "/" and following == "*":
                state = "block_comment"
                index += 2
                continue
            if character == '"':
                state = "string"
                index += 1
                continue
            if character == "'":
                state = "character"
                index += 1
                continue
            if character == "{":
                depth += 1
            elif character == "}":
                depth -= 1
                if depth == 0:
                    return index
            index += 1
            continue
        if state == "line_comment":
            if character == "\n":
                state = "code"
            index += 1
            continue
        if state == "block_comment":
            if character == "*" and following == "/":
                state = "code"
                index += 2
            else:
                index += 1
            continue
        if character == "\\":
            index += 2
        elif (state == "string" and character == '"') or (state == "character" and character == "'"):
            state = "code"
            index += 1
        else:
            index += 1
    return None


def _top_level_braced_records(text: str, start: int, end: int) -> list[tuple[int, str]]:
    """Extract direct `{ ... }` children from an already located braced body."""
    records: list[tuple[int, str]] = []
    depth = 0
    record_start: Optional[int] = None
    index = start + 1
    state = "code"
    while index < end:
        character = text[index]
        following = text[index + 1] if index + 1 < end else ""
        if state == "code":
            if character == "/" and following == "/":
                state = "line_comment"
                index += 2
                continue
            if character == "/" and following == "*":
                state = "block_comment"
                index += 2
                continue
            if character == '"':
                state = "string"
                index += 1
                continue
            if character == "'":
                state = "character"
                index += 1
                continue
            if character == "{":
                if depth == 0:
                    record_start = index
                depth += 1
            elif character == "}":
                depth -= 1
                if depth == 0 and record_start is not None:
                    records.append((record_start, text[record_start + 1 : index]))
                    record_start = None
            index += 1
            continue
        if state == "line_comment":
            if character == "\n":
                state = "code"
            index += 1
            continue
        if state == "block_comment":
            if character == "*" and following == "/":
                state = "code"
                index += 2
            else:
                index += 1
            continue
        if character == "\\":
            index += 2
        elif (state == "string" and character == '"') or (state == "character" and character == "'"):
            state = "code"
            index += 1
        else:
            index += 1
    return records


def _top_level_comma_fields(record: str) -> list[str]:
    """Split a table row on commas outside strings, comments, and parentheses."""
    fields: list[str] = []
    field_start = 0
    parenthesis_depth = 0
    index = 0
    state = "code"
    while index < len(record):
        character = record[index]
        following = record[index + 1] if index + 1 < len(record) else ""
        if state == "code":
            if character == "/" and following == "/":
                state = "line_comment"
                index += 2
                continue
            if character == "/" and following == "*":
                state = "block_comment"
                index += 2
                continue
            if character == '"':
                state = "string"
            elif character == "'":
                state = "character"
            elif character == "(":
                parenthesis_depth += 1
            elif character == ")" and parenthesis_depth > 0:
                parenthesis_depth -= 1
            elif character == "," and parenthesis_depth == 0:
                fields.append(record[field_start:index])
                field_start = index + 1
            index += 1
            continue
        if state == "line_comment":
            if character == "\n":
                state = "code"
            index += 1
            continue
        if state == "block_comment":
            if character == "*" and following == "/":
                state = "code"
                index += 2
            else:
                index += 1
            continue
        if character == "\\":
            index += 2
        elif (state == "string" and character == '"') or (state == "character" and character == "'"):
            state = "code"
            index += 1
        else:
            index += 1
    fields.append(record[field_start:])
    return fields


def _field_text_value(field: str) -> Optional[str]:
    match = TEXT_LITERAL.search(field)
    return _cpp_unescape(match.group(1)) if match is not None else None


def parse_translation_table(table_path: Path) -> tuple[list[TableRow], list[dict[str, Any]]]:
    """Parse the current FTranslation array without requiring Unreal headers."""
    problems: list[dict[str, Any]] = []
    try:
        source = table_path.read_text(encoding="utf-8")
    except OSError as error:
        return [], [{"kind": "table_read_error", "path": str(table_path), "detail": str(error)}]

    declaration = TABLE_START.search(source)
    if declaration is None:
        return [], [{"kind": "table_not_found", "path": str(table_path), "detail": "FTranslation Translations[] not found"}]
    opening = source.find("{", declaration.end())
    if opening < 0:
        return [], [{"kind": "table_not_found", "path": str(table_path), "detail": "translation array opening brace not found"}]
    closing = _matching_brace(source, opening)
    if closing is None:
        return [], [{"kind": "table_unclosed", "path": str(table_path), "detail": "translation array closing brace not found"}]

    rows: list[TableRow] = []
    for index, (offset, record) in enumerate(_top_level_braced_records(source, opening, closing), start=1):
        fields = _top_level_comma_fields(record)
        english = _field_text_value(fields[0]) if fields else None
        korean = _field_text_value(fields[1]) if len(fields) >= 2 else None
        row = TableRow(index=index, line=_line_number(source, offset), english=english, korean=korean)
        rows.append(row)
        if len(fields) != 2:
            problems.append(
                {
                    "kind": "malformed_table_entry",
                    "line": row.line,
                    "entry": row.index,
                    "detail": f"expected two fields, found {len(fields)}",
                }
            )
    return rows, problems


def _placeholder_list(value: str) -> list[str]:
    printf = [f"printf:{match.group(0)}" for match in PRINTF_PLACEHOLDER.finditer(value)]
    braces = [f"brace:{match.group(0)}" for match in BRACE_PLACEHOLDER.finditer(value)]
    return printf + braces


def _looks_untranslated(english: str, korean: str) -> bool:
    if not english or english != korean or not re.search(r"[A-Za-z]", english):
        return False
    compact = english.strip()
    # Product identity and short acronyms are not translation omissions merely
    # because both language columns intentionally retain them.
    if compact == "IMPERIAL WINGS" or re.fullmatch(r"[A-Z0-9]{1,5}", compact):
        return False
    words = WORD.findall(compact)
    if any(word[0].islower() for word in words):
        return True
    return len(words) >= 2 and all(word.isupper() and len(word) >= 3 for word in words)


def inspect_table(rows: Iterable[TableRow], parser_problems: Iterable[dict[str, Any]]) -> dict[str, list[dict[str, Any]]]:
    findings: dict[str, list[dict[str, Any]]] = {
        "missing_english": [],
        "missing_korean": [],
        "empty_english": [],
        "empty_korean": [],
        "identical_probably_untranslated": [],
        "format_placeholder_mismatch": [],
        "duplicate_english_keys": [],
        "malformed_table_entries": list(parser_problems),
    }
    seen_keys: dict[str, TableRow] = {}
    for row in rows:
        identity = {"entry": row.index, "line": row.line, "english": row.english, "korean": row.korean}
        if row.english is None:
            findings["missing_english"].append(identity)
        elif row.english == "":
            findings["empty_english"].append(identity)
        if row.korean is None:
            findings["missing_korean"].append(identity)
        elif row.korean == "":
            findings["empty_korean"].append(identity)
        if row.english is None or row.korean is None:
            continue
        if row.english in seen_keys:
            first = seen_keys[row.english]
            findings["duplicate_english_keys"].append(
                {"entry": row.index, "line": row.line, "english": row.english, "first_entry": first.index, "first_line": first.line}
            )
        else:
            seen_keys[row.english] = row
        if _looks_untranslated(row.english, row.korean):
            findings["identical_probably_untranslated"].append(identity)
        english_placeholders = _placeholder_list(row.english)
        korean_placeholders = _placeholder_list(row.korean)
        if Counter(english_placeholders) != Counter(korean_placeholders):
            findings["format_placeholder_mismatch"].append(
                {
                    **identity,
                    "english_placeholders": english_placeholders,
                    "korean_placeholders": korean_placeholders,
                }
            )
    return findings


def _mask_comments(source: str) -> str:
    """Mask comments while retaining line/column positions and string contents."""
    output = list(source)
    index = 0
    state = "code"
    while index < len(source):
        character = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""
        if state == "code":
            if character == "/" and following == "/":
                output[index] = output[index + 1] = " "
                index += 2
                state = "line_comment"
            elif character == "/" and following == "*":
                output[index] = output[index + 1] = " "
                index += 2
                state = "block_comment"
            elif character == '"':
                index += 1
                state = "string"
            elif character == "'":
                index += 1
                state = "character"
            else:
                index += 1
        elif state == "line_comment":
            if character == "\n":
                state = "code"
            else:
                output[index] = " "
            index += 1
        elif state == "block_comment":
            if character == "*" and following == "/":
                output[index] = output[index + 1] = " "
                index += 2
                state = "code"
            else:
                if character != "\n":
                    output[index] = " "
                index += 1
        elif character == "\\":
            index += 2
        elif (state == "string" and character == '"') or (state == "character" and character == "'"):
            state = "code"
            index += 1
        else:
            index += 1
    return "".join(output)


def _literal_spans(source: str) -> list[tuple[int, int, str]]:
    masked = _mask_comments(source)
    text_spans: list[tuple[int, int, str]] = []
    quote_ranges: list[tuple[int, int]] = []
    for match in TEXT_LITERAL.finditer(masked):
        text_spans.append((match.start(), match.end(), _cpp_unescape(match.group(1))))
        quote_start = match.start(1) - 1
        quote_end = match.end(1) + 1
        quote_ranges.append((quote_start, quote_end))

    def nested_in_text_literal(position: int) -> bool:
        return any(start <= position < end for start, end in quote_ranges)

    all_spans = list(text_spans)
    for match in NORMAL_LITERAL.finditer(masked):
        if not nested_in_text_literal(match.start(1) - 1):
            all_spans.append((match.start(), match.end(), _cpp_unescape(match.group(1))))
    all_spans.sort(key=lambda item: item[0])
    return all_spans


def _statement_window(source: str, start: int, end: int) -> str:
    left = max(source.rfind(";", 0, start) + 1, start - 400)
    semicolon = source.find(";", end)
    right = min(len(source), end + 400) if semicolon < 0 else min(semicolon + 1, end + 400)
    return source[left:right]


def _is_sentence_like_english(value: str) -> bool:
    words = WORD.findall(value)
    if len(words) < 3 or len(value.strip()) < 12:
        return False
    return any(character.islower() for character in value)


def load_allowlist(path: Optional[Path]) -> tuple[list[dict[str, str]], list[dict[str, Any]]]:
    if path is None:
        return [], []
    try:
        loaded = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return [], [{"kind": "allowlist_read_error", "path": str(path), "detail": str(error)}]
    if not isinstance(loaded, dict) or not isinstance(loaded.get("entries", []), list):
        return [], [{"kind": "allowlist_invalid", "path": str(path), "detail": "expected an object with an entries array"}]
    entries: list[dict[str, str]] = []
    problems: list[dict[str, Any]] = []
    for index, entry in enumerate(loaded["entries"], start=1):
        if not isinstance(entry, dict) or not isinstance(entry.get("literal"), str):
            problems.append({"kind": "allowlist_invalid_entry", "entry": index, "detail": "literal string is required"})
            continue
        path_glob = entry.get("path", "*")
        if not isinstance(path_glob, str):
            problems.append({"kind": "allowlist_invalid_entry", "entry": index, "detail": "path must be a string"})
            continue
        entries.append({"path": path_glob, "literal": entry["literal"], "reason": str(entry.get("reason", ""))})
    return entries, problems


def _is_allowlisted(literal: SourceLiteral, entries: Iterable[dict[str, str]]) -> bool:
    pure_path = Path(literal.relative_path)
    for entry in entries:
        if entry["literal"] == literal.value and pure_path.match(entry["path"]):
            return True
    return False


def _source_files(source_root: Path) -> list[Path]:
    paths = list(source_root.rglob("*.cpp")) + list(source_root.rglob("*.h"))
    return sorted(path for path in paths if path.is_file())


def inspect_bypasses(source_root: Path, table_path: Path, allowlist: Iterable[dict[str, str]]) -> dict[str, list[dict[str, Any]]]:
    findings: dict[str, list[dict[str, Any]]] = {"bypassed_hangul_literals": [], "bypassed_english_hud_literals": []}
    resolved_table = table_path.resolve()
    for path in _source_files(source_root):
        if path.resolve() == resolved_table:
            continue
        try:
            source = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            # C++ project source is expected to be UTF-8.  A decoding issue is
            # recorded as a scan limitation rather than silently treating bytes
            # as user-facing text.
            findings.setdefault("source_read_errors", []).append({"path": str(path), "detail": "not valid UTF-8"})
            continue
        except OSError as error:
            findings.setdefault("source_read_errors", []).append({"path": str(path), "detail": str(error)})
            continue
        relative_path = path.relative_to(source_root).as_posix()
        for start, end, value in _literal_spans(source):
            if not value:
                continue
            literal = SourceLiteral(
                path=path,
                relative_path=relative_path,
                line=_line_number(source, start),
                value=value,
                context=_statement_window(source, start, end).strip().replace("\n", " "),
            )
            if _is_allowlisted(literal, allowlist):
                continue
            record = {"path": literal.relative_path, "line": literal.line, "literal": literal.value}
            if HANGUL.search(value):
                findings["bypassed_hangul_literals"].append(record)
                continue
            if not _is_sentence_like_english(value):
                continue
            window = literal.context
            if "ImperialLocalization::Translate" in window or "NSLOCTEXT" in window or "LOCTEXT" in window:
                continue
            if HUD_OR_NOTICE_CALL.search(window):
                findings["bypassed_english_hud_literals"].append(record)
    return findings


def _counts(findings: dict[str, list[dict[str, Any]]]) -> dict[str, int]:
    counts = {name: len(values) for name, values in findings.items()}
    counts["total_findings"] = sum(counts.values())
    return counts


def build_report(source_root: Path, table_path: Path, allowlist_path: Optional[Path]) -> dict[str, Any]:
    rows, parser_problems = parse_translation_table(table_path)
    table_findings = inspect_table(rows, parser_problems)
    allowlist, allowlist_problems = load_allowlist(allowlist_path)
    bypass_findings = inspect_bypasses(source_root, table_path, allowlist)
    findings = {**table_findings, **bypass_findings}
    if allowlist_problems:
        findings["allowlist_configuration"] = allowlist_problems
    counts = _counts(findings)
    return {
        "schema_version": SCHEMA_VERSION,
        "source_root": str(source_root),
        "localization_table": str(table_path),
        "allowlist": str(allowlist_path) if allowlist_path is not None else None,
        "translation_entry_count": len(rows),
        "counts": counts,
        "findings": findings,
        "scope": (
            "Static UTF-8 source inspection of FTranslation rows and direct literals; "
            "does not compile, run, render, or prove Unreal localization coverage."
        ),
    }


def _finding_label(kind: str, finding: dict[str, Any]) -> str:
    if "path" in finding:
        location = f"`{finding['path']}:{finding.get('line', '?')}`"
    else:
        location = f"테이블 {finding.get('entry', '?')}행(소스 {finding.get('line', '?')}행)"
    if kind == "format_placeholder_mismatch":
        return f"{location}: 형식 인수 불일치 — `{finding.get('english')}` / `{finding.get('korean')}`"
    if kind == "duplicate_english_keys":
        return f"{location}: 영어 키 중복 — `{finding.get('english')}`"
    if "literal" in finding:
        return f"{location}: `{finding['literal']}`"
    if "detail" in finding:
        return f"{location}: {finding['detail']}"
    return f"{location}: `{finding.get('english', '')}` / `{finding.get('korean', '')}`"


def markdown_summary(report: dict[str, Any]) -> str:
    lines = [
        "# 현지화 정적 검사 요약",
        "",
        f"- 번역 표: `{report['localization_table']}`",
        f"- 번역 행: **{report['translation_entry_count']}**",
        "- 범위: FTranslation 행과 직접 리터럴의 정적 검사만 수행했으며 Unreal 컴파일·실행·화면 렌더는 포함하지 않습니다.",
        "",
        "| 검사 항목 | 발견 수 |",
        "|---|---:|",
    ]
    names = {
        "missing_english": "영어 값 누락",
        "missing_korean": "한국어 값 누락",
        "empty_english": "빈 영어 값",
        "empty_korean": "빈 한국어 값",
        "identical_probably_untranslated": "동일 값(번역 누락 의심)",
        "format_placeholder_mismatch": "형식 인수 불일치",
        "duplicate_english_keys": "영어 키 중복",
        "malformed_table_entries": "형식이 잘못된 표 행",
        "bypassed_hangul_literals": "표 밖 한글 리터럴",
        "bypassed_english_hud_literals": "표를 우회한 HUD/알림 영어 문장",
        "source_read_errors": "소스 읽기 오류",
        "allowlist_configuration": "allowlist 설정 오류",
    }
    for key, title in names.items():
        lines.append(f"| {title} | {report['counts'].get(key, 0)} |")
    lines.extend(["", f"총 발견 수: **{report['counts']['total_findings']}**", "", "## 상위 발견 항목"])
    shown = 0
    for kind, records in report["findings"].items():
        for record in records:
            lines.append(f"- [{kind}] {_finding_label(kind, record)}")
            shown += 1
            if shown == 12:
                break
        if shown == 12:
            break
    if shown == 0:
        lines.append("- 발견 항목 없음")
    lines.extend(
        [
            "",
            "## 해석",
            "",
            "- `--strict`를 지정한 경우에만 위 발견 항목 수가 0이 아니면 종료 코드 1을 반환합니다.",
            "- allowlist는 로그 전용 등 의도적으로 표 밖에 둔 리터럴을 정확한 파일 glob·문자열·사유로 기록할 때만 사용합니다.",
            "- 추가 언어, 안정된 의미 키, Unreal 리소스 이관, 글꼴·RTL·IME 검수는 이 정적 검사만으로 구현되지 않습니다.",
            "",
        ]
    )
    return "\n".join(lines)


def _write_utf8(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def parse_arguments(arguments: Optional[list[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Check current Korean/English FTranslation coverage and direct string bypasses.")
    parser.add_argument("--source-root", type=Path, required=True, help="Unreal module source directory to scan")
    parser.add_argument("--table", type=Path, help="ImperialLocalization.cpp path (defaults under --source-root)")
    parser.add_argument("--json-report", type=Path, required=True, help="write the machine-readable JSON report here")
    parser.add_argument("--markdown-summary", type=Path, required=True, help="write the concise Markdown summary here")
    parser.add_argument("--allowlist", type=Path, help="optional JSON allowlist for intentional log-only literals")
    parser.add_argument("--strict", action="store_true", help="exit 1 when any finding is reported")
    return parser.parse_args(arguments)


def main(arguments: Optional[list[str]] = None) -> int:
    options = parse_arguments(arguments)
    source_root = options.source_root.resolve()
    table_path = (options.table if options.table is not None else source_root / "ImperialLocalization.cpp").resolve()
    report = build_report(source_root, table_path, options.allowlist.resolve() if options.allowlist is not None else None)
    _write_utf8(options.json_report, json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n")
    _write_utf8(options.markdown_summary, markdown_summary(report))
    print(
        f"{report['translation_entry_count']} translation entries, "
        f"{report['counts']['total_findings']} findings"
    )
    return 1 if options.strict and report["counts"]["total_findings"] != 0 else 0


if __name__ == "__main__":
    raise SystemExit(main())
