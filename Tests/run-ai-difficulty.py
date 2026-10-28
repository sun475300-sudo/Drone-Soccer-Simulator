"""Measure six seeded AI duels using actual Chaos telemetry, without physical input.

Native pass/fail gates cover correctness. Baseline deltas describe performance;
this runner does not turn a faster speed or a higher score into automatic approval.
"""
import argparse
import json
import math
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import uuid
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {(opponents, difficulty, 1) for opponents in (3, 4) for difficulty in range(3)}
PHASE_NAMES = ("lift", "approach", "align", "cross", "clear", "return")
SPEED_FIELDS = ("active_speed", "active_horizontal_speed", "travel_speed", "travel_horizontal_speed")
SCOPE = ("Six fixed-seed local AI duels; actual synchronous Chaos positions and velocities; "
         "no physical controller, native keyboard/mouse input, human difficulty rating, "
         "network play, or physical-aircraft dynamics validation. AI opponents include one striker.")


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write_json(path, value):
    Path(path).write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def percentile(values, fraction):
    ordered = sorted(values)
    if not ordered:
        return None
    index = (len(ordered) - 1) * fraction
    lo, hi = math.floor(index), math.ceil(index)
    return ordered[lo] + (ordered[hi] - ordered[lo]) * (index - lo)


def sampled_stats(values):
    return dict(count=len(values), mean_mps=statistics.fmean(values) if values else None,
                peak_mps=max(values) if values else None,
                p50_mps=percentile(values, .5), p95_mps=percentile(values, .95))


def case_key(metric):
    return tuple(metric.get(field) for field in ("opponents", "difficulty", "seed"))


def validate_speed(stats, name):
    require(isinstance(stats, dict), name + " must be a speed-statistics object")
    require(isinstance(stats.get("count"), int) and stats["count"] > 0, name + " lacks samples")
    for field in ("mean_mps", "peak_mps", "p50_mps", "p95_mps"):
        require(number(stats.get(field)) and stats[field] >= 0, name + "." + field + " is invalid")
    require(stats["p50_mps"] <= stats["p95_mps"] + 1e-6 <= stats["peak_mps"] + 2e-6,
            name + " percentiles exceed the observed peak")
    require(stats["mean_mps"] <= stats["peak_mps"] + 1e-6, name + " mean exceeds observed peak")


def validate_result(result, fps=None, expected_sha=None):
    require(result.get("ok") is True, "Native duel fixture failed")
    checks = result.get("checks")
    require(isinstance(checks, list) and checks and all(row.get("ok") is True for row in checks),
            "Native duel checks are absent or failed")
    build = result.get("build", {})
    source_sha = build.get("source_sha256", "")
    require(isinstance(source_sha, str) and len(source_sha) == 64, "Missing native source fingerprint")
    if expected_sha:
        require(source_sha == expected_sha, "Runtime/source fingerprint mismatch; rebuild before comparison")
    minimum, maximum = result.get("min_game_dt_s"), result.get("max_game_dt_s")
    require(number(minimum) and number(maximum) and minimum > 0, "Missing positive fixture Tick intervals")
    inferred_fps = round(1. / minimum)
    fps = fps or inferred_fps
    require(fps in (20, 60, 240), "Unsupported actual fixture FPS")
    require(abs(minimum - 1. / fps) < 1e-6 and abs(maximum - 1. / fps) < 1e-6,
            "Actual fixture Tick intervals differ from requested FPS")
    metrics, rows = result.get("performance_metrics"), result.get("performance_rows")
    require(isinstance(metrics, list) and len(metrics) == 6, "Expected six native performance metrics")
    require(isinstance(rows, list) and rows, "Expected native raw performance rows")
    require({case_key(m) for m in metrics} == EXPECTED, "Missing, duplicate, or unexpected seeded AI cases")
    require({m.get("case") for m in metrics} == set(range(6)), "Case IDs must be unique 0..5")
    grouped = {index: [] for index in range(6)}
    for row in rows:
        require(isinstance(row, dict) and row.get("case") in grouped, "Raw row has an unknown case")
        grouped[row["case"]].append(row)
    analyzed = []
    for metric in sorted(metrics, key=lambda item: item["case"]):
        case = metric["case"]
        require(case == (metric["opponents"] - 3) * 3 + metric["difficulty"], "Case ID/settings mismatch")
        require(number(metric.get("active_seconds")) and 179 <= metric["active_seconds"] <= 181,
                "Case " + str(case) + " did not complete a three-minute active session")
        for field in SPEED_FIELDS:
            validate_speed(metric.get(field), "case " + str(case) + "." + field)
        require(metric["active_speed"]["count"] >= 1000, "Insufficient native active speed samples")
        for field in ("red_goals", "attack_cycles", "recovery_attempts", "striker_guard_impacts_at_stop", "roster_guard_impacts_at_stop"):
            require(isinstance(metric.get(field), int) and metric[field] >= 0, "Invalid count: " + field)
        require(metric["red_goals"] >= 2 and metric["attack_cycles"] >= 1, "Duel failed real goal/return prerequisites")
        require(number(metric.get("landing_seconds")) and 0 <= metric["landing_seconds"] < 35, "Invalid landing duration")
        require(number(metric.get("striker_guard_remaining_pct_at_stop")) and
                0 <= metric["striker_guard_remaining_pct_at_stop"] <= 100, "Invalid stop-time guard integrity")
        require(metric.get("stop_reason") == "time_limit" and metric.get("completion_reason") == "landed",
                "AI session stopped early or did not land")
        goals, cycles = metric.get("goal_times_s"), metric.get("cycle_times_s")
        for times, label in ((goals, "goal"), (cycles, "cycle")):
            require(isinstance(times, list) and all(number(t) and 0 <= t <= metric["active_seconds"] + .1 for t in times),
                    "Invalid " + label + " event times")
            require(all(b > a for a, b in zip(times, times[1:])), label + " event times are not strictly increasing")
        require(len(goals) == metric["red_goals"] and len(cycles) == metric["attack_cycles"], "Event counts disagree with native totals")
        require(number(metric.get("first_goal_s")) and abs(metric["first_goal_s"] - goals[0]) < 1e-4,
                "First-goal time disagrees with recorded events")
        intervals = [b - a for a, b in zip(goals, goals[1:])]
        require(number(metric.get("goal_interval_median_s")) and
                abs(metric["goal_interval_median_s"] - statistics.median(intervals)) < 1e-3,
                "Goal interval median disagrees with event times")
        require(number(metric.get("goal_to_retreat_median_s")) and metric["goal_to_retreat_median_s"] >= 0,
                "Missing goal-to-retreat duration")
        phases = metric.get("phase_metrics")
        require(isinstance(phases, list) and {p.get("phase") for p in phases} == set(range(6)),
                "Expected metrics for all six attack phases")
        for phase in phases:
            require(number(phase.get("duration_s")) and phase["duration_s"] >= 0, "Invalid phase duration")
            if phase["duration_s"] > 0:
                validate_speed(phase.get("speed"), "phase speed")
                validate_speed(phase.get("horizontal_speed"), "phase horizontal speed")
        case_rows = grouped[case]
        require(len(case_rows) >= 1000, "Case " + str(case) + " has fewer than 1000 raw telemetry samples")
        previous_time, previous_red, previous_cycle, previous_impact = -1., 0, 0, 0
        active, travel = [], []
        max_gap = 0.
        for row in case_rows:
            time = row.get("time")
            require(number(time) and time > previous_time, "Raw sample times are not finite and strictly increasing")
            if previous_time >= 0:
                max_gap = max(max_gap, time - previous_time)
            previous_time = time
            require(row.get("phase") in range(6) and isinstance(row.get("landing"), bool), "Invalid raw phase or landing flag")
            for vector in ("position_m", "velocity_mps", "target_m"):
                require(isinstance(row.get(vector), list) and len(row[vector]) == 3 and all(number(v) for v in row[vector]),
                        "Invalid raw vector: " + vector)
            speed = row.get("speed_mps")
            require(number(speed) and 0 <= speed < 12, "Invalid or unsafe raw speed")
            require(abs(math.sqrt(sum(v * v for v in row["velocity_mps"])) - speed) < 1e-3,
                    "Raw speed does not equal actual recorded velocity magnitude")
            for field, previous in (("red", previous_red), ("cycles", previous_cycle), ("guard_impacts", previous_impact)):
                require(isinstance(row.get(field), int) and row[field] >= previous, "Raw cumulative count regressed: " + field)
            previous_red, previous_cycle, previous_impact = row["red"], row["cycles"], row["guard_impacts"]
            require(number(row.get("guard_remaining_pct")) and 0 <= row["guard_remaining_pct"] <= 100,
                    "Invalid raw guard integrity")
            if not row["landing"]:
                active.append(speed)
                if row["phase"] in (1, 5):
                    travel.append(speed)
        require(previous_red == metric["red_goals"], "Raw final score and native metric disagree")
        analyzed.append(dict(metric, raw_sample_validation=dict(
            count=len(case_rows), max_sample_gap_s=max_gap,
            sampled_active_speed=sampled_stats(active), sampled_travel_speed=sampled_stats(travel)),
            derived_goal_cycle_median_s=statistics.median([b-a for a,b in zip(cycles,cycles[1:])]) if len(cycles)>1 else None))
    return dict(schema=1, ok=True, build=build, fps=fps, native_checks=len(checks), cases=analyzed, scope=SCOPE,
                measurement_notes=["Native speed metrics use actual body velocity, not displacement-derived estimates.",
                                   "Raw-row peak is only the peak among recorded telemetry samples.",
                                   "Guard impacts include floor, goal and other contacts; they are not aircraft-only collisions.",
                                   "Stop-time damage excludes later landing impacts; no human opponent flies in this benchmark."])


def baseline_cases(document):
    for field in ("performance_metrics", "cases"):
        value = document.get(field)
        if isinstance(value, list):
            return value
    raise ValueError("Baseline must contain performance_metrics or cases with explicit opponents/difficulty/seed")


def compare(summary, baseline):
    old = baseline_cases(baseline)
    keyed = {}
    for metric in old:
        key = case_key(metric)
        require(all(value is not None for value in key), "Baseline case lacks explicit matching conditions")
        require(key not in keyed, "Baseline contains duplicate case conditions")
        keyed[key] = metric
    scalar_fields = ("first_goal_s", "goal_interval_median_s", "goal_to_retreat_median_s", "red_goals", "attack_cycles",
                     "landing_seconds", "recovery_attempts", "striker_guard_impacts_at_stop",
                     "striker_guard_remaining_pct_at_stop", "roster_guard_impacts_at_stop")
    comparisons = []
    for new in summary["cases"]:
        key = case_key(new)
        previous = keyed.get(key)
        if previous is None:
            comparisons.append(dict(opponents=key[0], difficulty=key[1], seed=key[2], matched=False))
            continue
        deltas, unavailable = {}, []
        for field in scalar_fields:
            a, b = previous.get(field), new.get(field)
            if number(a) and number(b):
                deltas[field] = dict(baseline=a, current=b, delta=b-a, percent_delta=100*(b-a)/a if a else None)
            else:
                unavailable.append(field)
        for group in SPEED_FIELDS:
            for field in ("mean_mps", "peak_mps", "p50_mps", "p95_mps"):
                name = group + "." + field
                a, b = (previous.get(group) or {}).get(field), (new.get(group) or {}).get(field)
                if number(a) and number(b):
                    deltas[name] = dict(baseline=a, current=b, delta=b-a, percent_delta=100*(b-a)/a if a else None)
                else:
                    unavailable.append(name)
        comparisons.append(dict(opponents=key[0], difficulty=key[1], seed=key[2], matched=True,
                                deltas=deltas, unavailable_fields=unavailable))
    baseline_fps = baseline.get("fps")
    if baseline_fps is None and number(baseline.get("min_game_dt_s")) and baseline["min_game_dt_s"] > 0:
        baseline_fps = round(1./baseline["min_game_dt_s"])
    return dict(delta_definition="current minus baseline; comparison does not set pass/fail",
                baseline_build=baseline.get("build"), baseline_fps=baseline_fps,
                same_fps=baseline_fps == summary["fps"] if baseline_fps is not None else None,
                cases=comparisons,
                cautions=["Compare paired conditions at the same FPS; this is one seeded scenario per condition.",
                          "Unavailable baseline fields remain unavailable rather than being inferred as actual velocity."])


def source_identity():
    shell = shutil.which("powershell") or shutil.which("pwsh")
    require(shell is not None, "PowerShell is required to inspect the source fingerprint")
    script = ROOT / "Scripts/Get-SourceIdentity.ps1"
    # Escape this repository's known script path as a literal PowerShell string.
    literal = str(script).replace("'", "''")
    command = [shell, "-NoProfile", "-Command", "[Console]::OutputEncoding=[Text.Encoding]::UTF8; & '" + literal + "' | ConvertTo-Json -Compress"]
    return json.loads(subprocess.check_output(command, cwd=ROOT, text=True, encoding="utf-8-sig",
                                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0), timeout=60))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root", type=Path, default=Path("E:/Program Files/UE_5.8"))
    parser.add_argument("--packaged", action="store_true")
    parser.add_argument("--package-root", type=Path, default=ROOT / "dist/Portable/Windows")
    parser.add_argument("--fps", type=int, choices=(20, 60, 240), default=60)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--analyze-only", type=Path, metavar="RESULT_JSON")
    args = parser.parse_args()
    suffix = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8]
    output = ROOT / "Saved/Verification" / ("ai-difficulty-" + suffix)
    output.mkdir(parents=True)
    metadata = dict(scope=SCOPE, analysis_only=bool(args.analyze_only))
    print(output, flush=True)
    try:
        identity = None
        if args.analyze_only:
            result_file = args.analyze_only.resolve()
            result = read_json(result_file)
            summary = validate_result(result)
        else:
            identity = source_identity()
            result_file = output / "result.json"
            userdata = (output / "user").resolve()
            if args.packaged:
                executable = args.package_root.resolve() / "ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe"
                prefix = [str(executable)]
            else:
                executable = args.engine_root.resolve() / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
                prefix = [str(executable), str(ROOT / "ImperialWingsSimulator.uproject")]
            require(executable.is_file(), "Runtime executable not found: " + str(executable))
            command = prefix + ["-game", "-nullrhi", "-nosound", "-unattended", "-nosplash", "-ImperialNoWelcome",
                                "-ImperialAIDuelSmoke", "-ImperialAIDuelCases=6", "-UseFixedTimeStep", "-FPS=" + str(args.fps),
                                "-ImperialBridgeUrl=http://127.0.0.1:1", "-ImperialDataDir=" + str(userdata),
                                "-ImperialAIDuelResult=" + str(result_file), "-abslog=" + str(output / "runtime.log")]
            metadata.update(command=command, user_data=str(userdata), source_identity=identity)
            write_json(output / "invocation.json", metadata)
            with (output / "console.log").open("w", encoding="utf-8") as log:
                run = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=600,
                                     creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            require(result_file.is_file(), "Native result was not written; inspect console.log and runtime.log")
            result = read_json(result_file)
            require(run.returncode == 0, "Native runtime exited with code " + str(run.returncode))
            summary = validate_result(result, args.fps, identity["source_sha256"])
        summary.update(result_path=str(result_file), metadata=metadata)
        if args.baseline:
            summary["comparison"] = compare(summary, read_json(args.baseline))
            summary["comparison"]["baseline_path"] = str(args.baseline.resolve())
        write_json(output / "analysis.json", summary)
        print("PASS", summary["native_checks"], "native checks; 6 cases; actual velocity and event telemetry validated", flush=True)
        for metric in summary["cases"]:
            print("AI={opponents} difficulty={difficulty} seed={seed}: first_goal={first_goal_s:.2f}s "
                  "goals={red_goals} travel_p50={travel:.3f}m/s guard_contacts={striker_guard_impacts_at_stop}".format(
                      **metric, travel=metric["travel_speed"]["p50_mps"]), flush=True)
        print(output / "analysis.json", flush=True)
        return 0
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        write_json(output / "analysis.json", dict(ok=False, error=str(error), metadata=metadata, scope=SCOPE))
        print("FAIL:", error, "; evidence:", output, file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
