"""Actual S-course flight and contact/direction/timing rejection in isolated user data."""
import argparse,json,subprocess,uuid
from datetime import datetime,timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--packaged',action='store_true');p.add_argument('--package-root',type=Path,default=ROOT/'dist/Portable/Windows');p.add_argument('--fps',type=int,choices=[20,60,240],default=60);p.add_argument('--cases',type=int,choices=[1,6],default=6);p.add_argument('--capture',action='store_true');a=p.parse_args()
 out=ROOT/'Saved/Verification'/('topgun-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);out.mkdir(parents=True)
 identity=json.loads(subprocess.check_output(['powershell','-NoProfile','-Command','& "'+str(ROOT/'Scripts/Get-SourceIdentity.ps1')+'" | ConvertTo-Json'],cwd=ROOT,text=True,encoding='utf-8-sig'))
 command=[str(a.package_root/'ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe')] if a.packaged else ['E:/Program Files/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe',str(ROOT/'ImperialWingsSimulator.uproject')]
 base=['-game','-unattended','-nosplash','-nosound','-ImperialNoWelcome','-ImperialBridgeUrl=http://127.0.0.1:1']
 args=base+['-ImperialTopgunSmoke','-ImperialTopgunCases='+str(1 if a.capture else a.cases),'-ImperialDataDir='+str(out/'user'),'-ImperialTopgunResult='+str(out/'result.json'),'-abslog='+str(out/'runtime.log'),'-UseFixedTimeStep','-FPS='+str(a.fps)]
 args+=['-RenderOffscreen','-ResX=1600','-ResY=1000','-ForceRes','-ImperialTopgunCapture='+str(out/'topgun.png')] if a.capture else ['-nullrhi']
 print(out,flush=True)
 def launch(arguments,folder):
  with (folder/'console.log').open('w',encoding='utf-8') as log:run=subprocess.run(command+arguments,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=360,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
  r=json.loads((folder/'result.json').read_text(encoding='utf-8-sig'));assert run.returncode==0 and r['ok'] and all(c['ok'] for c in r['checks']),[c for c in r['checks'] if not c['ok']]
  assert r['build']['source_sha256']==identity['source_sha256'],'stale build'
  print('PASS',len(r['checks']),'native checks;',folder/'result.json',flush=True);return r
 r=launch(args,out)
 assert abs(r['min_game_dt_s']-1/a.fps)<1e-6 and abs(r['max_game_dt_s']-1/a.fps)<1e-6
 records=[json.loads(line) for f in (out/'user/FlightLogs').glob('*.jsonl') for line in f.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
 active=[r['topgun'] for r in records if r.get('topgun',{}).get('active')]
 assert len(active)>5 and all(not r['ranked'] and r['test_session'] and r['course']=='S-01-v1' for r in active)
 complete=[r for r in active if r['state']==2 and r['next_gate']==5 and r['elapsed_s']>5]
 assert complete,'missing complete course trace'
 assert all(r['splits_s'][0]==0 and r['splits_s'][-1]==r['elapsed_s'] and all(b>a for a,b in zip(r['splits_s'],r['splits_s'][1:])) for r in complete),'incorrect split telemetry'
 if not a.capture:
  folder=out/'normal-controls';folder.mkdir()
  launch(base+['-nullrhi','-ImperialTopgunControlSmoke','-ImperialDataDir='+str(folder/'user'),'-ImperialTopgunResult='+str(folder/'result.json'),'-abslog='+str(folder/'runtime.log')],folder)
 assert not a.capture or (out/'topgun.png').is_file()
if __name__=='__main__':main()
