"""Build a relocatable Windows release; never distribute Saved or pilot calibration."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def current_source_identity(root):
    """Match Get-SourceIdentity/UBT input normalization, including this script."""
    extensions = {'.h', '.cpp', '.cs', '.ini', '.json', '.py', '.ps1', '.txt',
                  '.cmd', '.bat', '.uproject', '.uasset', '.umap', '.png'}
    binary = {'.uasset', '.umap', '.png'}
    paths = []
    for folder in ('Source', 'Config', 'Content', 'ArtSource', 'Bridge', 'Scripts'):
        base = root/folder
        if base.is_dir():
            paths.extend(p for p in base.rglob('*') if p.is_file() and p.suffix.lower() in extensions)
    paths.extend(p for p in root.iterdir() if p.is_file() and p.suffix.lower() in {'.uproject', '.cmd'})
    # Ordinal UTF-16 order also agrees with the Windows/C# implementation for non-BMP names.
    paths.sort(key=lambda p: p.relative_to(root).as_posix().encode('utf-16-be', errors='surrogatepass'))
    manifest = []
    for path in paths:
        if path.suffix.lower() in binary:
            content = path.read_bytes()
        else:
            with path.open('r', encoding='utf-8-sig', newline='') as stream:
                content = stream.read().replace('\r\n', '\n').encode('utf-8')
        manifest.append(path.relative_to(root).as_posix()+'\0'+hashlib.sha256(content).hexdigest()+'\n')
    with (root/'Config/DefaultGame.ini').open('r', encoding='utf-8-sig', newline='') as stream:
        settings = stream.read()
    version = re.search(r'(?m)^ProjectVersion=([^\r\n]+)', settings)
    if version is None:
        raise ValueError('Current source ProjectVersion is missing')
    return dict(version=version.group(1).strip(), input_count=len(paths),
                source_sha256=hashlib.sha256(''.join(manifest).encode('utf-8')).hexdigest())


def validated_package_identity(package, source_root):
    """Reject stale/ambiguous staged identities before creating a release."""
    directory = package/'ImperialWingsSimulator/Binaries/Win64'
    identities = list(directory.glob('ImperialBuildIdentity-*.json'))
    if len(identities) != 1:
        raise ValueError('Package must contain exactly one build identity; found '+str(len(identities)))
    identity = json.loads(identities[0].read_text(encoding='utf-8-sig'))
    if (not isinstance(identity, dict)
            or identities[0].name != 'ImperialBuildIdentity-ImperialWingsSimulator.json'
            or identity.get('target') != 'ImperialWingsSimulator'
            or identity.get('platform') != 'Win64'
            or identity.get('configuration') != 'Shipping'
            or not isinstance(identity.get('source_sha256'), str)
            or re.fullmatch(r'[a-f0-9]{64}', identity['source_sha256']) is None
            or not isinstance(identity.get('version'), str) or not identity['version']
            or type(identity.get('input_count')) is not int or identity['input_count'] <= 0
            or not (directory/'ImperialWingsSimulator-Win64-Shipping.exe').is_file()):
        raise ValueError('Invalid Shipping game build identity or executable')
    expected = current_source_identity(source_root)
    if any(identity.get(key) != value for key, value in expected.items()):
        raise ValueError('Packaged build identity does not match current source version, hash and input count')
    return identity


def bundle_feedback_form(source, output):
    """Copy the reviewed standalone form without changing its contents."""
    expected = digest(source)
    destination = output/'feedback-form.html'
    shutil.copy2(source, destination)
    if digest(destination) != expected:
        raise ValueError('Bundled feedback form differs from its source')
    return dict(path=destination.name, sha256=expected)


def make(args):
    package=args.package.resolve()
    output=args.output.resolve()
    repo=args.repo.resolve()
    if output.exists():
        raise ValueError('Release destination already exists; choose a new directory')
    if not (package/'ImperialWingsSimulator.exe').is_file():
        raise ValueError('Package the Unreal game first')
    identity=validated_package_identity(package,ROOT)
    feedback_source=ROOT/'docs'/'feedback-form.html'
    if not feedback_source.is_file():
        raise ValueError('Reviewed standalone feedback form is missing')
    origin=subprocess.check_output(['git','-C',str(repo),'remote','get-url','origin'],text=True).strip()
    commit=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()
    if origin!='https://github.com/sun475300-sudo/swarm-drone-atc.git':
        raise ValueError('Unexpected upstream origin')
    tracked=['simulation/apf_engine/apf.py','LICENSE']
    if subprocess.check_output(['git','-C',str(repo),'status','--porcelain','--',*tracked],text=True).strip():
        raise ValueError('Upstream APF or licence has local changes; do not label it as a reviewed commit')
    work=ROOT/'Saved'/'PortableBuild'/output.name
    work.mkdir(parents=True,exist_ok=False)
    upstream=work/'upstream'
    for relative in tracked:
        target=upstream/relative;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(repo/relative,target)
    provenance=dict(origin=origin,commit=commit,files={p:digest(upstream/p) for p in tracked})
    (upstream/'provenance.json').write_text(json.dumps(provenance,indent=2),encoding='utf-8')
    subprocess.run([sys.executable,'-m','PyInstaller','--noconfirm','--clean','--onedir','--console',
                    '--name','ImperialGuidance','--distpath',str(work/'frozen'),'--workpath',str(work/'build'),
                    '--specpath',str(work),'--paths',str(ROOT/'Bridge'),
                    '--add-data',str(upstream)+os.pathsep+'upstream','--copy-metadata','numpy',
                    str(ROOT/'Bridge'/'portable_bridge.py')],check=True)
    output.mkdir(parents=True)
    def ignored(directory,names):
        return [name for name in names if name in {'Saved','Intermediate','DerivedDataCache'} or name.startswith('Manifest_') or Path(name).suffix.lower() in {'.pdb','.exp','.lib','.log','.target'}]
    shutil.copytree(package,output/'Windows',ignore=ignored)
    shutil.copytree(work/'frozen'/'ImperialGuidance',output/'Guidance')
    prereq=output/'Prerequisites';prereq.mkdir()
    shutil.copy2(args.engine/'Engine'/'Extras'/'Redist'/'en-us'/'vc_redist.x64.exe',prereq/'vc_redist.x64.exe')
    csc=Path(os.environ['WINDIR'])/'Microsoft.NET'/'Framework64'/'v4.0.30319'/'csc.exe'
    subprocess.run([str(csc),'/nologo','/target:winexe','/platform:x64','/optimize+',
                    '/reference:System.Windows.Forms.dll','/reference:System.Web.Extensions.dll',
                    '/out:'+str(output/'ImperialWingsLauncher.exe'),str(ROOT/'Scripts'/'PortableLauncher.cs')],check=True)
    shutil.copy2(ROOT/'docs'/'PORTABLE_QUICK_START.ko.md',output/'START_HERE.ko.md')
    feedback_form=bundle_feedback_form(feedback_source,output)
    shutil.copy2(ROOT/'THIRD_PARTY_NOTICES.md',output/'THIRD_PARTY_NOTICES.md')
    notices=output/'Licenses';notices.mkdir()
    shutil.copy2(repo/'LICENSE',notices/'swarm-drone-atc-MIT.txt')
    shutil.copy2(Path(sys.base_prefix)/'LICENSE.txt',notices/'Python-LICENSE.txt')
    import importlib.metadata as metadata
    for package_name in ['numpy','pyinstaller']:
        distribution=metadata.distribution(package_name)
        for f in distribution.files or []:
            if '.dist-info/licenses/' in str(f).replace('\\','/'):
                relative=Path(str(f).replace('\\','/').split('.dist-info/licenses/',1)[1])
                target=notices/package_name/relative;target.parent.mkdir(parents=True,exist_ok=True)
                shutil.copy2(distribution.locate_file(f),target)
    if validated_package_identity(output/'Windows',ROOT) != identity:
        raise ValueError('Copied package build identity changed during release assembly')
    manifest=dict(version=identity['version'],build=identity,upstream=provenance,
                  platform='Windows x64',data_directory='%LOCALAPPDATA%/ImperialWingsSimulator/Saved',
                  prerequisites='Microsoft Visual C++ x64 14.44 or later; Windows 10/11 x64; DX12 SM6 GPU',
                  portable_bridge=True,private_profiles_included=False,feedback_form=feedback_form)
    (output/'release-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    files=sorted(p for p in output.rglob('*') if p.is_file())
    if any(p.name.casefold() in {'radioprofile.json','pilotcontrols.json','pilotresponse.json'} or 'Saved' in p.relative_to(output).parts for p in files):
        raise ValueError('Private/generated user data found in distribution')
    (output/'SHA256SUMS.txt').write_text(''.join(digest(p)+'  '+p.relative_to(output).as_posix()+'\n' for p in files),encoding='utf-8')
    archive=Path(str(output)+'.zip')
    if archive.exists():raise ValueError('ZIP already exists')
    with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as target:
        for p in sorted(output.rglob('*')):
            if p.is_file():target.write(p,p.relative_to(output.parent))
    archive.with_suffix('.zip.sha256').write_text(digest(archive)+'  '+archive.name+'\n',encoding='utf-8')
    print(json.dumps(dict(directory=str(output),archive=str(archive),bytes=archive.stat().st_size,sha256=digest(archive),files=len(files)+1)))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package',type=Path,required=True)
    parser.add_argument('--repo',type=Path,required=True)
    parser.add_argument('--engine',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    make(parser.parse_args())
