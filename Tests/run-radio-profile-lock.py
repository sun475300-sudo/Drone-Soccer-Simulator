"""Cross-process Windows sidecar lock regression; synthetic QA files only.

The external holder uses Win32 CreateFileW; the contender is the actual
Shipping/Editor Unreal FRadioProfileWriteLock. No HID input is fabricated.
"""
import argparse,ctypes,ctypes.wintypes as W,hashlib,json,os,subprocess,sys,time,uuid
from datetime import datetime,timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def write(path,value):path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def digest(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def worker(lock,ready):
 kernel=ctypes.WinDLL('kernel32',use_last_error=True)
 kernel.CreateFileW.argtypes=[W.LPCWSTR,W.DWORD,W.DWORD,ctypes.c_void_p,W.DWORD,W.DWORD,W.HANDLE]
 kernel.CreateFileW.restype=W.HANDLE
 kernel.CloseHandle.argtypes=[W.HANDLE];kernel.CloseHandle.restype=W.BOOL
 handle=kernel.CreateFileW(str(lock),0xC0000000,0,None,4,0x00200080,None) # OPEN_ALWAYS, share=0, OPEN_REPARSE_POINT
 if handle in (None,ctypes.c_void_p(-1).value):
  write(ready,{'ok':False,'winerror':ctypes.get_last_error()});return 1
 write(ready,{'ok':True,'pid':os.getpid()})
 try:sys.stdin.readline()
 finally:kernel.CloseHandle(handle)
 return 0
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--runtime-path',type=Path,required=True)
 args=parser.parse_args()
 out=ROOT/'Saved/Verification'/('radio-profile-lock-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);out.mkdir(parents=True,exist_ok=False)
 state={'ok':False,'completed':False,'scope':'Isolated synthetic profile; external Python OS holder, actual Unreal lock contender; no HID/older-version/external-writer guarantee.'}
 def record():write(out/'summary.json',state)
 data=out/'user';folder=data/'Verification/RadioProfileStructure';folder.mkdir(parents=True)
 target=folder/'external.json';target.write_text('{"version":1,"profiles":{"existing":{"sentinel":"keep"}},"unknown":"retain"}\n',encoding='utf-8');before=digest(target)
 ready=out/'holder-ready.json';exe=args.runtime_path.resolve()
 expected=json.loads(subprocess.check_output(['powershell','-NoProfile','-Command',"[Console]::OutputEncoding=[Text.Encoding]::UTF8; & '"+str(ROOT/'Scripts/Get-SourceIdentity.ps1')+"' | ConvertTo-Json -Compress"],encoding='utf-8-sig'))
 assert exe.is_file()
 state.update(source=expected,executable=str(exe),target_sha256_before=before);record()
 holder=subprocess.Popen([sys.executable,str(Path(__file__).resolve()),'--worker',str(target)+'.lock',str(ready)],stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
 try:
  deadline=time.monotonic()+15
  while not ready.exists() and holder.poll() is None and time.monotonic()<deadline:time.sleep(.05)
  assert ready.exists() and read(ready)['ok'] and holder.poll() is None,'isolated lock holder did not start'
  state['holder_pid']=holder.pid;state['holder_ready']=True;record()
  def unreal(label,flag):
   result=out/(label+'.json');userdir=out/(label+'-engine-user')
   cmd=[str(exe),'-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialNoWelcome','-ImperialBridgeUrl=http://127.0.0.1:1','-ImperialRadioUXSmoke','-ImperialRadioLockExternal'+flag,'-ImperialRadioUXResult='+str(result),'-ImperialDataDir='+str(data),'-UserDir='+str(userdir),'-abslog='+str(out/(label+'.log'))]
   with (out/(label+'-console.log')).open('w',encoding='utf-8') as log:
    process=subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=120,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
   assert process.returncode==0 and result.exists(),label+' process/result failure'
   value=read(result);checks=value.get('checks',[])
   assert value.get('ok') is True and value['build']['source_sha256']==expected['source_sha256'] and all(c.get('ok') is True for c in checks)
   assert sum(c.get('check')=='cross-process radio lock matches external holder state' for c in checks)==1
   assert digest(target)==before,'fixture profile was modified'
   state[label]={'exit_code':process.returncode,'checks':len(checks),'result':str(result),'target_preserved':True};record()
  unreal('busy','Busy')
  assert holder.poll() is None,'external holder exited prematurely'
  holder.kill();holder.wait(timeout=10);state['known_qa_holder_terminated']=True;state['holder_exit_code']=holder.returncode;record()
  language=data/'Config/Language.txt';archive=data/'Config/Language-before-retry.txt'
  assert language.is_file() and not archive.exists()
  language.rename(archive) # preserve only this dedicated QA process's first language output
  response=data/'Config/PilotResponse.json';response_archive=out/'first-run-PilotResponse.json'
  assert response.is_file() and not response_archive.exists()
  response.rename(response_archive) # the second full UX fixture requires default response on its fresh start
  unreal('after_holder_termination','Available')
  assert digest(target)==before and not (data/'Config/RadioProfile.json').exists()
  state.update(ok=True,completed=True,target_sha256_after=digest(target),same_process_merge_verified_by_native_fixture=True);record()
  print('PASS actual Unreal rejection while separate process holds lock and reacquisition after holder termination',flush=True)
  print(out/'summary.json',flush=True);return 0
 except BaseException as e:
  state['error']=str(e);record();print('FAIL',e,'evidence',out,file=sys.stderr,flush=True);return 1
 finally:
  if holder.poll() is None:
   holder.kill();holder.wait(timeout=10)
if __name__=='__main__':
 if len(sys.argv)==4 and sys.argv[1]=='--worker':
  sys.exit(worker(Path(sys.argv[2]),Path(sys.argv[3])))
 sys.exit(main())
