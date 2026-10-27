"""Compare observed/baseline full-frame physics traces; no physical controller input."""
import argparse
import copy
import csv
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

from flight_trace_analysis import analyze, compare, require, validate_observer

ROOT=Path(__file__).resolve().parents[1]


def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))


def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def corruption_checks(csv_path,metadata,rate,source):
    with csv_path.open(encoding='utf-8-sig',newline='') as stream:
        table=list(csv.reader(stream))
    header=table[0]
    def alter(rows,line,key,value):rows[line][header.index(key)]=str(value)
    cases={
        'missing_body_row':lambda rows:rows.pop(19),
        'duplicate_body_row':lambda rows:rows.insert(19,rows[19].copy()),
        'swapped_body_order':lambda rows:rows.__setitem__(slice(18,20),list(reversed(rows[18:20]))),
        'missing_final_frame':lambda rows:rows.__delitem__(slice(-10,None)),
        'nonfirst_body_nan':lambda rows:alter(rows,19,'x_m','nan'),
        'nonfirst_body_infinity':lambda rows:alter(rows,19,'current_a','inf'),
        'lost_reset_epoch':lambda rows:alter(rows,21,'reset_epoch',int(rows[21][header.index('reset_epoch')])+2),
        'missed_physics_update':lambda rows:alter(rows,21,'physics_step_seq',int(rows[21][header.index('physics_step_seq')])+1),
        'charge_error_that_later_cancels':lambda rows:alter(rows,21,'consumed_ah',float(rows[21][header.index('consumed_ah')])+.001),
        'power_voltage_mismatch':lambda rows:alter(rows,21,'delivered_power_w',float(rows[21][header.index('delivered_power_w')])+1),
        'motor_supply_exceeded':lambda rows:alter(rows,21,'motor_applied_power_w',float(rows[21][header.index('motor_applied_power_w')])+1),
    }
    results=[]
    with tempfile.TemporaryDirectory(prefix='trace-corruption-',dir=csv_path.parent) as directory:
        path=Path(directory)/'tampered.csv'
        def save(rows):
            with path.open('w',encoding='utf-8',newline='') as stream:csv.writer(stream).writerows(rows)
        for name,mutate in cases.items():
            rows=copy.deepcopy(table);mutate(rows);save(rows)
            try:analyze(path,metadata,rate,source)
            except ValueError as error:results.append({'case':name,'rejected':True,'reason':str(error)})
            else:raise ValueError('Corruption escaped validation: '+name)
        # Changing a signed velocity leaves a speed-magnitude-only comparison blind.
        rows=copy.deepcopy(table)
        index=next(i for i,row in enumerate(rows[1:],1) if abs(float(row[header.index('vx_mps')]))>.1)
        alter(rows,index,'vx_mps',-float(rows[index][header.index('vx_mps')]))
        save(rows)
        try:compare(csv_path,path)
        except ValueError as error:results.append({'case':'velocity_direction_changed','rejected':True,'reason':str(error)})
        else:raise ValueError('Signed velocity change escaped comparison')
        rows=copy.deepcopy(table)
        index=next(i for i,row in enumerate(rows[1:-10],1) if row[header.index('id')]=='0' and float(row[header.index('motor_applied_power_w')])>1 and float(rows[i+10][header.index('motor_applied_power_w')])>1)
        for line,offset in ((index,.1),(index+10,-.1)):
            for key in ('motor_applied_power_w','motor_available_power_w'):
                alter(rows,line,key,float(rows[line][header.index(key)])+offset)
        save(rows)
        analyze(path,metadata,rate,source) # Equal and opposite changes preserve the final integral.
        try:compare(csv_path,path)
        except ValueError as error:results.append({'case':'motor_power_changes_cancel_in_total','rejected':True,'reason':str(error)})
        else:raise ValueError('Per-frame motor power changes escaped comparison')
        for name,key in [('wrong_final_step','final_physics_step_seq'),('wrong_final_epoch','final_reset_epoch'),('wrong_reset_count','reset_count')]:
            changed=copy.deepcopy(metadata);changed['per_body'][0][key]+=1
            try:analyze(csv_path,changed,rate,source)
            except ValueError as error:results.append({'case':name,'rejected':True,'reason':str(error)})
            else:raise ValueError('Incorrect trace summary escaped validation: '+name)
        try:validate_observer({},metadata,source)
        except ValueError as error:results.append({'case':'empty_observer_result','rejected':True,'reason':str(error)})
        else:raise ValueError('Empty observer result escaped validation')
        # A quaternion and its negation represent the same rotation.
        rows=copy.deepcopy(table)
        for row in rows[1:]:
            for key in ('qx','qy','qz','qw'):row[header.index(key)]=str(-float(row[header.index(key)]))
        save(rows);same=compare(csv_path,path)
        results.append({'case':'equivalent_negated_quaternions','accepted':same['ok']})
    return results


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packaged',action='store_true')
    parser.add_argument('--package-root',type=Path,default=ROOT/'dist/Portable/Windows')
    parser.add_argument('--engine',type=Path,default=Path('E:/Program Files/UE_5.8'))
    parser.add_argument('--rates',type=int,nargs='+',default=[20,30,45,60,120,240])
    parser.add_argument('--corruption-checks',action='store_true')
    args=parser.parse_args()
    if any(rate not in (20,30,45,60,120,240) for rate in args.rates):parser.error('Unsupported game-frame rate')
    identity=json.loads(subprocess.check_output(['powershell','-NoProfile','-Command','& "'+str(ROOT/'Scripts/Get-SourceIdentity.ps1')+'" | ConvertTo-Json'],cwd=ROOT,text=True,encoding='utf-8-sig'))
    source=identity['source_sha256']
    output=ROOT/'Saved/Verification'/('flight-trace-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f'))
    output.mkdir(parents=True)
    binary=args.package_root/'ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe'
    command=[str(binary)] if args.packaged else [str(args.engine/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'),str(ROOT/'ImperialWingsSimulator.uproject')]
    results=[];mutations=[]
    for rate in args.rates:
        pair=[]
        for observed in (False,True):
            directory=output/(f'{rate}-'+('observed' if observed else 'baseline'));directory.mkdir()
            flags=['-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialNoWelcome',
                '-ImperialBridgeUrl=http://127.0.0.1:1','-ImperialDataDir='+str(directory/'userdata'),
                '-abslog='+str(directory/'runtime.log'),'-UseFixedTimeStep',f'-FPS={rate}',
                '-ImperialPhysicsSmoke','-ImperialResult='+str(directory/'flight.json'),
                '-ImperialFlightTrace','-ImperialFlightTraceCsv='+str(directory/'trace.csv'),
                '-ImperialFlightTraceResult='+str(directory/'trace.json')]
            if observed:flags+=['-ImperialSolverProbe','-ImperialSolverProbePassthrough','-ImperialSolverProbeResult='+str(directory/'probe.json')]
            with (directory/'console.log').open('w',encoding='utf-8') as stream:
                process=subprocess.run(command+flags,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,
                    creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0),timeout=150)
            require(process.returncode==0,f'Unreal process failed: {directory}, exit={process.returncode}')
            metadata=read(directory/'trace.json');flight=read(directory/'flight.json')
            require(flight['ok'] and flight['build']['source_sha256']==source,'Physics smoke did not pass')
            require(metadata['solver_observer_enabled']==observed,'Unexpected observer selection')
            require(Path(metadata['csv_path']).resolve()==(directory/'trace.csv').resolve(),'Trace output path mismatch')
            analysis=analyze(directory/'trace.csv',metadata,rate,source)
            probe=read(directory/'probe.json') if observed else None
            if observed:validate_observer(probe,metadata,source)
            pair.append({'directory':directory.relative_to(ROOT).as_posix(),'trace':metadata,'independent':analysis,'flight':flight,
                         'trace_csv_sha256':sha(directory/'trace.csv'),'probe':probe})
            print(f'PASS {rate} FPS {"observed" if observed else "baseline"}: {analysis["rows"]} body samples, charge/energy independently checked',flush=True)
        baseline=ROOT/pair[0]['directory']/'trace.csv';observed=ROOT/pair[1]['directory']/'trace.csv'
        comparison=compare(baseline,observed)
        integral_differences=[]
        for a,b in zip(pair[0]['independent']['per_body'],pair[1]['independent']['per_body']):
            differences={key:abs(a[key]-b[key]) for key in ('current_ah','consumed_ah','delivered_j','motor_j')}
            for key,value in differences.items():require(value<=(1e-8 if key.endswith('_ah') else .01),'Observer changed integrated '+key)
            integral_differences.append({'id':a['id'],**differences})
        results.append({'rate':rate,'baseline':pair[0],'observed':pair[1],'comparison':comparison,'integral_differences':integral_differences})
        print(f'PASS {rate} FPS entire sampled trajectory and integrated power comparison',flush=True)
        if args.corruption_checks and not mutations:
            mutations=corruption_checks(baseline,pair[0]['trace'],rate,source)
            print(f'PASS {len(mutations)} corruption/equivalent-rotation checks',flush=True)
    summary={'ok':True,'build':identity,'pairs':results,'validator_checks':mutations,
             'scope':'Every completed synchronous game-physics-frame after the first reference sample through the 43-game-second fixture. Pairs are compared at the same game FPS, not across FPS. This is not an internal Chaos-substep trajectory, physical controller or GPU performance test.'}
    (output/'results.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
    print(output/'results.json')


if __name__=='__main__':main()
