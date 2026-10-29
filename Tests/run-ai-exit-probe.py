"""Isolated HUD Exit save-failure regression; baseline reproduces, fixed verifies retry."""
import argparse, hashlib, json, subprocess
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--exe',type=Path,required=True)
p.add_argument('--exe-sha256',required=True)
p.add_argument('--source-sha256',required=True)
p.add_argument('--out',type=Path,required=True,help='new empty directory, never an existing folder')
p.add_argument('--phase',choices=['baseline','fixed','active'],required=True)
a=p.parse_args()
assert a.exe.is_absolute() and a.exe.is_file()
assert a.out.is_absolute() and not a.out.exists(), 'output must be a new absolute path'
sha=lambda f: hashlib.sha256(f.read_bytes()).hexdigest()
assert sha(a.exe).lower()==a.exe_sha256.lower(), 'wrong executable'
a.out.mkdir(parents=True,exist_ok=False)
data=a.out/'data';data.mkdir()
block=data/'TrainingResults'
if a.phase!='active': block.write_text('IMPERIAL_ISOLATED_EXIT_FAILURE_FIXTURE_V1',encoding='utf-8')
before=sha(block) if block.exists() else None;probe=data/'imperial-exit-probe.json';result=a.out/'result.json'
args=[str(a.exe),'-game','-unattended','-nosound','-nosplash','-nullrhi','-ImperialNoWelcome','-ImperialBridgeUrl=http://127.0.0.1:1','-ImperialAIResultsSmoke','-ImperialAIResultsCase='+('exitactive' if a.phase=='active' else 'exitfail'),'-ImperialAIExitIsolated','-ImperialDataDir='+str(data),'-UserDir='+str(a.out/'engine-user'),'-ImperialAIExitProbe='+str(probe),'-ImperialAIResultsResult='+str(result),'-abslog='+str(a.out/'runtime.log'),'-UseFixedTimeStep','-FPS=60']
try:
 with (a.out/'console.log').open('w',encoding='utf-8') as log:
  proc=subprocess.run(args,cwd=a.exe.parent,stdout=log,stderr=subprocess.STDOUT,timeout=120,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
 assert proc.returncode==0, ('process exit',proc.returncode)
 build=json.loads((data/'running-build.json').read_text(encoding='utf-8-sig'))
 assert build['source_sha256'].lower()==a.source_sha256.lower(), 'wrong source identity'
 state=None
 if a.phase!='active':
  assert probe.is_file(), 'failed before reaching Exit; no probe'
  state=json.loads(probe.read_text(encoding='utf-8-sig'))
  assert state['exists'] is True and state['saved'] is False and state['session_id'], state
 if a.phase=='baseline':
  assert not result.exists(), 'Exit did not terminate verifier as expected'
  assert block.is_file() and sha(block)==before, 'synthetic obstruction changed'
  reports=list(data.glob('TrainingResults/AI/*.json'))
  assert not reports, 'unexpected saved training result'
  detail='Exit terminated process with unsaved result in memory; synthetic obstruction unchanged'
 else:
  assert result.is_file(), 'Exit terminated before verifier could check recovery'
  report=json.loads(result.read_text(encoding='utf-8-sig'))
  assert report['ok'] and all(c['ok'] for c in report['checks']), report
  assert report['build']['source_sha256'].lower()==a.source_sha256.lower()
  reports=list((data/'TrainingResults/AI').glob('*.json'))
  assert len(reports)==1, reports
  saved=json.loads(reports[0].read_text(encoding='utf-8-sig'))
  assert saved['test_session'] is True
  if a.phase=='fixed':
   assert saved['session_id']==state['session_id']
   detail='Exit refused; same isolated session saved after retry'
  else:
   assert saved['stop_reason']=='manual_stop' and saved['completion_reason']=='landed'
   detail='Active Exit preserves pilot state, repeated Exit remains pending, AI landing saves result'
 summary=dict(ok=True,phase=a.phase,detail=detail,exit_code=proc.returncode,exe=str(a.exe),exe_sha256=sha(a.exe),build=build,probe=state,scope='synthetic interrupted session, HUD Exit call, no physical HID or flying pilot; OS forced close not tested')
 (a.out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
 print(json.dumps(summary,indent=2))
except Exception as e:
 (a.out/'failure.txt').write_text(repr(e)+'\n',encoding='utf-8')
 raise
