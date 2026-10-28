"""Isolated native AI motion API and actual Chaos phase/landing regression. No USB."""
import argparse
import hashlib
import json
import re
import subprocess
import uuid
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def source_identity():
    # Same manifest as Get-SourceIdentity.ps1; also works in source ZIPs without .git.
    extensions = {".h", ".cpp", ".cs", ".ini", ".json", ".py", ".ps1", ".txt",
                  ".cmd", ".bat", ".uproject", ".uasset", ".umap", ".png"}
    binary = {".uasset", ".umap", ".png"}
    inputs = []
    for folder in ("Source", "Config", "Content", "ArtSource", "Bridge", "Scripts"):
        base = ROOT / folder
        if base.is_dir():
            inputs.extend(p for p in base.rglob("*") if p.is_file() and p.suffix.lower() in extensions)
    inputs.extend(p for p in ROOT.iterdir() if p.is_file() and p.suffix.lower() in {".uproject", ".cmd"})
    manifest = []
    for path in sorted(inputs, key=lambda p: p.relative_to(ROOT).as_posix()):
        data = path.read_bytes()
        if path.suffix.lower() not in binary:
            data = data.decode("utf-8-sig").replace("\r\n", "\n").encode("utf-8")
        manifest.append(path.relative_to(ROOT).as_posix() + "\0" + hashlib.sha256(data).hexdigest() + "\n")
    settings = (ROOT / "Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
    version = re.search(r"(?m)^ProjectVersion=([^\r\n]+)", settings).group(1).strip()
    return {"version": version, "source_sha256": hashlib.sha256("".join(manifest).encode("utf-8")).hexdigest()}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packaged", action="store_true")
    parser.add_argument("--package-root", type=Path, default=ROOT / "dist/Portable/Windows")
    parser.add_argument("--engine", type=Path, default=Path("E:/Program Files/UE_5.8"))
    parser.add_argument("--output-root", type=Path, default=ROOT / "Saved/Verification")
    parser.add_argument("--fps", type=int, choices=[20, 60, 240], default=60)
    args = parser.parse_args()
    out = args.output_root.resolve() / ("ai-motion-" + datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8])
    out.mkdir(parents=True)
    data = out / "userdata"
    data.mkdir()
    assert data.is_absolute() and data.parent == out
    expected = source_identity()
    if args.packaged:
        command = [str(args.package_root.resolve() / "ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe")]
    else:
        command = [str(args.engine.resolve() / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"), str(ROOT / "ImperialWingsSimulator.uproject")]
    command += ["-game", "-nullrhi", "-unattended", "-nosound", "-nosplash",
                "-ImperialNoWelcome", "-ImperialBridgeUrl=http://127.0.0.1:1",
                "-ImperialAIMotionSmoke", "-ImperialAIMotionResult=" + str(out / "result.json"),
                "-ImperialDataDir=" + str(data), "-abslog=" + str(out / "runtime.log"),
                "-UseFixedTimeStep", "-FPS=" + str(args.fps)]
    (out / "invocation.json").write_text(json.dumps({"command": command, "expected": expected,
        "scope": "Explicit unique QA user data; hidden nullrhi native fixture; no OS input or real controller."}, indent=2) + "\n", encoding="utf-8")
    print(out, flush=True)
    with (out / "console.log").open("w", encoding="utf-8") as log:
        process = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
            timeout=180, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    path = out / "result.json"
    assert path.is_file(), ("native fixture did not create a result", process.returncode, out / "console.log")
    result = json.loads(path.read_text(encoding="utf-8-sig"))
    assert process.returncode == 0 and result["ok"] and result["checks"] and all(c["ok"] for c in result["checks"]), result
    assert result["build"]["source_sha256"] == expected["source_sha256"], "stale source/build identity"
    assert result["build"]["version"] == expected["version"], "version mismatch"
    assert Path(result["data_directory"]).resolve() == data.resolve(), "fixture used another user directory"
    assert abs(result["min_game_dt_s"] - 1 / args.fps) < 1e-6 and abs(result["max_game_dt_s"] - 1 / args.fps) < 1e-6
    assert result["goals_observed"] >= 1 and result["cycles_observed"] >= 1
    assert all(p["seen"] and p["frames"] > 0 for p in result["phases"]), "missing actual attack phase"
    assert result["transit_frames"] > 0 and result["precision_frames"] > 0
    assert result["samples"] and all(s["other_nine_limits_default"] for s in result["samples"]), "limits leaked to other bodies"
    assert not (data / "Config/RadioProfile.json").exists(), "fixture unexpectedly created a radio profile"
    print("PASS", len(result["checks"]), "native motion safety checks;", path, flush=True)

if __name__ == "__main__":
    main()
