"""Bounded Shipping rejection checks, including preserving an existing trace file."""
import argparse
from datetime import datetime,timezone
import json
from pathlib import Path
import subprocess
from flight_trace_analysis import require

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package-root',type=Path,default=ROOT/'dist/Portable/Windows')
    args=parser.parse_args()
    identity=json.loads(subprocess.check_output(['powershell','-NoProfile','-Command','& "'+str(ROOT/'Scripts/Get-SourceIdentity.ps1')+'" | ConvertTo-Json'],cwd=ROOT,text=True,encoding='utf-8-sig'))
    output=ROOT/'Saved/Verification'/('flight-trace-options-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f'));output.mkdir()
    binary=args.package_root/'ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe'
    cases=[('orphan',[],True,'requires the explicit ImperialPhysicsSmoke'),
           ('fixed',['-ImperialPhysicsSmoke','-ImperialSolverProbeFixed'],True,'only the unchanged synchronous'),
           ('probe_without_passthrough',['-ImperialPhysicsSmoke','-ImperialSolverProbe'],True,'only the unchanged synchronous'),
           ('missing_csv',['-ImperialPhysicsSmoke'],False,'Both ImperialFlightTraceCsv'),
           ('invalid_fps',['-ImperialPhysicsSmoke','-FPS=15'],True,'requires FPS'),
           ('existing_csv',['-ImperialPhysicsSmoke'],True,'CSV already exists')]
    results=[]
    for name,options,has_csv,expected in cases:
        directory=output/name;directory.mkdir()
        target=directory/'trace.csv';report=directory/'result.json'
        canary=b'Existing trace must remain unchanged.\r\n'
        if name=='existing_csv':target.write_bytes(canary)
        command=[str(binary),'-nullrhi','-nosound','-unattended','-nosplash','-ImperialNoWelcome',
                 '-ImperialDataDir='+str(directory/'userdata'),'-ImperialBridgeUrl=http://127.0.0.1:1',
                 '-ImperialFlightTrace','-ImperialFlightTraceResult='+str(report),
                 '-ImperialSolverProbeResult='+str(directory/'probe.json'),'-abslog='+str(directory/'runtime.log')]+options
        if has_csv:command.append('-ImperialFlightTraceCsv='+str(target))
        with (directory/'console.log').open('w',encoding='utf-8') as stream:
            process=subprocess.run(command,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,timeout=30,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        require(process.returncode in (0,1),'Unexpected engine failure code')
        data=json.loads(report.read_text(encoding='utf-8-sig'))
        require(data['ok'] is False and expected in data['error'],'Expected explicit trace rejection missing')
        require(data['build']['source_sha256']==identity['source_sha256'],'Wrong tested source')
        if name=='existing_csv':require(target.read_bytes()==canary,'Existing CSV was overwritten')
        else:require(not target.exists(),'Rejected options created a trace CSV')
        results.append({'case':name,'ok':True,'reason':data['error'],'process_exit_code':process.returncode})
        print('PASS '+name,flush=True)
    (output/'results.json').write_text(json.dumps({'ok':True,'build':identity,'checks':results},indent=2)+'\n',encoding='utf-8')
    print(output/'results.json')


if __name__=='__main__':main()
