"""Run bounded, isolated input/force bridge fixtures; no controller input or GPU."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import uuid

from solver_bridge_analysis import RATES, analyze, corruption_checks, require

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packaged', action='store_true')
    parser.add_argument('--package-root', type=Path, default=ROOT/'dist/Portable/Windows')
    parser.add_argument('--engine', type=Path, default=Path('E:/Program Files/UE_5.8'))
    parser.add_argument('--rates', type=int, nargs='+', default=list(RATES))
    parser.add_argument('--skip-options', action='store_true')
    args = parser.parse_args()
    if any(rate not in RATES for rate in args.rates):
        parser.error('Unsupported frame rate')
    output = ROOT/'Saved/Verification'/('solver-bridge-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')+'-'+uuid.uuid4().hex[:8])
    output.mkdir(parents=True)
    identity = json.loads(subprocess.check_output(['powershell', '-NoProfile', '-Command',
        '& "'+str(ROOT/'Scripts/Get-SourceIdentity.ps1')+'" | ConvertTo-Json'], cwd=ROOT, text=True, encoding='utf-8-sig'))
    binary = args.package_root/'ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe'
    command = [str(binary)] if args.packaged else [str(args.engine/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'), str(ROOT/'ImperialWingsSimulator.uproject')]
    def run(label, flags):
        directory = output/label
        directory.mkdir()
        argv = command+['-game', '-nullrhi', '-nosound', '-unattended', '-nosplash', '-ImperialNoWelcome',
            '-ImperialBridgeUrl=http://127.0.0.1:1', '-ImperialDataDir='+str(directory/'userdata'),
            '-abslog='+str(directory/'runtime.log'), '-ImperialSolverBridgeSmoke',
            '-ImperialSolverBridgeResult='+str(directory/'result.json')]+flags
        with (directory/'console.log').open('w', encoding='utf-8') as log:
            result = subprocess.run(argv, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                    creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0), timeout=90)
        (directory/'process.json').write_text(json.dumps(dict(exit_code=result.returncode))+'\n', encoding='utf-8')
        require(result.returncode in (0, 1), 'engine failed: '+str(directory))
        return directory, read(directory/'result.json'), result.returncode
    results, rejected, fault_checks = [], [], []
    for rate in args.rates:
        directory, data, exit_code = run('sync-'+str(rate), ['-FPS='+str(rate)])
        require(exit_code == 0, 'positive fixture exit status')
        measurement = analyze(data, rate, identity['source_sha256'])
        results.append(dict(rate=rate, independent=measurement,
                            result_path=(directory/'result.json').relative_to(ROOT).as_posix(),
                            result_sha256=sha(directory/'result.json')))
        if not fault_checks:
            fault_checks = corruption_checks(data, rate, identity['source_sha256'])
        print('PASS sync '+str(rate)+' FPS: '+str(measurement['steps'])+' actual solver steps', flush=True)
    if not args.skip_options:
        for label, flags, expected in (
            ('invalid_fps', ['-FPS=15'], 'FPS'),
            ('conflicting_probe', ['-FPS=60', '-ImperialSolverProbe'], 'conflict'),
            ('conflicting_flight', ['-FPS=60', '-ImperialPhysicsSmoke'], 'conflict'),
            ('conflicting_fixed', ['-FPS=60', '-ImperialSolverProbeFixed'], 'conflict'),
        ):
            directory, data, exit_code = run('reject-'+label, flags)
            require(data['ok'] is False and expected.lower() in data['error'].lower(), 'missing expected rejection: '+label)
            require(data['build']['source_sha256'] == identity['source_sha256'], 'rejection build mismatch')
            rejected.append(dict(case=label, ok=True, error=data['error'], result_sha256=sha(directory/'result.json')))
            print('PASS reject '+label, flush=True)
    summary = dict(ok=True, build=identity, measurements=results, rejected_options=rejected,
                   validator_checks=fault_checks,
                   scope='Isolated synchronous simple sphere input-buffer/force path. Simulated-age leases; no production drone PID migration, fixed120 flight, hardware/USB/GPU/other-PC verification.')
    (output/'results.json').write_text(json.dumps(summary, indent=2)+'\n', encoding='utf-8')
    print(output/'results.json', flush=True)


if __name__ == '__main__':
    main()
