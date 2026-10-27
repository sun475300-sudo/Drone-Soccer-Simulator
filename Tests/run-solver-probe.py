"""Bounded read-only Chaos callback measurements; not a fixed-rate flight controller."""
import argparse
import csv
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    if not __debug__:
        raise RuntimeError('Run without Python optimization; this verification uses assertions.')
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packaged',action='store_true')
    parser.add_argument('--package-root',type=Path,default=ROOT/'dist/Portable/Windows')
    parser.add_argument('--engine',type=Path,default=Path('E:/Program Files/UE_5.8'))
    parser.add_argument('--rates',type=int,nargs='+',default=[20,30,45,60,120,240])
    parser.add_argument('--modes',choices=['sync','fixed120'],nargs='+',default=['sync','fixed120'])
    parser.add_argument('--seconds',type=float,default=8)
    parser.add_argument('--compare-flight',action='store_true')
    args=parser.parse_args()
    if any(rate not in [20,30,45,60,120,240] for rate in args.rates):parser.error('Unsupported rate')
    if not 5<=args.seconds<=10:parser.error('Duration must be 5..10 game seconds')
    output=ROOT/'Saved/Verification'/('solver-probe-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f'))
    output.mkdir(parents=True)
    identity=json.loads(subprocess.check_output(['powershell','-NoProfile','-Command',
        '& "'+str(ROOT/'Scripts/Get-SourceIdentity.ps1')+'" | ConvertTo-Json'],cwd=ROOT,text=True,encoding='utf-8-sig'))
    source=identity['source_sha256']
    binary=args.package_root/'ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe'
    command=[str(binary)] if args.packaged else [str(args.engine/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'),str(ROOT/'ImperialWingsSimulator.uproject')]
    checks=[]
    def run(label,flags,allowed_codes=(0,)):
        directory=output/label;directory.mkdir()
        common=['-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialNoWelcome',
            '-ImperialBridgeUrl=http://127.0.0.1:1','-ImperialDataDir='+str(directory/'userdata'),'-abslog='+str(directory/'runtime.log')]
        with (directory/'console.log').open('w',encoding='utf-8') as log:
            process=subprocess.run(command+common+flags(directory),cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
                creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0),timeout=90)
        (directory/'process.json').write_text(json.dumps({'exit_code':process.returncode})+'\n',encoding='utf-8')
        assert process.returncode in allowed_codes,(label,'engine exit',process.returncode,directory)
        return directory
    def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
    def validate(data,rate,mode):
        assert data['ok'] is True and data['force_path_changed'] is False
        assert data['build']['source_sha256']==source
        assert data['mode']==mode and data['solver_fixed_dt_enabled']==(mode=='fixed120')
        for key in ('actual_min_game_dt_s','actual_max_game_dt_s'):
            assert math.isfinite(data[key]) and abs(data[key]-1/rate)<1e-5,(key,data[key],rate)
        assert data['solver_steps']>0 and data['solver_steps']==data['pre_simulates']==data['pre_integrations']==data['post_integrations']
        assert data['callback_order_errors']==0
        assert data['solver_fixed_flag_stable'] and data['solver_clock_matches_completed_outputs']
        assert data['samples_strictly_positive'] and data['one_integration_per_solver_step']
        assert data['rigid_state_read'] is False
        assert 0<data['solver_min_dt_s']<=data['solver_max_dt_s']<.1
        assert data['solver_elapsed_s']>0
        assert abs(data['game_elapsed_s']-data['game_frames']/rate)<1e-4
        assert data['time_balance_valid'] and abs(data['time_balance_error_s'])<1e-5
        assert data['integration_dt_compatible'] and data['integration_dt_violations']==0
        assert data['solver_integration_settings_stable']
        assert abs(data['solver_elapsed_s']-(data['solver_end_s']-data['solver_start_s']))<1e-5
        remainder_delta=data['solver_accumulated_remainder_s']-data['solver_initial_remainder_s']
        assert abs(data['game_elapsed_s']-data['solver_elapsed_s']-remainder_delta)<1e-5
        assert data['samples'] and len(data['samples'])<=data['samples_limit']
        for i,sample in enumerate(data['samples'],1):
            assert sample['sequence']==i
            assert sample['order']==['pre_simulate','pre_integrate','post_integrate']
            assert sample['pre_integrations']==sample['post_integrations']==1
            assert 0<sample['solver_dt_s']<.1
        if mode=='fixed120':
            assert abs(data['solver_min_dt_s']-1/120)<1e-7 and abs(data['solver_max_dt_s']-1/120)<1e-7
        return True
    rejected=[]
    for label,options in (
        ('reject-invalid-fps',['-FPS=0']),
        ('reject-orphan-passthrough',['-FPS=60','-ImperialSolverProbePassthrough']),
        ('reject-fixed-passthrough',['-FPS=60','-ImperialPhysicsSmoke','-ImperialSolverProbePassthrough','-ImperialSolverProbeFixed']),
        ('reject-armed-fixed',['-FPS=60','-ImperialPhysicsSmoke','-ImperialSolverProbeFixed']),
    ):
        # UE 5.8 Windows graceful exit posts WM_QUIT but the main loop can return
        # zero before consuming its status. Require the explicit rejection JSON;
        # never infer verification success/failure from that exit code alone.
        directory=run(label,lambda d:['-ImperialSolverProbe','-ImperialSolverProbeResult='+str(d/'result.json')]+options,allowed_codes=(0,1))
        data=read(directory/'result.json')
        assert data['ok'] is False and data['error'] and data['build']['source_sha256']==source
        expected_error={
            'reject-invalid-fps':'Use FPS',
            'reject-orphan-passthrough':'Passthrough requires ImperialPhysicsSmoke',
            'reject-fixed-passthrough':'Passthrough is allowed only',
            'reject-armed-fixed':'cannot run with armed or autonomous aircraft',
        }[label]
        assert expected_error in data['error'],(label,data['error'])
        rejected.append({'case':label,'error':data['error'],'ok':True,'process_exit_code':read(directory/'process.json')['exit_code']})
        print('PASS '+label,flush=True)
    for mode in args.modes:
        for rate in args.rates:
            directory=run(f'{mode}-{rate}',lambda d:[
                '-ImperialSolverProbe','-UseFixedTimeStep',f'-FPS={rate}',f'-ImperialSolverProbeSeconds={args.seconds}',
                '-ImperialSolverProbeResult='+str(d/'result.json')]+(['-ImperialSolverProbeFixed'] if mode=='fixed120' else []))
            data=read(directory/'result.json');validate(data,rate,mode)
            assert args.seconds<=data['game_elapsed_s']<args.seconds+1/rate+1e-5
            checks.append({'mode':mode,'rate':rate,'result':data,'evidence':str((directory/'result.json').relative_to(ROOT))})
            print(f'PASS {mode} game {rate} FPS: {data["solver_steps"]} solver steps, dt {data["solver_min_dt_s"]:.9f}..{data["solver_max_dt_s"]:.9f}',flush=True)
    comparison=None
    if args.compare_flight:
        pair=[]
        fleets=[]
        for observed in (False,True):
            def flags(d):
                result=['-ImperialPhysicsSmoke','-UseFixedTimeStep','-FPS=60','-ImperialResult='+str(d/'flight.json')]
                if observed:result+=['-ImperialSolverProbe','-ImperialSolverProbePassthrough','-ImperialSolverProbeResult='+str(d/'probe.json')]
                return result
            directory=run('flight-observed' if observed else 'flight-baseline',flags)
            data=read(directory/'flight.json')
            assert data['ok'] and data['build']['source_sha256']==source
            if observed:
                observer=read(directory/'probe.json');validate(observer,60,'sync')
                assert observer['passthrough_completed_by_smoke'] and observer['observer_covers_completion_tick']
                assert observer['smoke_completion_elapsed_s']>=43
                assert observer['observer_begin_world_s']<1
                assert observer['observer_end_world_s']>=observer['completion_signal_world_s']
                assert 42<observer['game_elapsed_s']<45
            pair.append(data)
            exports=list((directory/'userdata/Reports').glob('*-fleet.csv'))
            assert len(exports)==1
            with exports[0].open(encoding='utf-8-sig',newline='') as stream:
                fleet=list(csv.DictReader(stream))
            assert len(fleet)==10 and len({row['id'] for row in fleet})==10
            assert all(row['build_source_sha256']==source for row in fleet)
            fleets.append({row['id']:row for row in fleet})
        differences={}
        for key in ('hover_error_cm','hover_max_z_cm','hover_min_z_cm','net_peak_m','net_visible_peak_m','net_rebound_mps'):
            differences[key]=abs(pair[1][key]-pair[0][key]);assert differences[key]<.002,(key,differences[key])
        for key in ('striker_physical_goal','blue_score','floor_wall_contact','net_contact','drones'):
            assert pair[0][key]==pair[1][key]
        fleet_differences={}
        for key,tolerance in {'battery_soc':1e-6,'battery_v':1e-5,'x_m':2e-5,'y_m':2e-5,'z_m':2e-5,'speed_mps':2e-5}.items():
            assert all(math.isfinite(float(fleet[identifier][key])) for fleet in fleets for identifier in fleet),(key,'nonfinite fleet value')
            difference=max(abs(float(fleets[0][identifier][key])-float(fleets[1][identifier][key])) for identifier in fleets[0])
            assert difference<=tolerance,(key,difference,tolerance)
            fleet_differences[key]=difference
        comparison={'ok':True,'baseline':pair[0],'observed':pair[1],'differences':differences,
                    'final_fleet_max_differences':fleet_differences,
                    'observer':observer,
                    'scope':'Synchronous read-only counter enabled/disabled; real hover/net/goal outcomes and final ten-body position, speed magnitude, SOC, voltage. Not a fixed-step flight or full energy-integral test.'}
        print('PASS synchronous probe preserves measured hover, contacts and score',flush=True)
    summary={'ok':True,'build':identity,'measurements':checks,'rejected_options':rejected,'flight_comparison':comparison,
             'scope':'Explicit callback counter probe. Production flight controller and default physics settings are unchanged. No physical controller input.'}
    (output/'results.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
    print(output/'results.json')


if __name__=='__main__':main()
