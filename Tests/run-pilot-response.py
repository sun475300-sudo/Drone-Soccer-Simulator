"""Isolated native HUD/settings and actual ANGLE motor path; never injects HID data."""
import argparse
import copy
import ctypes
from ctypes import wintypes
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import subprocess
import uuid

ROOT=Path(__file__).resolve().parents[1]
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def require(ok,why):
    if not ok:raise ValueError(why)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--packaged',action='store_true');p.add_argument('--package-root',type=Path,default=ROOT/'dist/Portable/Windows')
    p.add_argument('--engine',type=Path,default=Path('E:/Program Files/UE_5.8'))
    p.add_argument('--capture',action='store_true');p.add_argument('--quick',action='store_true')
    a=p.parse_args();out=ROOT/'Saved/Verification'/('pilot-response-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);out.mkdir(parents=True)
    identity=json.loads(subprocess.check_output(['powershell','-NoProfile','-Command','& "'+str(ROOT/'Scripts/Get-SourceIdentity.ps1')+'" | ConvertTo-Json'],cwd=ROOT,text=True,encoding='utf-8-sig'))
    command=[str(a.package_root/'ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe')] if a.packaged else [str(a.engine/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'),str(ROOT/'ImperialWingsSimulator.uproject')]
    cases=[]
    def run(name,case,data):
        data.mkdir(parents=True,exist_ok=True);directory=out/name;directory.mkdir()
        args=['-game','-nosound','-nosplash','-unattended','-ImperialNoWelcome','-ImperialBridgeUrl=http://127.0.0.1:1',
              '-ImperialDataDir='+str(data),'-ImperialResponseSmoke','-ImperialResponseCase='+case,
              '-ImperialResponseResult='+str(directory/'result.json'),'-abslog='+str(directory/'runtime.log')]
        if a.capture:args+=['-RenderOffscreen','-ResX=1600','-ResY=1000','-ForceRes','-ImperialResponseCapture='+str(directory/'response.png')]
        else:args+=['-nullrhi']
        with (directory/'console.log').open('w',encoding='utf-8') as log:
            proc=subprocess.run(command+args,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=90,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        require(proc.returncode==0,'Unreal exit '+str(proc.returncode))
        r=read(directory/'result.json');require(r['ok'] is True and len(r['checks'])>=3 and all(c['ok'] for c in r['checks']),name+' failed: '+str(directory))
        require(r['build']['source_sha256']==identity['source_sha256'],'build mismatch')
        if a.capture:require((directory/'response.png').is_file(),'missing rendered response screen')
        cases.append({'case':name,'checks':r['checks'],'result_path':(directory/'result.json').relative_to(ROOT).as_posix(),'result_sha256':digest(directory/'result.json')})
        print('PASS '+name+' '+str(len(r['checks']))+' checks',flush=True)
    data=out/'fresh-user';run('fresh','fresh',data)
    settings=data/'Config/PilotResponse.json';saved=read(settings)
    require(abs(saved['expo_0']-.2)<1e-8 and abs(saved['expo_2']-.4)<1e-8 and saved['rate_0']==30 and saved['rate_2']==240 and not saved['arm_toggle'],'persisted actual HUD selection')
    before=digest(settings)
    if not a.capture:
        run('reload','reload',data);require(digest(settings)==before,'reload changed saved response')
        if not a.quick:
            for name,locked in [('replace',False),('replacefail',True)]:
                target=out/('user-'+name)/'Config/PilotResponse.json';target.parent.mkdir(parents=True)
                target.write_text(json.dumps(saved),encoding='utf-8');old=digest(target)
                kernel=ctypes.WinDLL('kernel32',use_last_error=True)
                kernel.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
                kernel.CreateFileW.restype=wintypes.HANDLE
                kernel.CloseHandle.argtypes=[wintypes.HANDLE];kernel.CloseHandle.restype=wintypes.BOOL
                handle=None
                if locked:
                    handle=kernel.CreateFileW(str(target),0x80000000,1,None,3,0,None)
                    require(handle not in (None,ctypes.c_void_p(-1).value),'could not lock saved settings for replacement test')
                try:
                    run(name,name,target.parent.parent)
                    if locked:require(digest(target)==old,'failed replacement lost original saved settings')
                    else:require(read(target)['expo_0']==.25,'successful replacement not persisted')
                finally:
                    if handle is not None:kernel.CloseHandle(handle)
            broken={
                'unsupported_version':lambda d:d.update(version=99),
                'string_expo':lambda d:d.update(expo_0='0.2'),
                'invalid_rate':lambda d:d.update(rate_1=0),
                'invalid_expo':lambda d:d.update(expo_2=1),
                'fractional_button':lambda d:d.update(arm_button=.5,arm_device='fixture'),
                'unbound_button':lambda d:d.update(arm_button=0,arm_device=''),
                'bad_bool':lambda d:d.update(arm_toggle='false'),
                'missing_axis':lambda d:d.pop('rate_0'),
            }
            for name,alter in broken.items():
                target=out/('user-'+name)/'Config/PilotResponse.json';target.parent.mkdir(parents=True)
                fixture=copy.deepcopy(saved);alter(fixture);target.write_text(json.dumps(fixture),encoding='utf-8');old=digest(target)
                run(name,'invalid',target.parent.parent);require(digest(target)==old,'invalid settings overwritten')
            target=out/'write-blocked-user/Config/PilotResponse.json';target.mkdir(parents=True)
            run('save_failure','savefail',target.parent.parent)
    (out/'results.json').write_text(json.dumps({'ok':True,'build':identity,'cases':cases,'scope':'Synthetic native menu/settings and actual ANGLE target/motor force checks, no real HID button or calibration.'},indent=2)+'\n',encoding='utf-8')
    print(out/'results.json',flush=True)

if __name__=='__main__':main()
