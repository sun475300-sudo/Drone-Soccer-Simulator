"""Synthetic installer filesystem fixtures only; never runs Unreal or real HID."""
import argparse,ctypes,hashlib,json,os,subprocess,time,uuid,zipfile
from pathlib import Path
from datetime import datetime,timezone
ROOT=Path(__file__).resolve().parent
RECEIPT='.imperial-wings-install.json'

def digest(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def tree(root):
 root=Path(root)
 if not root.exists():return None
 result={}
 for current,dirs,files in os.walk(root,followlinks=False):
  for d in list(dirs):
   p=Path(current)/d
   if getattr(p.lstat(),'st_file_attributes',0)&0x400:
    result[p.relative_to(root).as_posix()]='REPARSE';dirs.remove(d)
  for name in files:
   p=Path(current)/name;result[p.relative_to(root).as_posix()]=digest(p)
 return result

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--old-installer',type=Path,default=ROOT/'fixtures/Setup-old-fixture.exe')
 ap.add_argument('--new-installer',type=Path,default=ROOT/'fixtures/Setup-new-fixture.exe')
 ap.add_argument('--old-zip',type=Path,default=ROOT/'fixtures/0.0.1-fixture.zip')
 ap.add_argument('--new-zip',type=Path,default=ROOT/'fixtures/0.0.2-fixture.zip')
 ap.add_argument('--build-script',type=Path,default=ROOT/'Build-Installer.ps1')
 ap.add_argument('--output',type=Path)
 args=ap.parse_args()
 out=(args.output or ROOT/'qa'/('harness-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:6])).resolve()
 if not out.is_relative_to((ROOT/'qa').resolve()) or out==(ROOT/'qa').resolve():raise ValueError('Output must be a new child of scratch/qa')
 out.mkdir(parents=True,exist_ok=False);(out/'results').mkdir();checks=[];runs=[];skips=[]
 sentinel=out/'personal-sentinel/Saved/Config/RadioProfile.json';sentinel.parent.mkdir(parents=True);sentinel.write_text('synthetic private calibration sentinel',encoding='utf-8')
 (sentinel.parent/'Language.txt').write_text('ko',encoding='utf-8')
 original=tree(out/'personal-sentinel')
 def save():
  (out/'summary.json').write_text(json.dumps(dict(ok=all(c['passed'] for c in checks),scope='Synthetic installer fixture files only; no real game, controller, second PC, ACL changes or GUI input.',checks=checks,runs=runs,skips=skips,installer_sha256={args.old_installer.name:digest(args.old_installer),args.new_installer.name:digest(args.new_installer)},personal_sentinel_preserved=tree(out/'personal-sentinel')==original),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 def check(name,ok,detail=''):
  checks.append(dict(name=name,passed=bool(ok),detail=detail));save();print(('PASS ' if ok else 'FAIL ')+name,flush=True)
  if not ok:raise AssertionError(name+': '+detail)
 def call(name,installer,action,target,mode=None,extra=(),expect=True):
  result=out/'results'/(name+'.json')
  command=[str(installer),action,str(target),'--quiet','--no-register','--result',str(result)]
  if mode:command+=['--mode',mode]
  command+=list(extra);started=time.monotonic()
  try:
   p=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=25,creationflags=subprocess.CREATE_NO_WINDOW)
  except subprocess.TimeoutExpired:
   runs.append(dict(name=name,timeout=True));check(name+' bounded quiet process',False,'No UI should block a quiet installer')
  data=json.loads(result.read_text(encoding='utf-8-sig')) if result.is_file() else None
  runs.append(dict(name=name,exit_code=p.returncode,result=data,elapsed_s=round(time.monotonic()-started,3)));save()
  check(name+' explicit result',data is not None)
  check(name+' success status' if expect else name+' safe refusal', (p.returncode==0 and data['ok']) if expect else (p.returncode!=0 and not data['ok']))
  return data
 def install_old(name):
  target=out/name;call(name+'-setup',args.old_installer,'--install',target,'fresh');return target
 def payload_matches(target,archive):
  with zipfile.ZipFile(archive) as z:
   files={Path(n).parts[1:]:z.read(n) for n in z.namelist() if not n.endswith('/')}
  return all((target.joinpath(*parts)).is_file() and (target.joinpath(*parts)).read_bytes()==data for parts,data in files.items())
 def scenario(name,fn):
  try:fn()
  except Exception as e:
   if not checks or checks[-1]['passed']:checks.append(dict(name=name,passed=False,detail=repr(e)))
   print('SCENARIO FAILED '+name+' '+repr(e),flush=True);save()
 def fresh():
  target=out/'새 설치 한글 spaces';call('fresh-unicode',args.new_installer,'--install',target,'fresh')
  check('fresh contains complete payload',payload_matches(target,args.new_zip))
  receipt=json.loads((target/RECEIPT).read_text(encoding='utf-8-sig'))
  check('receipt binds product version absolute target',receipt['product']=='ImperialWingsSimulator' and receipt['version']=='0.0.2-fixture' and Path(receipt['target']).resolve()==target.resolve())
  call('verify-fresh',args.new_installer,'--verify',target)
  before=tree(target);call('fresh-occupied',args.new_installer,'--install',target,'fresh',expect=False);check('fresh refusal leaves occupied tree unchanged',tree(target)==before)
 def empty():
  target=out/'empty chosen folder';target.mkdir();call('fresh-empty-folder',args.new_installer,'--install',target,'fresh');check('empty chooser folder gets payload',payload_matches(target,args.new_zip))
 def update():
  target=install_old('managed-update');before=tree(target)
  result=call('managed-update-new',args.new_installer,'--install',target,'update');backup=Path(result['backup'])
  check('successful update retains byte-exact old backup',tree(backup)==before)
  check('updated install contains exact new payload',payload_matches(target,args.new_zip));call('verify-updated',args.new_installer,'--verify',target)
  before=tree(target);same=call('same-version-explicit-update',args.new_installer,'--install',target,'update');check('same-version update backup retained',tree(Path(same['backup']))==before)
 def separate():
  old=install_old('separate-old');before=tree(old);new=out/'separate-new';call('separate-new',args.new_installer,'--install',new,'fresh');check('separate-folder install preserves old installation',tree(old)==before)
 def manual():
  target=out/'manual portable';target.mkdir()
  with zipfile.ZipFile(args.old_zip) as z:
   for info in z.infolist():
    if info.is_dir():continue
    rel=Path(*Path(info.filename).parts[1:]);p=target/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(z.read(info))
  before=tree(target);result=call('manual-clean-update',args.new_installer,'--install',target,'update');check('manual portable backup preserved',tree(Path(result['backup']))==before);check('manual update new payload complete',payload_matches(target,args.new_zip))
 def refused():
  target=out/'unknown folder';target.mkdir();(target/'notes.txt').write_text('retain unknown',encoding='utf-8');before=tree(target)
  call('reject-unknown',args.new_installer,'--install',target,'update',expect=False);check('unknown target preserved',tree(target)==before)
  for name,rel in [('legacy-saved','Saved/Config/RadioProfile.json'),('unknown-extra','notes.txt')]:
   target=install_old(name);p=target/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('preserve this user file',encoding='utf-8');before=tree(target)
   call('reject-'+name,args.new_installer,'--install',target,'update',expect=False);check(name+' tree preserved on refusal',tree(target)==before)
  target=install_old('modified-owned');p=target/'START_HERE.ko.md';p.write_text('user edited guidance',encoding='utf-8');before=tree(target)
  call('reject-modified-owned',args.new_installer,'--install',target,'update',expect=False);check('modified owned file preserved on update refusal',tree(target)==before)
 def rollback():
  target=install_old('rollback-old');before=tree(target)
  result=call('injected-after-backup',args.new_installer,'--install',target,'update',extra=['--test-fail','after-backup','--test-root',str(out)],expect=False)
  check('commit failure explicitly reports restored rollback',result['rollback_restored'])
  check('rollback restores exact original target bytes',tree(target)==before)
  check('rollback has no dangling renamed backup',not Path(result['backup']).exists())
 def locked():
  target=install_old('locked-old');before=tree(target);kernel=ctypes.WinDLL('kernel32',use_last_error=True)
  kernel.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p];kernel.CreateFileW.restype=ctypes.c_void_p;kernel.CloseHandle.argtypes=[ctypes.c_void_p]
  handle=kernel.CreateFileW(str(target/'ImperialWingsLauncher.exe'),0x80000000,1,None,3,0,None)
  check('real Windows deny-delete-sharing lock acquired',handle not in (None,ctypes.c_void_p(-1).value))
  try:call('locked-update-refusal',args.new_installer,'--install',target,'update',expect=False)
  finally:kernel.CloseHandle(handle)
  check('locked original target bytes retained',tree(target)==before)
 def uninstall():
  target=install_old('remove-clean');call('uninstall-clean',args.new_installer,'--uninstall',target);check('clean owned target removed',not target.exists())
  target=install_old('remove-preserve');(target/'notes.txt').write_text('retain extra',encoding='utf-8');(target/'START_HERE.ko.md').write_text('retain edited owned',encoding='utf-8');result=call('uninstall-preserve',args.new_installer,'--uninstall',target)
  check('uninstall preserves unknown and modified files',(target/'notes.txt').read_text(encoding='utf-8')=='retain extra' and (target/'START_HERE.ko.md').read_text(encoding='utf-8')=='retain edited owned')
  check('uninstall reports preserved remnants',set(result['retained_files'])=={'notes.txt','START_HERE.ko.md'})
  check('uninstall removes package executables but not personal data',not (target/'ImperialWingsLauncher.exe').exists() and tree(out/'personal-sentinel')==original)
 def invalid_targets():
  parent=out/'parent-is-file';parent.write_text('retain parent',encoding='utf-8');call('parent-file-refusal',args.new_installer,'--install',parent/'child','fresh',expect=False);check('parent regular file preserved',parent.read_text(encoding='utf-8')=='retain parent')
  for name,path in [('relative-target',Path('relative-install-test')),('drive-root',Path(Path(out).anchor)),('overlong-target',out/('x'*180))]:call(name,args.new_installer,'--install',path,'fresh',expect=False)
  target=install_old('moved-receipt-original');copy=out/'moved-receipt-copy';import shutil;shutil.copytree(target,copy);before=tree(copy);call('wrong-receipt-root',args.new_installer,'--verify',copy,expect=False);check('receipt mismatch is read-only refusal',tree(copy)==before)
 def symlink():
  real=out/'junction-real';real.mkdir();link=out/'junction-link'
  try:os.symlink(real,link,target_is_directory=True)
  except OSError as e:skips.append(dict(name='reparse-target-refusal',reason='Link creation unavailable without permission changes: '+str(e)));save();return
  call('reparse-target-refusal',args.new_installer,'--install',link/'install','fresh',expect=False);check('reparse destination left untouched',not (real/'install').exists())
 for name,fn in [('fresh',fresh),('empty-folder',empty),('update',update),('separate',separate),('manual-portable',manual),('safe-refusals',refused),('rollback',rollback),('file-lock',locked),('uninstall',uninstall),('invalid-paths',invalid_targets),('reparse',symlink)]:scenario(name,fn)
 check('all synthetic personal settings remain byte-identical',tree(out/'personal-sentinel')==original)
 check('no generated staging directories remain',not any(p.name.startswith('.iw-stage-') for p in out.iterdir()))
 save();print(out/'summary.json',flush=True)
 return 0 if all(c['passed'] for c in checks) else 1
if __name__=='__main__':raise SystemExit(main())
