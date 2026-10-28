"""Full embedded release payload installer CLI QA. No game launch or personal profiles."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,time,traceback,uuid,zipfile
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--installer',type=Path,required=True,help='QA-only installer EXE to exercise')
parser.add_argument('--bundle-zip',type=Path,required=True,help='Exact portable ZIP embedded in that EXE')
parser.add_argument('--qa-root',type=Path,required=True,help='Disposable QA directory; only unique new child paths are modified')
parser.add_argument('--source-root',type=Path,default=Path(__file__).resolve().parent,help='Read-only directory containing InstallerSetup.cs and Build-Installer.ps1')
parser.add_argument('--build-metadata',type=Path,help='Optional builder JSON with SHA256/PayloadSHA256')
parser.add_argument('--expected-payload-files',type=int,default=133)
OPTIONS=parser.parse_args()
SCRATCH=OPTIONS.source_root.resolve()
QA=OPTIONS.qa_root.resolve()
if not QA.is_dir() or QA.parent==QA:raise ValueError('An existing dedicated QA directory is required')
ID=uuid.uuid4().hex[:6]
OUT=QA/('harness-review-'+ID);OUT.mkdir(exist_ok=False)
EXE=OPTIONS.installer.resolve()
VERSION=None
RECEIPT='.imperial-wings-install.json'
checks=[];runs=[];fingerprints={};expected={};sentinel=None

def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def tree(root):
 if not root.exists():return None
 data={}
 for current,dirs,files in os.walk(root,followlinks=False):
  for d in dirs:
   assert not (getattr((Path(current)/d).lstat(),'st_file_attributes',0)&0x400),'No reparse traversals permitted in QA snapshots'
  for name in files:
   p=Path(current)/name;assert not(getattr(p.lstat(),'st_file_attributes',0)&0x400)
   data[p.relative_to(root).as_posix()]=sha(p)
 return data

def save(error=None):
 data=dict(ok=bool(checks) and all(c['passed'] for c in checks) and error is None,scope='Full portable payload in QA-only installer. CLI only, launcher --inspect only, synthetic private-data sentinels only. No actual game, normal launcher, real profiles, registration or OS input.',payload_version=VERSION,payload_file_count=len(expected),checks=checks,runs=runs,source_fingerprints=fingerprints,error=error,not_verified=['GUI installer/self-uninstaller interactions','Real controller hardware','Second PC','Windows SmartScreen on download','Power-loss recovery','OS permission/disk-full changes'])
 (OUT/'summary.json').write_text(json.dumps(data,ensure_ascii=True,indent=2)+'\n',encoding='utf-8')
def check(name,ok,detail=None):
 checks.append(dict(name=name,passed=bool(ok),detail=detail));save();print(('PASS ' if ok else 'FAIL ')+name,flush=True)
 if not ok:raise AssertionError(name+': '+str(detail))
def call(name,action,target,mode=None,extra=(),expect=True):
 assert target.parent==QA and target.name.startswith(('H1','H2','H3')) and ID in target.name
 result=OUT/(name+'.json');assert not result.exists()
 args=[str(EXE),action,str(target),'--quiet','--no-register','--result',str(result)]
 if mode:args+=['--mode',mode]
 args+=list(extra)
 started=time.monotonic()
 try:p=subprocess.run(args,capture_output=True,timeout=120,creationflags=subprocess.CREATE_NO_WINDOW)
 except subprocess.TimeoutExpired:
  runs.append(dict(name=name,timeout=True,args=args));save();raise
 (OUT/(name+'.log')).write_bytes(p.stdout+b'\nSTDERR:\n'+p.stderr)
 d=json.loads(result.read_text(encoding='utf-8-sig')) if result.exists() else None
 runs.append(dict(name=name,args=args,exit_code=p.returncode,elapsed_s=round(time.monotonic()-started,3),result=d));save()
 check(name+' structured result',isinstance(d,dict))
 check(name+(' succeeds' if expect else ' safely refuses'),(p.returncode==0 and d['ok']) if expect else (p.returncode!=0 and not d['ok']),dict(exit_code=p.returncode,message=d.get('message')))
 return d

def validate_complete(target):
 hashes=tree(target)
 check('all 133 payload files match embedded ZIP hashes',len(expected)==OPTIONS.expected_payload_files and all(hashes.get(rel)==h for rel,h in expected.items()))
 check('installed inventory is exactly 133 payload plus uninstaller plus receipt',set(hashes)==set(expected)|{'ImperialWingsUninstall.exe',RECEIPT},dict(installed_files=len(hashes)))
 r=json.loads((target/RECEIPT).read_text(encoding='utf-8-sig'))
 check('receipt binds product schema version and absolute target',r['product']=='ImperialWingsSimulator' and r['schema']==1 and r['version']==VERSION and Path(r['target'])==target)
 check('receipt owns exactly all 134 program files with correct hashes',len(r['files'])==len(expected)+1 and set(r['files'])==set(expected)|{'ImperialWingsUninstall.exe'} and all(hashes.get(rel)==h for rel,h in r['files'].items()))
 return hashes

def main():
 global fingerprints,expected,sentinel,VERSION
 build=json.loads(OPTIONS.build_metadata.read_text(encoding='utf-8-sig')) if OPTIONS.build_metadata else None
 check('reviewed full installer exists',EXE.is_file())
 fingerprints={str(p):sha(p) for p in [SCRATCH/'InstallerSetup.cs',SCRATCH/'Build-Installer.ps1',EXE]}
 archive=OPTIONS.bundle_zip.resolve()
 archive_sha=sha(archive)
 if build:
  check('installer hash matches reviewed build metadata',fingerprints[str(EXE)]==build['SHA256'])
  check('embedded ZIP source hash matches reviewed build metadata',archive_sha==build['PayloadSHA256'])
 with zipfile.ZipFile(archive) as z:
  release=[i for i in z.infolist() if i.filename.endswith('/release-manifest.json')]
  assert len(release)==1
  VERSION=json.loads(z.read(release[0]))['version']
  for i in z.infolist():
   if not i.is_dir():
    with z.open(i) as f:expected[i.filename.split('/',1)[1]]=hashlib.file_digest(f,'sha256').hexdigest()
 check('full archive has expected payload inventory',len(expected)==OPTIONS.expected_payload_files,dict(actual=len(expected),expected=OPTIONS.expected_payload_files))
 (OUT/'payload-hashes.json').write_text(json.dumps(expected,indent=2)+'\n',encoding='utf-8')
 sentinel=OUT/'synthetic-personal/Saved/Config/RadioProfile.json';sentinel.parent.mkdir(parents=True);sentinel.write_bytes(b'synthetic external calibration: never overwrite')
 sentinel_before=tree(OUT/'synthetic-personal')
 target=QA/('H1\ud55c \uae00-'+ID)
 assert all(not p.exists() for p in [target,QA/('H2-'+ID),QA/('H3-'+ID)])
 assert len(str(target))+len('.backup-20261028-000000-12345678')+max(map(len,expected))+1<245
 call('fresh-unicode-space','--install',target,'fresh')
 before=validate_complete(target)
 call('verify-complete','--verify',target)
 inspect=OUT/'launcher-inspect.json';started=time.monotonic()
 p=subprocess.run([str(target/'ImperialWingsLauncher.exe'),'--inspect',str(inspect)],capture_output=True,timeout=30,creationflags=subprocess.CREATE_NO_WINDOW)
 d=json.loads(inspect.read_text(encoding='utf-8-sig')) if inspect.exists() else None
 runs.append(dict(name='launcher-inspect-only',exit_code=p.returncode,elapsed_s=round(time.monotonic()-started,3),result=d));save()
 check('launcher inspect succeeds without normal launcher execution',p.returncode==0 and isinstance(d,dict))
 check('launcher inspect reports VC runtime field and complete components',isinstance(d.get('vc_runtime_installed'),bool) and d.get('windows64') is True and d.get('game_exists') is True and d.get('bridge_exists') is True,dict(vc_runtime_installed=d.get('vc_runtime_installed')))
 check('launcher inspect reports no installed Python Git or Unreal requirement',all(d.get(k) is False for k in ['requires_installed_python','requires_installed_git','requires_installed_unreal']))
 check('inspect leaves all installed file bytes unchanged',tree(target)==before)
 updated=call('same-version-update','--install',target,'update')
 backup=Path(updated['backup']);assert backup.parent==QA and backup.name.startswith(target.name+'.backup-')
 backup_snapshot=tree(backup)
 check('same-version update keeps byte-exact previous backup',backup_snapshot==before)
 before=validate_complete(target)
 call('verify-after-update','--verify',target)
 backupnames={p.name for p in QA.iterdir() if p.name.startswith(target.name+'.backup-')}
 rollback=call('injected-rollback','--install',target,'update',extra=['--test-fail','after-backup','--test-root',str(QA)],expect=False)
 check('injected commit failure explicitly restores rollback',rollback.get('rollback_restored') is True)
 check('rollback target is exactly equal to pre-failure tree',tree(target)==before)
 check('rollback leaves no extra backup and keeps prior backup',{p.name for p in QA.iterdir() if p.name.startswith(target.name+'.backup-')}==backupnames and tree(backup)==backup_snapshot)
 separate=QA/('H2-'+ID)
 call('separate-install','--install',separate,'fresh');validate_complete(separate)
 check('separate install preserves original installation bytes',tree(target)==before)
 call('separate-clean-remove','--uninstall',separate)
 check('separate clean removal removes whole owned target',not separate.exists())
 check('separate removal preserves original installation bytes',tree(target)==before)
 unknown=target/'player-notes.txt';unknown.write_bytes(b'synthetic player notes retained')
 before_unknown=tree(target)
 refused=call('unknown-file-update-refusal','--install',target,'update',expect=False)
 check('unknown-file refusal names additional data policy','\ucd94\uac00 \ud30c\uc77c' in refused.get('message',''))
 check('unknown-file update refusal preserves exact full tree',tree(target)==before_unknown)
 radio=target/'Saved/Config/RadioProfile.json';radio.parent.mkdir(parents=True);radio.write_bytes(b'synthetic in-target calibration preserved')
 flight=target/'FlightLogs/session-qa.json';flight.parent.mkdir(parents=True);flight.write_bytes(b'{"synthetic":true,"preserve":"flight record"}')
 before_private=tree(target)
 call('synthetic-saved-flightlogs-update-refusal','--install',target,'update',expect=False)
 check('Saved and FlightLogs refusal preserves every source byte',tree(target)==before_private)
 doc=target/'START_HERE.ko.md';assert doc.relative_to(target).as_posix() in expected
 with doc.open('ab') as f:f.write(b'\nSynthetic user annotation: retain modified known document.\n')
 retained={p.relative_to(target).as_posix():sha(p) for p in [unknown,radio,flight,doc]}
 removed=call('remove-preserve-user-data','--uninstall',target)
 check('uninstall preserves exactly unknown modified Saved and FlightLogs files',tree(target)==retained)
 check('uninstall explicitly reports every retained file',set(removed['retained_files'])==set(retained))
 check('uninstall removes receipt and owned executables',not(target/RECEIPT).exists() and not(target/'ImperialWingsLauncher.exe').exists() and not(target/'ImperialWingsUninstall.exe').exists())
 missing=QA/('H3-'+ID)
 call('missing-required-setup','--install',missing,'fresh')
 required=missing/'Windows/ImperialWingsSimulator.exe'
 check('missing-binary regression starts with correct owned required file',required.is_file() and sha(required)==expected['Windows/ImperialWingsSimulator.exe'])
 required.unlink()
 check('only QA-owned required game executable deliberately removed',not required.exists() and (missing/RECEIPT).is_file())
 call('missing-required-verify-refusal','--verify',missing,expect=False)
 call('missing-required-remove','--uninstall',missing)
 check('P2 uninstall succeeds when required payload binary is missing',not missing.exists())
 check('external synthetic private sentinel stays byte-identical',tree(OUT/'synthetic-personal')==sentinel_before)
 check('owned targets leave no staging remnants',not any(p.name.startswith('.iw-stage-') for p in QA.iterdir()))
 check('source builder and reviewed installer inputs remain unchanged',all(sha(Path(name))==h for name,h in fingerprints.items()))
 save();print('RESULT '+str(OUT/'summary.json'),flush=True)
try:main()
except BaseException:
 error=traceback.format_exc();save(error);print(error,flush=True);raise
