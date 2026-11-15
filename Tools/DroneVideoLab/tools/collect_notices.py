"""Collect licence notices from the exact build environment; no user files are read."""
from pathlib import Path
import importlib.metadata as metadata
import json
import shutil
import sys

out=Path('licenses');out.mkdir(exist_ok=True)
packages={}
for dist in metadata.distributions():
    name=dist.metadata.get('Name','unknown')
    packages[name]=dist.version
    for entry in dist.files or []:
        leaf=Path(str(entry)).name.lower()
        if 'license' in leaf or 'licence' in leaf or leaf in ('copying','notice','copyright'):
            source=Path(dist.locate_file(entry))
            if source.is_file():
                target=out/name/str(entry).replace('..','_')
                target.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(source,target)
for relative in ('LICENSE.txt','LICENSE','tcl/tcl8.6/license.terms','tcl/tk8.6/license.terms'):
    source=Path(sys.base_prefix)/relative
    if source.is_file():
        target=out/'Python-Tcl-Tk'/relative
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,target)
(out/'BUILD_PACKAGES.json').write_text(json.dumps(packages,sort_keys=True,indent=2),encoding='utf-8')
