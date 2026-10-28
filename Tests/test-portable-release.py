"""Verify a relocated release with developer tools removed from the child environment."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import urllib.request
import uuid


def verify_feedback_form(root, manifest, source, hashed_paths):
    """Require the reviewed form even if it was omitted from the checksum list."""
    relative='feedback-form.html'
    info=manifest.get('feedback_form',{})
    if info.get('path')!=relative or hashed_paths.count(relative)!=1:
        raise AssertionError('Feedback form missing from release metadata or checksums')
    bundled=root/relative
    if not bundled.is_file() or not source.is_file():
        raise AssertionError('Bundled feedback form or reviewed source is missing')
    with source.open('rb') as f:expected=hashlib.file_digest(f,'sha256').hexdigest()
    with bundled.open('rb') as f:actual=hashlib.file_digest(f,'sha256').hexdigest()
    if actual!=expected or info.get('sha256')!=expected:
        raise AssertionError('Bundled feedback form differs from reviewed source')


def run(args):
    root=args.bundle.resolve();output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    clean=os.environ.copy()
    for name in ('PYTHONHOME','PYTHONPATH','VIRTUAL_ENV','SDACS_REPO','IMPERIAL_UE_ROOT','IMPERIAL_PYTHON'):
        clean.pop(name,None)
    clean['PATH']=os.path.join(os.environ['WINDIR'],'System32')+os.pathsep+os.environ['WINDIR']
    checks=[]
    def check(name,condition):
        checks.append(dict(name=name,passed=bool(condition)))
        (output/'result.json').write_text(json.dumps(dict(ok=False,completed=False,checks=checks),indent=2),encoding='utf-8')
        if not condition:raise AssertionError(name)
        print('PASS '+name,flush=True)
    manifest=json.loads((root/'release-manifest.json').read_text(encoding='utf-8-sig'))
    hashed_paths=[]
    for line in (root/'SHA256SUMS.txt').read_text(encoding='utf-8').splitlines():
        expected,relative=line.split('  ',1)
        hashed_paths.append(relative)
        path=(root/relative).resolve()
        if not path.is_relative_to(root):raise AssertionError('Manifest traversal')
        with path.open('rb') as f:actual=hashlib.file_digest(f,'sha256').hexdigest()
        if actual!=expected:raise AssertionError('Damaged release file '+relative)
    check('all released file hashes match',True)
    verify_feedback_form(root,manifest,args.feedback_source.resolve(),hashed_paths)
    check('standalone feedback form exists, is checksummed and matches reviewed source',True)
    check('no pilot settings or generated Saved directory',not any(p.name.casefold() in {'radioprofile.json','pilotcontrols.json','pilotresponse.json'} or p.name=='Saved' for p in root.rglob('*')))
    inspection=output/'launcher-inspection.json'
    p=subprocess.run([str(root/'ImperialWingsLauncher.exe'),'--inspect',str(inspection)],env=clean,cwd=str(output),timeout=15)
    state=json.loads(inspection.read_text(encoding='utf-8-sig'))
    check('native launcher resolves relocated files without developer tools',p.returncode==0 and state['game_exists'] and state['bridge_exists'] and not state['requires_installed_python'])
    ready=output/'guidance-ready.json'
    with (output/'guidance.log').open('w',encoding='utf-8') as log:
        # Explicit bounded ephemeral test server, not a persistent service.
        p=subprocess.Popen([str(root/'Guidance/ImperialGuidance.exe'),'--port','0','--ready-file',str(ready),'--test-lifetime','6'],env=clean,cwd=str(output),stdout=log,stderr=log)
        try:
            deadline=time.monotonic()+20
            while not ready.exists() and p.poll() is None and time.monotonic()<deadline:time.sleep(.1)
            check('frozen APF starts with no installed Python or Git on PATH',ready.exists())
            data=json.loads(ready.read_text(encoding='utf-8'))
            health=json.loads(urllib.request.urlopen(data['url']+'/health',timeout=3).read())
            check('frozen APF provenance matches released upstream',health['source_commit']==manifest['upstream']['commit'] and health['apf_sha256']==manifest['upstream']['files']['simulation/apf_engine/apf.py'])
            lanes=[0,-1.25,-2.5,1.25,2.5]
            drones=[dict(id=i,position_m=[-6.5 if i<5 else 6.5,lanes[i%5]*(1 if i<5 else -1),.35],velocity_mps=[0,0,0],home_m=[-6.5 if i<5 else 6.5,lanes[i%5]*(1 if i<5 else -1),.35]) for i in range(10)]
            payload=dict(protocol_version=2,session_id=str(uuid.uuid4()),command_id=1,request_id=1,dt=.1,mode='mission',drones=drones)
            request=urllib.request.Request(data['url']+'/step',data=json.dumps(payload).encode(),headers={'Content-Type':'application/json'})
            reply=json.loads(urllib.request.urlopen(request,timeout=3).read())
            check('actual bundled APF responds to ten-aircraft protocol 2',reply['source']=='swarm-drone-atc' and len(reply['targets'])==10 and reply['request_id']==1)
            check('bounded APF test exits cleanly',p.wait(timeout=12)==0)
        finally:
            if p.poll() is None:p.terminate();p.wait(timeout=10)
    binary=next((root/'Windows/ImperialWingsSimulator/Binaries/Win64').glob('ImperialWingsSimulator*-Shipping.exe'),None)
    if binary is None:binary=root/'Windows/ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator.exe'
    def storage(label,profile=None):
        data=output/(label+' 사용자 데이터');data.mkdir()
        if profile:
            (data/'Config').mkdir();shutil.copy2(profile,data/'Config/RadioProfile.json')
        result=output/(label+'-storage.json')
        command=[str(binary),'-nullrhi','-nosound','-unattended','-nosplash','-ImperialStorageSmoke','-ImperialDataDir='+str(data),'-ImperialStorageResult='+str(result),'-abslog='+str(output/(label+'-game.log'))]
        if profile:command.append('-ImperialStorageExpectSaved')
        p=subprocess.run(command,env=clean,cwd=str(output),timeout=60)
        check(label+' standalone Unreal storage result',p.returncode==0 and result.exists())
        r=json.loads(result.read_text(encoding='utf-8-sig'))
        check(label+' keyboard startup and profile safety',r['ok'] and r['ended_disabled'] and r['source_sha256']==manifest['build']['source_sha256'])
        check(label+' writes only to explicit per-user data path',Path(r['data_directory']).resolve()==data.resolve() and (data/'Diagnostics').is_dir())
        return data
    fresh=storage('fresh')
    check('first use never invents a controller profile',not (fresh/'Config/RadioProfile.json').exists())
    if args.real_profile:
        # Genuine existing local calibration, isolated QA only; never copied to the release.
        storage('existing-real-profile',args.real_profile.resolve())
    check('package remains free of generated pilot data after execution',not any(p.name.casefold() in {'radioprofile.json','pilotcontrols.json','pilotresponse.json'} or p.name=='Saved' for p in root.rglob('*')))
    expected=16 if args.real_profile else 13
    assert len(checks)==expected and all(c['passed'] for c in checks),'Incomplete portable verification'
    (output/'result.json').write_text(json.dumps(dict(ok=True,completed=True,expected_check_count=expected,checks=checks,build=manifest['build']),indent=2),encoding='utf-8')
    print(str(output/'result.json'))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--real-profile',type=Path)
    parser.add_argument('--feedback-source',type=Path,default=Path(__file__).resolve().parents[1]/'docs'/'feedback-form.html',
                        help='Reviewed original HTML used to check the bundled feedback form')
    args=parser.parse_args()
    output_preexisting=args.output.exists()
    try:
        run(args)
    except Exception as error:
        # Preserve previous test runs; mark only this fresh run as failed.
        if not output_preexisting and args.output.is_dir():
            result=args.output/'result.json'
            try: state=json.loads(result.read_text(encoding='utf-8')) if result.exists() else {}
            except (ValueError,OSError): state={}
            state.update(ok=False,completed=False,failure=str(error))
            result.write_text(json.dumps(state,indent=2),encoding='utf-8')
        raise
