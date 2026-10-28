"""Compare the product's read-only AI result model with the CLI on fresh synthetic files.
--fixtures-only never starts Unreal. Runtime runs require an explicit packaged
executable or --editor and write only a new QA directory. No OS/USB input is sent.
"""
import argparse
import copy
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
SCOPE = ("Synthetic parser/eligibility/delta and file-preservation QA only. "
         "test_session=false fixtures are fabricated to exercise the eligible branch; "
         "they are not human records, physical-controller, other-PC, flight or UI evidence.")
SCRIPT = ROOT / "Scripts/compare-ai-results.py"

def require(value, message):
    if not value:
        raise ValueError(message)

def save(path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def imported():
    spec = importlib.util.spec_from_file_location("imperial_comparison_qa_cli", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def fixture(session=1, schema=2, mode="defence", count=3):
    first = 5 if mode == "duel" else 6
    result = dict(schema=schema, session_id=f"{session:032x}", started_utc="2026-01-01T00:00:00.000Z",
                  ended_utc="2026-01-01T00:03:10.000Z", stop_reason="time_limit",
                  completion_reason="landed", source_sha256="ab" * 32,
                  build_version="SYNTHETIC_UI_QA_ONLY", test_session=False, difficulty=1,
                  seed=17, goals=1, practice_seconds=180.0, landing_seconds=10.0,
                  recovery_attempts=2,
                  aircraft=[dict(id=i, guard_impacts=3 if i == 0 else 0,
                                 guard_remaining_percent=95.0 if i == 0 else 100.0)
                            for i in [0, *range(first, first + count)]])
    if schema == 1:
        result["defenders"] = count
    else:
        result.update(opponents=count, session_mode=mode, opponent_goals=0)
    return result

def encoded(value):
    return value if isinstance(value, bytes) else json.dumps(value, ensure_ascii=True, allow_nan=True).encode("utf-8")

def corpus(folder):
    """Each expected result is calculated from the unchanged CLI, never a game result."""
    module = imported()
    cases = []
    def add(name, a=None, b=None, note=""):
        a = fixture(1) if a is None else a
        b = fixture(2) if b is None else b
        paths = [folder / (name + "-A.json"), folder / (name + "-B.json")]
        for path, data in zip(paths, (a, b)):
            path.write_bytes(encoded(data))
        row = dict(id=name, reference=paths[0].name, candidate=paths[1].name, note=note)
        records = []
        for label, path in zip(("reference", "candidate"), paths):
            try:
                value = module.read_result(path)
                module.summarize(value)
                records.append(value)
                row["expected_valid_" + label] = True
            except (OSError, ValueError, RecursionError):
                records.append(None)
                row["expected_valid_" + label] = False
        if all(row["expected_valid_" + x] for x in ("reference", "candidate")):
            result = module.compare_results(*records)
            row.update(expected_exit=0 if result["eligible_for_deltas"] else 2, expected=result)
        else:
            row["expected_exit"] = 1
        cases.append(row)

    add("eligible-defence")
    add("eligible-schema1", fixture(1, schema=1), fixture(2, schema=1))
    add("eligible-duel-four", fixture(1, mode="duel", count=4), fixture(2, mode="duel", count=4))
    b = fixture(2); b.update(goals=4, opponent_goals=2, practice_seconds=181, landing_seconds=8.5, recovery_attempts=1)
    b["aircraft"][0].update(guard_impacts=1, guard_remaining_percent=98.0)
    add("signed-deltas", b=b)
    b = fixture(2); b["aircraft"].reverse(); add("roster-order-independent", b=b)
    for side in ("reference", "candidate", "both"):
        a, b = fixture(1), fixture(2)
        if side in ("reference", "both"): del a["recovery_attempts"]
        if side in ("candidate", "both"): del b["recovery_attempts"]
        add("optional-recovery-" + side, a, b)
    b = fixture(0xabc); b["session_id"] = b["session_id"].upper(); b["source_sha256"] = b["source_sha256"].upper()
    add("hex-case-normalization", b=b)
    a, b = fixture(1), fixture(2); a["started_utc"] = "2026-01-02T00:00:00.000Z"; a["ended_utc"] = "2026-01-02T00:03:10.000Z"
    add("B-earlier-than-A-allowed", a, b)
    for suffix in ("+00:00", "-00:00", "+0000", "-0000", "+00", "-00"):
        b = fixture(2)
        b["started_utc"] = "2026-01-01T00:00:00.000" + suffix
        b["ended_utc"] = "2026-01-01T00:03:10.000" + suffix
        add("utc-zero-offset-" + suffix.replace(":", "c"), b=b)
    for duration in (179.0, 181.0):
        b = fixture(2); b["practice_seconds"] = duration; add("duration-boundary-" + str(int(duration)), b=b)
    b = b"\xef\xbb\xbf" + encoded(fixture(2)); add("utf8-BOM", b=b)
    a = encoded(fixture(2)); add("exact-65536-bytes", b=a + b" " * (65536 - len(a)))
    a = fixture(1); b = fixture(2); a["build_version"] = b["build_version"] = "\U0001f681" * 32; add("UTF16-limit-64-units", a, b)
    b = fixture(2); b["build_version"] = "\U0001f681" * 33; add("UTF16-over-limit", b=b)
    for field, value in (("source_sha256", "cd" * 32), ("build_version", "SYNTHETIC_DIFFERENT"),
                         ("difficulty", 2), ("seed", 18)):
        b = fixture(2); b[field] = value; add("condition-" + field, b=b)
    add("condition-schema", b=fixture(2, schema=1))
    add("condition-mode", b=fixture(2, mode="duel"))
    add("condition-count", b=fixture(2, count=4))
    add("same-session", b=fixture(1))
    b = fixture(0xabc); a = fixture(0xabc); b["session_id"] = b["session_id"].upper(); add("same-session-case", a, b)
    for side in ("reference", "candidate"):
        a, b = fixture(1), fixture(2); (a if side == "reference" else b)["test_session"] = True
        add("automated-" + side, a, b)
    for reason in sorted(module.STOP_REASONS - {"time_limit"}):
        b = fixture(2); b["stop_reason"] = reason; add("stop-" + reason, b=b)
    for reason in sorted(module.COMPLETION_REASONS - {"landed"}):
        b = fixture(2); b["completion_reason"] = reason; add("completion-" + reason, b=b)
    for value in (178.999, 181.001):
        b = fixture(2); b["practice_seconds"] = value; add("duration-outside-" + str(value), b=b)
    for field, value in (("goals", True), ("opponents", 2), ("difficulty", 1.5), ("seed", 0),
                         ("practice_seconds", -1), ("landing_seconds", float("nan")),
                         ("goals", float("inf")), ("build_version", "   "), ("build_version", "\ud800"),
                         ("session_id", "g" * 32), ("source_sha256", "a" * 63),
                         ("started_utc", "2026-01-01T00:00:00"), ("ended_utc", "2026-01-01T00:03:10+01:00"),
                         ("ended_utc", "2025-12-31T23:59:59Z")):
        b = fixture(2); b[field] = value
        add("invalid-" + field + "-" + str(len(cases)), b=b)
    b = fixture(2); del b["test_session"]; add("missing-required", b=b)
    b = fixture(2); b["aircraft"][-1] = copy.deepcopy(b["aircraft"][0]); add("duplicate-aircraft", b=b)
    b = fixture(2); b["aircraft"][0]["id"] = True; add("boolean-aircraft-id", b=b)
    b = fixture(2); b["aircraft"][0]["guard_remaining_percent"] = 101; add("invalid-guard-range", b=b)
    text = encoded(fixture(2))
    add("duplicate-top-key", b=text.replace(b'"goals": 1', b'"goals": 1, "goals": 9'))
    add("duplicate-nested-key", b=text.replace(b'"id": 0', b'"id": 0, "id": 1'))
    add("malformed-JSON", b=b"{broken synthetic JSON")
    add("invalid-UTF8", b=b"\xff\xfeinvalid synthetic input")
    add("UTF16-BOM-rejected", b=json.dumps(fixture(2)).encode("utf-16"))
    add("oversize-65537", b=text + b" " * (65537 - len(text)))
    add("F6-JSONL-not-result", b=b'{"event":"start","aircraft":[]}\n{"event":"stop","aircraft":[]}\n')
    add("top-level-array", b=b"[]")
    require(len({row["id"] for row in cases}) == len(cases), "Fixture case IDs must be unique")
    require(len({row[key] for row in cases for key in ("reference", "candidate")}) == len(cases) * 2, "Fixture filenames must be unique")
    manifest = dict(schema=1, scope=SCOPE, records_are_synthetic=True,
                    expected_from="Scripts/compare-ai-results.py", CLI_sha256=digest(SCRIPT),
                    native_extra_constraints_not_exercised=["depth over 64", "unknown-field malformed surrogate", "noncanonical Python-only ISO representations"],
                    cases=cases)
    save(folder / "manifest.json", manifest)
    save(folder / "SYNTHETIC-ONLY.json", dict(scope=SCOPE, test_session_false_is_not_human_evidence=True))
    return manifest

def identity():
    shell = shutil.which("powershell") or shutil.which("pwsh")
    require(shell, "PowerShell is required for source identity")
    script = str(ROOT / "Scripts/Get-SourceIdentity.ps1").replace("'", "''")
    command = "[Console]::OutputEncoding=[Text.Encoding]::UTF8; & '" + script + "' | ConvertTo-Json -Compress"
    return json.loads(subprocess.check_output([shell, "-NoProfile", "-Command", command], cwd=ROOT, encoding="utf-8-sig", timeout=60, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0)))

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures-only", action="store_true")
    parser.add_argument("--runtime-path", type=Path, help="Exact packaged Shipping executable")
    parser.add_argument("--editor", action="store_true")
    parser.add_argument("--engine-root", type=Path, default=Path("E:/Program Files/UE_5.8"))
    parser.add_argument("--fps", type=int, choices=(20, 60, 240), default=60)
    args = parser.parse_args(argv)
    out = (ROOT / "Saved/Verification/ProductUI-QA" / ("comparison-" + datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8])).resolve()
    out.mkdir(parents=True, exist_ok=False)
    folder = out / "corpus"; folder.mkdir()
    info = dict(scope=SCOPE, output=str(out), native_executed=False)
    print(out, flush=True)
    try:
        manifest = corpus(folder)
        before = {p.name: digest(p) for p in folder.iterdir() if p.is_file()}
        info["fixture_cases"] = len(manifest["cases"])
        info["expected_exit_counts"] = {str(code): sum(x["expected_exit"] == code for x in manifest["cases"]) for code in (0, 1, 2)}
        if args.fixtures_only:
            save(out / "summary.json", dict(ok=True, **info, fixture_files=before))
            print("PASS fresh synthetic corpus only;", len(manifest["cases"]), "cases; Unreal not started", flush=True)
            return 0
        require(args.editor != bool(args.runtime_path), "Choose exactly one of --editor or --runtime-path")
        expected = identity()
        exe = (args.engine_root / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe") if args.editor else args.runtime_path
        exe = exe.resolve()
        require(exe.is_file(), "Runtime executable missing: " + str(exe))
        exe_before = digest(exe)
        user = out / "user"; user.mkdir()
        (user / ".ai-comparison-qa").write_bytes(b"imperial-ai-comparison-qa-v1\n")
        (user / "keep.txt").write_bytes(b"synthetic comparison QA sentinel\n")
        prefix = [str(exe)] + ([str(ROOT / "ImperialWingsSimulator.uproject")] if args.editor else [])
        command = prefix + ["-game", "-nullrhi", "-unattended", "-nosound", "-nosplash", "-ImperialNoWelcome",
                            "-ImperialBridgeUrl=http://127.0.0.1:1", "-ImperialAIComparisonSmoke",
                            "-ImperialAIComparisonManifest=" + str(folder / "manifest.json"),
                            "-ImperialAIComparisonResult=" + str(out / "result.json"),
                            "-ImperialDataDir=" + str(user), "-UserDir=" + str(out / "engine-user"),
                            "-abslog=" + str(out / "runtime.log"), "-UseFixedTimeStep", "-FPS=" + str(args.fps)]
        info.update(source_before=expected, executable_sha256_before=exe_before, command=command)
        save(out / "invocation.json", info)
        with (out / "console.log").open("w", encoding="utf-8") as log:
            process = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=90, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        info.update(native_executed=True, actual_process_exit_code=process.returncode)
        require((out / "result.json").is_file(), "Native verifier did not produce a new result")
        result = json.loads((out / "result.json").read_text(encoding="utf-8-sig"))
        require(process.returncode == 0 and result.get("ok") is True, "Native verifier failed or did not exit normally")
        require(len(result.get("cases", [])) == len(manifest["cases"]) and all(x.get("ok") is True for x in result["cases"]), "Not every fixture case agreed")
        require([x["id"] for x in result["cases"]] == [x["id"] for x in manifest["cases"]], "Native fixture case IDs/order differ")
        require(len(result.get("checks", [])) == len(manifest["cases"]) + 6 and all(x.get("ok") is True for x in result["checks"]), "Expected native guard/case/safety checks did not all run")
        require(result["build"]["source_sha256"] == expected["source_sha256"] and result["build"]["version"] == expected["version"], "Runtime/source identity mismatch")
        require(Path(result["data_directory"]).resolve() == user, "Unexpected native data directory")
        require(abs(result["min_game_dt_s"] - 1 / args.fps) < 1e-6 and abs(result["max_game_dt_s"] - 1 / args.fps) < 1e-6, "Actual Tick interval mismatch")
        require(result["settle_min_simulation_s"] == .5 and result["settle_max_simulation_s"] == 2 and .5 <= result["comparison_ready_simulation_s"] <= 2 and result["consecutive_safe_ticks"] >= 2, "Fixture settling did not meet its bounded unchanged-safety gate")
        require([x["phase"] for x in result["safety_states"]] == ["initial_state", "before_comparison", "after_comparison"], "Expected safety diagnostic phases missing")
        require(all(x["wait_invariant"] is True for x in result["safety_states"]), "Waiting manual/disarmed/no-score invariant failed")
        require(before == {p.name: digest(p) for p in folder.iterdir() if p.is_file()}, "Fixture originals were modified")
        require((user / "keep.txt").read_bytes() == b"synthetic comparison QA sentinel\n", "Unrelated sentinel modified")
        require(not list(user.rglob("RadioProfile.json")) and not (user / "TrainingResults").exists(), "Verifier wrote calibration or training data")
        require(identity()["source_sha256"] == expected["source_sha256"] and digest(exe) == exe_before, "Source or executable changed")
        save(out / "summary.json", dict(ok=True, **info, native_checks=result.get("checks"), fixture_files=before, originals_preserved=True, result=str(out / "result.json")))
        print("PASS", len(manifest["cases"]), "native/CLI fixture agreements; originals preserved", flush=True)
        return 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        save(out / "summary.json", dict(ok=False, **info, error=str(error)))
        print("FAIL:", error, "; evidence:", out, file=sys.stderr, flush=True)
        return 1

if __name__ == "__main__":
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")
    sys.exit(main())
