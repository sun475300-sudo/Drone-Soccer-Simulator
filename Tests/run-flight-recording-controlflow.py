"""Compile the actual ToggleFlightRecording body with an injected failing writer.
This is control-flow reproduction, not an Unreal/filesystem or physical-flight test.
"""
import argparse,hashlib,json,os,re,subprocess,sys,uuid
from datetime import datetime,timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--expect-bug",action="store_true")
    p.add_argument("--compiler",type=Path,default=Path(r"E:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe"))
    p.add_argument("--sdk-root",type=Path,default=Path(r"C:\Program Files (x86)\Windows Kits\10"))
    a=p.parse_args()
    path=ROOT/"Source/ImperialWingsSimulator/ImperialMatchManager.cpp"
    original=path.read_bytes();text=original.decode("utf-8-sig")
    signature="void AImperialMatchManager::ToggleFlightRecording()"
    start=text.index(signature);opening=text.index("{",start);depth=1;end=opening+1
    while depth:
        if text[end]=="{":depth+=1
        elif text[end]=="}":depth-=1
        end+=1
    body=text[opening:end]
    returns_bool="bool AImperialMatchManager::RecordFlightSample(" in text
    version=a.compiler.parents[3]
    sdk_version=max(d for d in (a.sdk_root/"Include").iterdir() if (d/"ucrt/stdio.h").exists())
    out=ROOT/"Saved/Verification"/("flight-recording-controlflow-"+datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")+"-"+uuid.uuid4().hex[:8])
    out.mkdir(parents=True)
    code=r'''#include <string>
#include <vector>
#include <iostream>
#define TEXT(x) x
struct FString {
    std::string Value;
    FString()=default;
    FString(const char* V):Value(V){}
    FString(std::string V):Value(V){}
    bool IsEmpty() const{return Value.empty();}
    void Empty(){Value.clear();}
    const char* operator*() const{return Value.c_str();}
};
FString operator+(const FString& A,const FString& B){return FString(A.Value+B.Value);}
namespace ImperialUserData { FString Directory(){return "isolated-QA";} }
namespace FPaths { FString Combine(const FString& A,const FString& B){return A+FString("/")+B;} }
struct FDateTime {static FDateTime UtcNow(){return {};} FString ToString(const char*)const{return "time";}};
enum class EGuidFormats {Digits};
struct FGuid {static FGuid NewGuid(){return {};} FString ToString(EGuidFormats)const{static int I=0;return std::to_string(++I);}};
struct IFileManager {static IFileManager& Get(){static IFileManager M;return M;} bool MakeDirectory(const char*,bool){return true;}};
struct AImperialMatchManager {
    FString FlightLogPath,Notice;
    double LastFlightRecord=0;
    bool Fail=false;
    std::vector<std::string> Events;
    bool IsRecordingFlight()const{return !FlightLogPath.IsEmpty();}
    void Log(const FString& V){Notice=V;Events.push_back(V.Value);}
    void ToggleFlightRecording();
    @RETURN@ RecordFlightSample(const FString&){
        if(Fail){FlightLogPath.Empty();Log(TEXT("Flight recording stopped: could not write to Saved/FlightLogs."));@FAIL@}
        @SUCCESS@
    }
};
void AImperialMatchManager::ToggleFlightRecording()
@BODY@
int main(){
    AImperialMatchManager Start;Start.Fail=true;Start.ToggleFlightRecording();
    AImperialMatchManager Stop;Stop.FlightLogPath="previous.jsonl";Stop.Fail=true;Stop.ToggleFlightRecording();
    const bool StartLie=Start.Notice.Value=="Recording actual controls and physics. F6 stops recording.";
    const bool StopLie=Stop.Notice.Value=="Flight recording saved in Saved/FlightLogs.";
    const bool Inactive=!Start.IsRecordingFlight()&&!Stop.IsRecordingFlight();
    Start.Fail=false;Start.ToggleFlightRecording();const std::string First=Start.FlightLogPath.Value;
    const bool Retry=Start.IsRecordingFlight()&&Start.Notice.Value=="Recording actual controls and physics. F6 stops recording.";
    Start.ToggleFlightRecording();
    const bool StopSaved=!Start.IsRecordingFlight()&&Start.Notice.Value=="Flight recording saved in Saved/FlightLogs.";
    Start.ToggleFlightRecording();const bool Unique=Start.FlightLogPath.Value!=First;
    std::cout<<"{\"start_failure_overwritten\":"<<(StartLie?"true":"false")
       <<",\"stop_failure_overwritten\":"<<(StopLie?"true":"false")
       <<",\"failure_stops_recording\":"<<(Inactive?"true":"false")
       <<",\"explicit_retry_starts\":"<<(Retry?"true":"false")
       <<",\"normal_stop_saved\":"<<(StopSaved?"true":"false")
       <<",\"new_file_is_unique\":"<<(Unique?"true":"false")<<"}\n";
    return Inactive&&Retry&&StopSaved&&Unique&&(@EXPECT@)?0:1;
}
'''
    code=code.replace("@RETURN@","bool" if returns_bool else "void").replace("@FAIL@","return false;" if returns_bool else "return;").replace("@SUCCESS@","return true;" if returns_bool else "return;").replace("@BODY@",body).replace("@EXPECT@","StartLie&&StopLie" if a.expect_bug else "!StartLie&&!StopLie")
    cpp=out/"actual-toggle-injected.cpp";cpp.write_text(code,encoding="utf-8")
    executable=out/"actual-toggle-injected.exe"
    command=[str(a.compiler),"/nologo","/std:c++17","/EHsc","/W4","/WX","/O2","/MT","/I"+str(version/"include"),"/I"+str(sdk_version/"ucrt"),str(cpp),"/Fe:"+str(executable),"/Fo:"+str(out)+os.sep,"/link","/LIBPATH:"+str(version/"lib/x64"),"/LIBPATH:"+str(a.sdk_root/"Lib"/sdk_version.name/"ucrt/x64"),"/LIBPATH:"+str(a.sdk_root/"Lib"/sdk_version.name/"um/x64")]
    build=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
    (out/"compile.log").write_bytes(build.stdout)
    if build.returncode:raise RuntimeError("MSVC failed; "+str(out/"compile.log"))
    run=subprocess.run([str(executable)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30)
    details=json.loads(run.stdout.decode("utf-8-sig"))
    result=dict(ok=run.returncode==0,expect_bug=a.expect_bug,scope="Actual production ToggleFlightRecording C++ body, compiled with an injected writer. No Unreal runtime, real filesystem fault, OS input or personal calibration.",source_sha256=hashlib.sha256(original).hexdigest(),source_preserved=path.read_bytes()==original,details=details)
    (out/"result.json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result),flush=True);print(out/"result.json",flush=True)
    return 0 if result["ok"] and result["source_preserved"] else 1
if __name__=="__main__":sys.exit(main())
