"""Settings preservation regression using isolated synthetic files and packaged UE binary."""
import argparse, ctypes, hashlib, json, subprocess, uuid
from ctypes import wintypes
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,required=True);p.add_argument('--expected-source',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=False)
response={'version':1,'arm_button':-1,'arm_device':'','arm_toggle':True,'expo_0':0.1,'expo_1':0.0,'expo_2':0.0,'rate_0':180,'rate_1':180,'rate_2':90,'future_meta':{'nested':[1,'keep']}}
controls={'version':1,'precision_throttle':False,'future_meta':{'nested':[1,'keep']}}
cases=['missing','valid_unknown','future_version','invalid_schema','malformed','directory','unreadable']
k=ctypes.WinDLL('kernel32',use_last_error=True);k.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE];k.CreateFileW.restype=wintypes.HANDLE;k.CloseHandle.argtypes=[wintypes.HANDLE]
results=[]
for name in cases:
 out=a.output/name; config=out/'data'/'Config';config.mkdir(parents=True); paths=[config/'PilotResponse.json',config/'PilotControls.json']; should_save=name in ('missing','valid_unknown'); handles=[]; before=[]
 if name=='directory':
  for path in paths:path.mkdir()
 elif name!='missing':
  for path,base in zip(paths,(response,controls)):
   doc=dict(base)
   if name=='future_version':doc['version']=99
   if name=='invalid_schema':doc.pop('arm_toggle' if path==paths[0] else 'precision_throttle')
   path.write_text('{broken' if name=='malformed' else json.dumps(doc,ensure_ascii=False),encoding='utf8')
   before.append(hashlib.sha256(path.read_bytes()).hexdigest())
 if name=='unreadable':
  for path in paths:
   h=k.CreateFileW(str(path),0x80000000,0,None,3,0,None)
   if h in (None,ctypes.c_void_p(-1).value):raise OSError(ctypes.get_last_error(),'exclusive file fixture')
   handles.append(h)
 try:
  args=[str(a.exe),'-game','-nullrhi','-nosound','-nosplash','-unattended','-ImperialNoWelcome','-ImperialBridgeUrl=http://127.0.0.1:1','-ImperialDataDir='+str(out/'data'),'-UserDir='+str(out/'engine'),'-ImperialResponseSmoke','-ImperialSettingsGuard='+('save' if should_save else 'block'),'-ImperialResponseResult='+str(out/'result.json'),'-abslog='+str(out/'runtime.log')]
  with (out/'console.log').open('w') as log:r=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
 finally:
  for h in handles:k.CloseHandle(h)
 if r.returncode:raise RuntimeError(name+' exit '+str(r.returncode))
 result=json.loads((out/'result.json').read_text(encoding='utf-8-sig'))
 if not result.get('ok') or len(result.get('checks',[]))!=6 or not all(x['ok'] for x in result['checks']):raise RuntimeError(name+' verifier failed')
 if result['build']['source_sha256']!=a.expected_source:raise RuntimeError(name+' unexpected source identity')
 if should_save:
  docs=[json.loads(path.read_text(encoding='utf-8-sig')) for path in paths]
  if name=='valid_unknown' and any(d.get('future_meta')!={'nested':[1,'keep']} for d in docs):raise RuntimeError('unknown fields were lost')
  if docs[0]['expo_0']!=.25 or docs[1]['precision_throttle'] is not True:raise RuntimeError('requested values not saved')
 elif name=='directory':
  if not all(path.is_dir() and not any(path.iterdir()) for path in paths):raise RuntimeError('directory modified')
 elif [hashlib.sha256(path.read_bytes()).hexdigest() for path in paths]!=before:raise RuntimeError(name+' original bytes changed')
 results.append({'case':name,'checks':len(result['checks']),'exit_code':r.returncode,'result':str(out/'result.json')});print('PASS',name,flush=True)
(a.output/'summary.json').write_text(json.dumps({'ok':True,'source_sha256':a.expected_source,'cases':results,'scope':'Isolated synthetic settings; no real USB or physical flight; older or non-cooperating writers do not honor the lock.'},indent=2),encoding='utf-8')
print(a.output/'summary.json')