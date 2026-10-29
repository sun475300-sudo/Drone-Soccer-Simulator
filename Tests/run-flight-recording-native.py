"""Native flight-recording real filesystem failure and explicit-retry regression.
Only fresh per-run QA data is used. No OS input, GPU rendering or physical USB.
"""
import argparse,hashlib,json,os,shutil,subprocess,sys,uuid
from datetime import datetime,timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SCOPE="Native Toggle/periodic Record calls, real directory obstruction and deny-share sample/stop write failures; no OS/GPU/USB or commanded flight. Disk-full/partial-write/power-loss atomicity is not verified."

def read(path):return json.loads(path.read_text(encoding="utf-8-sig"))
def save(path,value):path.write_text(json.dumps(value,ensure_ascii=False,indent=2,allow_nan=False)+"\n",encoding="utf-8")
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def require(ok,message):
    if not ok:raise ValueError(message)
def identity():
    shell=shutil.which("powershell") or shutil.which("pwsh")
    require(shell,"PowerShell is required for source identity")
    script=str(ROOT/"Scripts/Get-SourceIdentity.ps1").replace("'","''")
    return json.loads(subprocess.check_output([shell,"-NoProfile","-Command","[Console]::OutputEncoding=[Text.Encoding]::UTF8; & '"+script+"' | ConvertTo-Json -Compress"],cwd=ROOT,text=True,encoding="utf-8-sig",timeout=60,creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0)))
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packaged",action="store_true")
    parser.add_argument("--package-root",type=Path,default=ROOT/"dist/PostRelease/Windows")
    parser.add_argument("--engine-root",type=Path,default=Path("E:/Program Files/UE_5.8"))
    parser.add_argument("--fps",type=int,choices=(20,60,240),default=60)
    a=parser.parse_args()
    out=(ROOT/"Saved/Verification"/("flight-recording-native-"+datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")+"-"+uuid.uuid4().hex[:8])).resolve()
    out.mkdir(parents=True)
    user=out/"user";user.mkdir()
    (user/".flight-recording-qa").write_bytes(b"imperial-flight-recording-qa-v1\n")
    sentinel=b"keep unrelated QA data unchanged\n";(user/"keep.txt").write_bytes(sentinel)
    info=dict(scope=SCOPE,user_data=str(user),fps=a.fps)
    print(out,flush=True)
    try:
        expected=identity()
        if a.packaged:
            exe=a.package_root.resolve()/"ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe"
            prefix=[str(exe)]
        else:
            exe=a.engine_root.resolve()/"Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
            prefix=[str(exe),str(ROOT/"ImperialWingsSimulator.uproject")]
        require(exe.is_file(),"Runtime executable missing: "+str(exe))
        before=digest(exe)
        args=prefix+["-game","-nullrhi","-unattended","-nosound","-nosplash","-ImperialNoWelcome","-ImperialBridgeUrl=http://127.0.0.1:1","-ImperialFlightRecordingSmoke","-ImperialFlightRecordingResult="+str(out/"result.json"),"-ImperialDataDir="+str(user),"-UserDir="+str(out/"engine-user"),"-abslog="+str(out/"runtime.log"),"-UseFixedTimeStep","-FPS="+str(a.fps)]
        info.update(command=args,source_before=expected,executable_sha256_before=before)
        save(out/"invocation.json",info)
        with (out/"console.log").open("w",encoding="utf-8") as log:
            process=subprocess.run(args,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=90,creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
        info["actual_process_exit_code"]=process.returncode
        require((out/"result.json").is_file(),"Native fixture did not write result JSON")
        result=read(out/"result.json")
        require(process.returncode==0 and result.get("ok") is True,"Native fixture failed or did not exit normally")
        checks=result.get("checks",[])
        require(len(checks)==42 and all(x.get("ok") is True for x in checks),"Native checks failed or expected scenarios did not all run")
        require(result["build"]["source_sha256"]==expected["source_sha256"],"Runtime does not match current source fingerprint")
        require(result["build"]["version"]==expected["version"],"Runtime version mismatch")
        require(Path(result["data_directory"]).resolve()==user,"Native fixture used different user data")
        require(abs(result["min_game_dt_s"]-1/a.fps)<1e-6 and abs(result["max_game_dt_s"]-1/a.fps)<1e-6,"Actual Tick interval mismatch")
        logs=sorted((user/"FlightLogs").glob("*.jsonl"))
        failed={Path(result["failed_sample_path"]).resolve(),Path(result["failed_stop_path"]).resolve()}
        require(len(logs)==5 and len(failed)==2 and failed.issubset({x.resolve() for x in logs}),"Wrong log separation or partial-file evidence")
        report=[]
        for path in logs:
            lines=[json.loads(line) for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip()]
            require(lines and lines[0]["event"]=="start","Log lost its start boundary")
            partial=path.resolve() in failed
            require((lines[-1]["event"]!="stop") if partial else (lines[-1]["event"]=="stop"),"Failed log falsely claims a normal stop")
            require(all(x["source_sha256"]==expected["source_sha256"] and len(x["aircraft"])==10 for x in lines),"Flight JSON schema or source identity mismatch")
            require(all(not body["armed"] and not body["autonomous"] for x in lines for body in x["aircraft"]),"Recording fixture unexpectedly armed or flew an aircraft")
            report.append(dict(name=path.name,sha256=digest(path),events=[x["event"] for x in lines],incomplete_after_write_failure=partial))
        require((user/"keep.txt").read_bytes()==sentinel,"Unrelated QA sentinel was modified")
        require(not list(user.rglob("RadioProfile.json")),"Fixture created a calibration profile")
        after=identity()
        require(after["source_sha256"]==expected["source_sha256"],"Source changed during verification")
        require(digest(exe)==before,"Runtime executable changed during verification")
        summary=dict(ok=True,scope=SCOPE,build=result["build"],native_checks=len(checks),actual_process_exit_code=process.returncode,actual_tick_min_s=result["min_game_dt_s"],actual_tick_max_s=result["max_game_dt_s"],source_preserved=True,executable_preserved=True,logs=report,metadata=info,result=str(out/"result.json"))
        save(out/"summary.json",summary)
        print("PASS",len(checks),"native recording failure/retry checks; preserved two incomplete logs and three complete logs",flush=True)
        print(out/"summary.json",flush=True)
        return 0
    except (OSError,ValueError,KeyError,TypeError,subprocess.SubprocessError) as error:
        save(out/"summary.json",dict(ok=False,error=str(error),scope=SCOPE,metadata=info))
        print("FAIL:",error,"; evidence:",out,file=sys.stderr,flush=True)
        return 1
if __name__=="__main__":sys.exit(main())
