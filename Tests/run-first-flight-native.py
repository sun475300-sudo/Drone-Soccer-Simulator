"""Verify first-flight tutorial safety and native keyboard takeoff in isolated data.

This fixture does not verify the complete six-task flight/goal tutorial, physical
keyboard input, USB hardware, or a person's ability to follow the instructions.
"""
import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
SCOPE = ("Native first-flight tutorial safety, explicit UI/controller APIs and simulated engine keyboard input; "
         "no OS input, physical controller, personal user settings, or complete six-task/goal runtime verification.")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def save_json(path, value):
    Path(path).write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def source_identity():
    shell = shutil.which("powershell") or shutil.which("pwsh")
    require(shell is not None, "PowerShell is required to inspect source identity")
    script = str(ROOT / "Scripts/Get-SourceIdentity.ps1").replace("'", "''")
    code = "[Console]::OutputEncoding=[Text.Encoding]::UTF8; & '" + script + "' | ConvertTo-Json -Compress"
    return json.loads(subprocess.check_output(
        [shell, "-NoProfile", "-Command", code], cwd=ROOT, text=True,
        encoding="utf-8-sig", timeout=60, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0)))


def validate(result, identity, fps):
    require(result.get("ok") is True, "Native first-flight fixture reported failure")
    checks = result.get("checks")
    require(isinstance(checks, list) and checks, "Native first-flight checks are absent")
    failed = [row for row in checks if not isinstance(row, dict) or row.get("ok") is not True]
    require(not failed, "Native first-flight checks failed: " + json.dumps(failed, ensure_ascii=False))
    build = result.get("build")
    require(isinstance(build, dict) and build.get("source_sha256") == identity["source_sha256"],
            "Native build/source fingerprint mismatch; rebuild the current source")
    require(isinstance(result.get("scope"), str) and result["scope"].strip(), "Native scope is absent")
    require(result.get("full_goal_runtime_verified") is False,
            "Fixture must explicitly mark complete goal-runtime verification as unverified")
    probe = result.get("score_probe")
    require(isinstance(probe, dict) and probe.get("hover_stabilized") is True,
            "Independent score probe did not establish a stable real-motor hover")
    require(isinstance(probe.get("launch_age_s"), (int, float)) and math.isfinite(probe["launch_age_s"])
            and 2.5 <= probe["launch_age_s"] <= 8.1, "Invalid independent probe launch age")
    require(probe.get("observed_score") == 1, "Independent probe did not observe one real goal")
    rows = probe.get("samples")
    require(isinstance(rows, list) and len(rows) >= 25, "Independent probe raw observations are missing")
    last_age = -1.
    for row in rows:
        age = row.get("probe_age_s")
        require(isinstance(age, (int, float)) and math.isfinite(age) and age >= last_age,
                "Invalid independent probe observation time")
        last_age = age
        for name in ("position_cm", "velocity_mps"):
            value = row.get(name)
            require(isinstance(value, list) and len(value) == 3
                    and all(isinstance(v, (int, float)) and math.isfinite(v) for v in value),
                    "Invalid independent probe " + name)
        for name in ("speed_mps", "tilt_deg", "heading_error_deg", "total_thrust_n"):
            value = row.get(name)
            require(isinstance(value, (int, float)) and math.isfinite(value) and value >= 0,
                    "Invalid independent probe " + name)
        vector_speed = math.sqrt(sum(v*v for v in row["velocity_mps"]))
        require(abs(vector_speed-row["speed_mps"]) < 1e-4, "Independent probe speed/vector mismatch")
        require(row.get("phase") in ("hover", "crossing") and row.get("score") in (0, 1),
                "Invalid independent probe phase or score")
    require(any(row["phase"] == "hover" for row in rows)
            and any(row["phase"] == "crossing" and row["score"] == 1 for row in rows),
            "Independent probe must contain observed hover and a new crossing goal")
    tick_verified = False
    for minimum_name, maximum_name in (("min_game_dt_s", "max_game_dt_s"),
                                        ("actual_min_game_dt_s", "actual_max_game_dt_s")):
        if minimum_name in result or maximum_name in result:
            minimum, maximum = result.get(minimum_name), result.get(maximum_name)
            require(isinstance(minimum, (int, float)) and isinstance(maximum, (int, float))
                    and math.isfinite(minimum) and math.isfinite(maximum), "Invalid native Tick interval evidence")
            require(abs(minimum-1./fps) < 1e-6 and abs(maximum-1./fps) < 1e-6,
                    "Native actual Tick intervals differ from requested FPS")
            tick_verified = True
    return dict(ok=True, native_checks=len(checks), build=build,
                native_scope=result["scope"], full_goal_runtime_verified=False,
                requested_fps=fps, actual_tick_interval_verified=tick_verified,
                score_probe_rows=len(rows), score_probe_launch_age_s=probe["launch_age_s"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root", type=Path, default=Path("E:/Program Files/UE_5.8"))
    parser.add_argument("--packaged", action="store_true")
    parser.add_argument("--package-root", type=Path, default=ROOT / "dist/Portable/Windows")
    parser.add_argument("--render-capture", action="store_true")
    parser.add_argument("--width", type=int, default=1600)
    parser.add_argument("--height", type=int, default=1000)
    parser.add_argument("--fps", type=int, choices=(20, 60, 240), default=60)
    args = parser.parse_args()
    if not 640 <= args.width <= 3840 or not 480 <= args.height <= 2160:
        parser.error("Capture dimensions must be width 640..3840 and height 480..2160")
    suffix = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8]
    output = (ROOT / "Saved/Verification" / ("first-flight-native-" + suffix)).resolve()
    output.mkdir(parents=True)
    metadata = dict(scope=SCOPE, requested_fps=args.fps, render_capture=args.render_capture)
    print(output, flush=True)
    try:
        identity = source_identity()
        result_file, capture_file = output / "result.json", output / "first-flight.png"
        userdata = (output / "user").resolve()
        if args.packaged:
            executable = args.package_root.resolve() / "ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe"
            prefix = [str(executable)]
        else:
            executable = args.engine_root.resolve() / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
            prefix = [str(executable), str(ROOT / "ImperialWingsSimulator.uproject")]
        require(executable.is_file(), "Runtime executable not found: " + str(executable))
        command = prefix + ["-game", "-unattended", "-nosplash", "-nosound", "-ImperialNoWelcome",
                            "-ImperialFirstFlightSmoke", "-ImperialFirstFlightResult=" + str(result_file),
                            "-ImperialDataDir=" + str(userdata), "-UserDir=" + str(output / "engine-user"), "-ImperialBridgeUrl=http://127.0.0.1:1",
                            "-abslog=" + str(output / "runtime.log"), "-UseFixedTimeStep", "-FPS=" + str(args.fps)]
        if args.render_capture:
            command += ["-RenderOffscreen", "-ForceRes", "-ResX=" + str(args.width), "-ResY=" + str(args.height),
                        "-ImperialFirstFlightCapture=" + str(capture_file)]
        else:
            command.append("-nullrhi")
        metadata.update(command=command, source_identity=identity, user_data=str(userdata))
        save_json(output / "invocation.json", metadata)
        with (output / "console.log").open("w", encoding="utf-8") as log:
            run = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                 timeout=180, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        require(result_file.is_file(), "No native result; inspect runtime.log and console.log")
        result = read_json(result_file)
        require(run.returncode == 0, "Native fixture exited with code " + str(run.returncode))
        summary = validate(result, identity, args.fps)
        if args.render_capture:
            require(capture_file.is_file() and capture_file.stat().st_size > 0, "Native GPU capture is missing or empty")
            summary["capture"] = str(capture_file)
        summary.update(result_path=str(result_file), metadata=metadata, scope=SCOPE)
        save_json(output / "summary.json", summary)
        print("PASS", summary["native_checks"], "native first-flight checks; complete goal runtime remains unverified", flush=True)
        print(output / "summary.json", flush=True)
        return 0
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        save_json(output / "summary.json", dict(ok=False, error=str(error), metadata=metadata, scope=SCOPE,
                                               full_goal_runtime_verified=False))
        print("FAIL:", error, "; evidence:", output, file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
