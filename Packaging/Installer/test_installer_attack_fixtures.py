"""Builder rejection and real process-lock checks using tiny synthetic payloads."""
from pathlib import Path
import argparse,ctypes,hashlib,json,os,subprocess,time,uuid,zipfile,warnings
ROOT=Path(__file__).resolve().parent

def hashfile(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def tree(root):return {p.relative_to(root).as_posix():hashfile(p) for p in root.rglob('*') if p.is_file()} if root.exists() else None

def main():
 out=ROOT/'qa'/('attack-fixtures-'+uuid.uuid4().hex[:8]);out.mkdir(parents=True);checks=[];runs=[]
 def save():
  (out/'summary.json').write_text(json.dumps(dict(ok=all(c['passed'] for c in checks),scope='Synthetic test installer payloads; no real game, OS keyboard/mouse or personal profiles.',checks=checks,runs=runs),indent=2)+'\n',encoding='utf-8')
 def check(name,ok,detail=''):
  checks.append(dict(name=name,passed=bool(ok),detail=detail));save();print(('PASS ' if ok else 'FAIL ')+name,flush=True)
  if not ok:raise AssertionError(name+': '+detail)
 with zipfile.ZipFile(ROOT/'fixtures/0.0.2-fixture.zip') as source:base={i.filename:source.read(i) for i in source.infolist() if not i.is_dir()}
 prefix=next(iter(base)).split('/')[0]+'/'
 def build(name,data=None,extra=None,symlink=None):
  archive=out/(name+'.zip');exe=out/(name+'.exe');payload=dict(base if data is None else data)
  with warnings.catch_warnings():
   warnings.simplefilter('ignore',UserWarning)
   with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_DEFLATED) as target:
    for path,b in payload.items():target.writestr(path,b)
    if extra:target.writestr(*extra)
    if symlink:
     info=zipfile.ZipInfo(prefix+symlink);info.create_system=3;info.external_attr=(0o120777<<16);target.writestr(info,b'../outside-do-not-touch')
  command=['powershell','-NoProfile','-File',str(ROOT/'Build-Installer.ps1'),'-BundleZip',str(archive),'-OutputPath',str(exe)]
  started=time.monotonic()
  with (out/(name+'-build.log')).open('w',encoding='utf-8') as log:p=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=60,creationflags=subprocess.CREATE_NO_WINDOW)
  runs.append(dict(name=name,kind='build',exit_code=p.returncode,output_exists=exe.exists(),elapsed_s=round(time.monotonic()-started,3)));save();return p.returncode,exe
 def refusal(name,extra=None,alter=None):
  payload=dict(base)
  if alter:alter(payload)
  code,exe=build(name,payload,extra=extra)
  check(name+' builder rejects before executable output',code!=0 and not exe.exists())
 refusal('traversal',extra=(prefix+'../escape.txt',b'never escape'))
 refusal('absolute-drive',extra=(prefix+'C:/escape.txt',b'never escape'))
 refusal('private-data',extra=(prefix+'Saved/Config/RadioProfile.json',b'never distribute'))
 refusal('unlisted-entry',extra=(prefix+'unexpected.bin',b'not in manifest'))
 refusal('duplicate-entry',extra=(next(iter(base)),b'duplicate'))
 refusal('changed-hash',alter=lambda d:d.__setitem__(prefix+'START_HERE.ko.md',b'corrupted bytes'))
 # Hash-consistent Unix symlink fixture: either build or installer must reject it.
 payload=dict(base);manifest_name=prefix+'SHA256SUMS.txt';payload[manifest_name]+=hashlib.sha256(b'../outside-do-not-touch').hexdigest().encode()+b'  unsafe-link\n'
 code,exe=build('symlink-payload',payload,symlink='unsafe-link')
 if code!=0:
  check('symlink payload rejected by builder',not exe.exists())
 else:
  target=out/'symlink-target';result=out/'symlink-result.json';sentinel=out/'outside-do-not-touch';sentinel.write_bytes(b'keep outside sentinel')
  p=subprocess.run([str(exe),'--install',str(target),'--mode','fresh','--quiet','--no-register','--result',str(result)],timeout=25,creationflags=subprocess.CREATE_NO_WINDOW)
  d=json.loads(result.read_text(encoding='utf-8-sig'));runs.append(dict(name='symlink-runtime',exit_code=p.returncode,result=d));check('symlink resource rejected by runtime',p.returncode!=0 and not d['ok'] and not target.exists());check('symlink rejection preserves outside sentinel',sentinel.read_bytes()==b'keep outside sentinel')
 # Running-file detection uses a real harmless waiting process, never Unreal.
 source=out/'WaitFixture.cs';source.write_text('using System; using System.Threading; internal static class WaitFixture { static int Main(string[] args) { Thread.Sleep(8000); return 0; } }',encoding='utf-8')
 waitexe=out/'WaitFixture.exe';csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
 p=subprocess.run([str(csc),'/nologo','/target:exe','/platform:x64','/out:'+str(waitexe),str(source)],capture_output=True,timeout=30,creationflags=subprocess.CREATE_NO_WINDOW);check('harmless wait fixture compiles with existing compiler',p.returncode==0 and waitexe.exists())
 payload=dict(base);payload[prefix+'ImperialWingsLauncher.exe']=waitexe.read_bytes();payload[manifest_name]=b''.join(hashlib.sha256(b).hexdigest().encode()+b'  '+name[len(prefix):].encode()+b'\n' for name,b in sorted(payload.items()) if name!=manifest_name)
 code,exe=build('real-running-fixture',payload);check('real running fixture installer built',code==0)
 target=out/'running-install';result=out/'running-install.json'
 p=subprocess.run([str(exe),'--install',str(target),'--mode','fresh','--quiet','--no-register','--result',str(result)],timeout=25,creationflags=subprocess.CREATE_NO_WINDOW);check('running fixture install succeeds',p.returncode==0)
 before=tree(target);child=subprocess.Popen([str(target/'ImperialWingsLauncher.exe')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
 try:
  time.sleep(.4);check('harmless installed process is actually alive',child.poll() is None)
  result=out/'running-update.json';p=subprocess.run([str(ROOT/'fixtures/Setup-new-fixture.exe'),'--install',str(target),'--mode','update','--quiet','--no-register','--result',str(result)],timeout=25,creationflags=subprocess.CREATE_NO_WINDOW)
  d=json.loads(result.read_text(encoding='utf-8-sig'));runs.append(dict(name='running-update',exit_code=p.returncode,result=d));check('update refuses actual running installed process',p.returncode!=0 and not d['ok']);check('running update refusal preserves original file tree',tree(target)==before)
 finally:check('harmless process exits naturally without forced termination',child.wait(timeout=15)==0)
 check('attack fixtures leave no staging remnants',not any(p.name.startswith('.iw-stage-') for p in out.iterdir()));save();print(out/'summary.json',flush=True)
 return 0 if all(c['passed'] for c in checks) else 1
if __name__=='__main__':raise SystemExit(main())
