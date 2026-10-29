"""Actual goal/contact summary, report persistence, malformed history and save recovery."""
import argparse,copy,hashlib,json,subprocess,uuid
from datetime import datetime,timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--packaged',action='store_true');p.add_argument('--package-root',type=Path,default=ROOT/'dist/Portable/Windows');p.add_argument('--capture',action='store_true');p.add_argument('--engine',type=Path,default=Path('E:/Program Files/UE_5.8'));a=p.parse_args()
 out=ROOT/'Saved/Verification'/('ai-results-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);out.mkdir(parents=True)
 identity=json.loads(subprocess.check_output(['powershell','-NoProfile','-Command','& "'+str(ROOT/'Scripts/Get-SourceIdentity.ps1')+'" | ConvertTo-Json'],cwd=ROOT,text=True,encoding='utf-8-sig'))
 command=[str(a.package_root/'ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe')] if a.packaged else [str(a.engine/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'),str(ROOT/'ImperialWingsSimulator.uproject')]
 results=[]
 def run(name,case,data):
  data.mkdir(parents=True,exist_ok=True);directory=out/name;directory.mkdir()
  args=['-game','-unattended','-nosound','-nosplash','-ImperialNoWelcome','-ImperialBridgeUrl=http://127.0.0.1:1','-ImperialAIResultsSmoke','-ImperialAIResultsCase='+case,'-ImperialDataDir='+str(data),'-UserDir='+str(directory/'engine-user'),'-ImperialAIResultsResult='+str(directory/'result.json'),'-abslog='+str(directory/'runtime.log'),'-UseFixedTimeStep','-FPS=60']
  args+=['-RenderOffscreen','-ResX=1600','-ResY=1000','-ForceRes','-ImperialAIResultsCapture='+str(directory/'results.png')] if a.capture else ['-nullrhi']
  with (directory/'console.log').open('w',encoding='utf-8') as log:
   proc=subprocess.run(command+args,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=120,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
  r=read(directory/'result.json');assert proc.returncode==0 and r['ok'] and all(c['ok'] for c in r['checks']),(directory,r)
  assert r['build']['source_sha256']==identity['source_sha256'],'stale build'
  results.append(dict(name=name,path=(directory/'result.json').relative_to(ROOT).as_posix(),sha256=digest(directory/'result.json'),result=r));print('PASS',name,len(r['checks']),'checks',flush=True)
 data=out/'fresh-user';print(out,flush=True);run('fresh','fresh',data)
 files=list((data/'TrainingResults/AI').glob('*.json'));assert len(files)==1,'report duplicated'
 saved=read(files[0]);assert saved['goals']==1 and saved['completion_reason']=='landed' and saved['test_session'] and saved['aircraft'][0]['guard_impacts']>0
 original=digest(files[0])
 if not a.capture:
  run('reload','reload',data);assert digest(files[0])==original and len(list(files[0].parent.glob('*.json')))==1
  bad_cases={
   'unsupported_schema':lambda d:d.update(schema=99),
   'string_number':lambda d:d.update(opponents='3'),
   'invalid_count':lambda d:d.update(opponents=8),
   'negative_time':lambda d:d.update(practice_seconds=-1),
   'fractional_goal':lambda d:d.update(goals=.5),
   'duplicate_aircraft':lambda d:d['aircraft'][1].update(id=0),
   'invalid_guard':lambda d:d['aircraft'][0].update(guard_remaining_percent=101),
   'invalid_reason':lambda d:d.update(completion_reason='victory'),
   'invalid_hash':lambda d:d.update(source_sha256='x'*64),
   'string_bool':lambda d:d.update(test_session='true'),
   'numeric_version_name':lambda d:d.update(build_version=78),
   'missing_field':lambda d:d.pop('test_session'),
   'invalid_duel_mode':lambda d:d.update(session_mode='match'),
   'invalid_opponent_score':lambda d:d.update(opponent_goals=-1),
   'invalid_recovery_count':lambda d:d.update(recovery_attempts=-1),
  }
  for name,alter in bad_cases.items():
   user=out/('user-'+name);target=user/'TrainingResults/AI/9999999999999999999-bad.json';target.parent.mkdir(parents=True)
   fixture=copy.deepcopy(saved);alter(fixture);target.write_text(json.dumps(fixture),encoding='utf-8');before=digest(target)
   run(name,'invalid',user);assert digest(target)==before,'malformed history altered'
  corrupt=files[0].parent/'9999999999999999999-corrupt.json';corrupt.write_text('{broken',encoding='utf-8');old=digest(corrupt)
  run('latest_corrupt_fallback','reload',data);assert digest(corrupt)==old and digest(files[0])==original
  legacy=out/'legacy-user/TrainingResults/AI/0000000000000000001-legacy.json';legacy.parent.mkdir(parents=True);fixture=copy.deepcopy(saved);fixture.pop('recovery_attempts');fixture['schema']=1;fixture['defenders']=fixture.pop('opponents');fixture.pop('session_mode');fixture.pop('opponent_goals');legacy.write_text(json.dumps(fixture),encoding='utf-8');before=digest(legacy)
  run('legacy_without_recovery_count','reload',legacy.parent.parent.parent);assert digest(legacy)==before
  blocked=out/'save-blocked-user';blocked.mkdir();(blocked/'TrainingResults').write_text('intentional file blocks directory creation',encoding='utf-8')
  run('save_failure_retry','savefail',blocked)
  history=[read(f) for f in (blocked/'TrainingResults/AI').glob('*.json')];assert len(history)==2 and len({r['session_id'] for r in history})==2 and all(r['completion_reason']=='reset' for r in history)
  discard=out/'discard-user';discard.mkdir();block=discard/'TrainingResults';block.write_text('intentional save obstruction',encoding='utf-8');old=digest(block)
  run('explicit_discard','discard',discard);assert digest(block)==old,'discard modified unrelated blocking file'
  exitdata=out/'exit-user';run('application_exit','exit',exitdata)
  exits=[read(f) for f in (exitdata/'TrainingResults/AI').glob('*.json')];assert len(exits)==1 and exits[0]['stop_reason']=='application_exit' and exits[0]['completion_reason']=='application_exit' and exits[0]['practice_seconds']>=1,'missing interrupted exit report'
 else:assert (out/'fresh/results.png').is_file()
 (out/'results.json').write_text(json.dumps(dict(ok=True,build=identity,cases=results,scope='Explicit diagnostic goal flight, contacts, native menu and isolated local result files. No USB/human pilot.'),indent=2)+'\n',encoding='utf-8')
 print(out/'results.json',flush=True)
if __name__=='__main__':main()
