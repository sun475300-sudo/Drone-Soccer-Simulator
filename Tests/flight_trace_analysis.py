"""Independent checks of every recorded game-physics-frame, including reset epochs."""
import csv
import math
from itertools import zip_longest

FIELDS=('frame world_s game_dt_s id flight_model_s physics_step_seq sensor_model_s reset_epoch '
        'x_m y_m z_m vx_mps vy_mps vz_mps qx qy qz qw wx_radps wy_radps wz_radps '
        'armed autonomous raw_throttle throttle_command current_a consumed_ah battery_soc '
        'battery_voltage_v delivered_power_w motor_applied_power_w motor_available_power_w').split()
INTEGERS={'frame','id','physics_step_seq','reset_epoch','armed','autonomous'}


def require(ok,message):
    if not ok:raise ValueError(message)


def frames(path):
    with path.open(encoding='utf-8-sig',newline='') as stream:
        reader=csv.DictReader(stream)
        require(reader.fieldnames==FIELDS,'Unexpected trace columns')
        group=[];frame=0
        for line,record in enumerate(reader,2):
            require(None not in record and all(value is not None for value in record.values()),f'Malformed CSV line {line}')
            row={}
            for key,value in record.items():
                try:parsed=int(value) if key in INTEGERS else float(value)
                except (ValueError,TypeError):raise ValueError(f'Invalid {key} on line {line}')
                require(math.isfinite(parsed),f'Nonfinite {key} on line {line}')
                row[key]=parsed
            require(row['frame']==frame and row['id']==len(group),f'Missing, duplicated or reordered frame/body at line {line}')
            require(row['armed'] in (0,1) and row['autonomous'] in (0,1),'Invalid boolean')
            require(row['reset_epoch']>=0 and row['physics_step_seq']>=0,'Invalid epoch/step sequence')
            norm=sum(row[key]**2 for key in ('qx','qy','qz','qw'))
            require(abs(norm-1)<1e-5,'Quaternion not normalized')
            if group:
                require(row['world_s']==group[0]['world_s'] and row['game_dt_s']==group[0]['game_dt_s'],'Frame clocks disagree between bodies')
            group.append(row)
            if len(group)==10:
                yield group
                frame+=1;group=[]
        require(not group and frame>1,'Incomplete or empty final frame')


def analyze(path,summary,rate,source_hash):
    require(summary['ok'] and summary['build']['source_sha256']==source_hash,'Trace summary/build did not pass: '+summary.get('error',''))
    require(summary['first_sample_is_reference'] and summary['initial_interval_omitted'],'Reference interval contract missing')
    require(summary['completed_by_smoke'] and summary['smoke_completion_elapsed_s']>=43,'Flight did not reach completion')
    require(summary['force_path_changed'] is False and summary['finite_bodies'],'Invalid trace scope')
    require(summary['bodies']==10 and len(summary['per_body'])==10,'Missing body summaries')
    require(summary['requested_fps']==rate,'Summary requested FPS mismatch')
    require(summary['completion_frame_covered'] and summary['charge_integrals_match'] and summary['expected_resets_observed'],'Trace failed coverage or reset reconciliation')
    require(abs(summary['actual_min_game_dt_s']-1/rate)<1e-5 and abs(summary['actual_max_game_dt_s']-1/rate)<1e-5,'Summary game intervals mismatch')
    totals=[dict(id=i,reset_count=0,current_ah=0.,consumed_ah=0.,delivered_j=0.,vi_j=0.,motor_j=0.,max_charge_interval_error_ah=0.,max_soc_interval_error=0.) for i in range(10)]
    previous=None;first=None;count=0
    for group in frames(path):
        if first is None:first=group
        for row in group:
            i=row['id'];stats=totals[i]
            for key in ('current_a','consumed_ah','battery_voltage_v','delivered_power_w','motor_applied_power_w','motor_available_power_w'):
                require(row[key]>=0,f'Negative {key}')
            require(0<=row['battery_soc']<=1,'SOC outside bounds')
            require(0<=row['raw_throttle']<=1 and 0<=row['throttle_command']<=1,'Throttle outside bounds')
            require(abs(row['delivered_power_w']-row['current_a']*row['battery_voltage_v'])<1e-8,'Delivered power differs from V*I')
            # Actual applied forces are stored as float motor thrusts, whereas
            # the supply budget is double. P ~ T^1.5 magnifies float rounding;
            # allow a conservative analytic bound of 3 half-ULPs, not a tuned
            # physical power allowance. This is about 0.000057 W at 320 W.
            power_roundoff=1e-8+3*2**-24*max(row['motor_applied_power_w'],row['motor_available_power_w'])
            require(row['motor_applied_power_w']<=row['motor_available_power_w']+power_roundoff,'Applied motor power exceeds supply beyond float-thrust rounding')
            if previous is None:continue
            old=previous[i]
            dt=row['flight_model_s']-old['flight_model_s']
            require(row['physics_step_seq']==old['physics_step_seq']+1,'Not exactly one control/model update per recorded frame')
            require(abs(dt-1/rate)<1e-5 and dt>0,'Model interval does not match controlled frame')
            require(abs(row['sensor_model_s']-old['sensor_model_s']-dt)<1e-8,'Sensor/model time mismatch')
            require(abs(row['world_s']-old['world_s']-dt)<1e-5,'World/model interval mismatch')
            require(abs(row['game_dt_s']-1/rate)<1e-5,'Game Tick interval mismatch')
            epoch_delta=row['reset_epoch']-old['reset_epoch']
            require(epoch_delta in (0,1),'Reset epoch lost, reversed or jumped')
            used=row['consumed_ah']-(old['consumed_ah'] if epoch_delta==0 else 0.)
            integrated=row['current_a']*dt/3600
            charge_error=abs(used-integrated)
            require(charge_error<1e-10,'Per-frame charge integral mismatch')
            metadata=summary['per_body'][i]
            require(metadata['id']==i and metadata['battery_model_enabled'],'Unexpected battery profile')
            capacity=metadata['battery_capacity_ah']
            require(math.isfinite(capacity) and capacity>0,'Invalid capacity')
            old_soc=old['battery_soc'] if epoch_delta==0 else 1.
            soc_error=abs(old_soc-row['battery_soc']-integrated/capacity)
            require(soc_error<1e-10,'Per-frame SOC integral mismatch')
            stats['reset_count']+=epoch_delta
            stats['current_ah']+=integrated;stats['consumed_ah']+=used
            stats['delivered_j']+=row['delivered_power_w']*dt
            stats['vi_j']+=row['battery_voltage_v']*row['current_a']*dt
            stats['motor_j']+=row['motor_applied_power_w']*dt
            stats['max_charge_interval_error_ah']=max(stats['max_charge_interval_error_ah'],charge_error)
            stats['max_soc_interval_error']=max(stats['max_soc_interval_error'],soc_error)
        previous=group;count+=1
    require(count==summary['frames'] and count*10==summary['rows'],'Frame/row count differs from finalized summary')
    require(first[0]['world_s']<1 and previous[0]['world_s']-first[0]['world_s']>=42,'Trace misses flight window')
    require(abs(first[0]['world_s']-summary['window_start_world_s'])<1e-8,'Start endpoint mismatch')
    require(abs(previous[0]['world_s']-summary['window_end_world_s'])<1e-8,'End endpoint mismatch')
    require(previous[0]['world_s']+1e-8>=summary['completion_signal_world_s'],'Missing final completion frame')
    for i,stats in enumerate(totals):
        require(stats['reset_count']==(3 if i==0 else 0),'Expected three striker resets and no other resets')
        require(stats['current_ah']>0 and stats['delivered_j']>0,'Missing airborne/avionics consumption')
        require(abs(stats['current_ah']-stats['consumed_ah'])<1e-10,'Total charge integral mismatch')
        require(abs(stats['delivered_j']-stats['vi_j'])<1e-7,'Total V*I energy mismatch')
        reported=summary['per_body'][i]
        require(reported['reset_count']==stats['reset_count'] and reported['first_reset_epoch']==first[i]['reset_epoch'] and reported['final_reset_epoch']==previous[i]['reset_epoch'],'Summary reset history differs')
        require(reported['final_physics_step_seq']==previous[i]['physics_step_seq'],'Summary physics step sequence differs')
        for key,other,tolerance in [('current_ah','integrated_current_ah',1e-10),('consumed_ah','observed_consumed_ah',1e-10),('delivered_j','integrated_delivered_j',1e-7),('motor_j','integrated_motor_applied_j',1e-7)]:
            require(math.isfinite(reported[other]) and abs(stats[key]-reported[other])<tolerance,f'Summary integral differs: {key}')
    return {'ok':True,'frames':count,'rows':count*10,'window_start_s':first[0]['world_s'],'window_end_s':previous[0]['world_s'],'per_body':totals}


def compare(left,right):
    maxima=dict(position_m=0.,velocity_mps=0.,orientation_rad=0.,angular_velocity_radps=0.,current_a=0.,consumed_ah=0.,soc=0.,voltage_v=0.,power_w=0.,motor_applied_power_w=0.,motor_available_power_w=0.)
    count=0
    for baseline,observed in zip_longest(frames(left),frames(right)):
        require(baseline is not None and observed is not None,'Trace lengths differ')
        for a,b in zip(baseline,observed):
            for key in ('frame','id','physics_step_seq','reset_epoch','armed','autonomous'):
                require(a[key]==b[key],f'Control/lifetime difference: {key}')
            for key in ('world_s','flight_model_s','sensor_model_s','raw_throttle','throttle_command'):
                require(abs(a[key]-b[key])<1e-8,f'Input/clock difference: {key}')
            for label,keys in [('position_m',('x_m','y_m','z_m')),('velocity_mps',('vx_mps','vy_mps','vz_mps')),('angular_velocity_radps',('wx_radps','wy_radps','wz_radps'))]:
                maxima[label]=max(maxima[label],math.sqrt(sum((a[key]-b[key])**2 for key in keys)))
            quaternion=('qx','qy','qz','qw')
            dot=abs(sum(a[key]*b[key] for key in quaternion))/math.sqrt(sum(a[key]**2 for key in quaternion)*sum(b[key]**2 for key in quaternion))
            maxima['orientation_rad']=max(maxima['orientation_rad'],2*math.acos(min(1.,dot)))
            for label,key in [('current_a','current_a'),('consumed_ah','consumed_ah'),('soc','battery_soc'),('voltage_v','battery_voltage_v'),('power_w','delivered_power_w'),('motor_applied_power_w','motor_applied_power_w'),('motor_available_power_w','motor_available_power_w')]:
                maxima[label]=max(maxima[label],abs(a[key]-b[key]))
        count+=1
    limits=dict(position_m=1e-4,velocity_mps=1e-4,orientation_rad=1e-4,angular_velocity_radps=1e-3,current_a=1e-3,consumed_ah=1e-8,soc=1e-8,voltage_v=1e-4,power_w=.02,motor_applied_power_w=.02,motor_available_power_w=.02)
    for name,maximum in maxima.items():require(maximum<=limits[name],f'Observer changed {name}: {maximum} > {limits[name]}')
    return {'ok':True,'frames':count,'maxima':maxima,'limits':limits}


def validate_observer(probe,trace,source):
    require(isinstance(probe,dict) and probe.get('ok') is True,'Missing or failed observer result')
    require(probe['build']['source_sha256']==source,'Observer build differs')
    require(probe['mode']=='sync' and probe['passthrough'] and probe['solver_steps']>0,'Wrong observer mode/count')
    require(probe['observer_covers_completion_tick'] and probe['passthrough_completed_by_smoke'],'Observer did not cover final frame')
    require(abs(probe['observer_end_world_s']-trace['window_end_world_s'])<1e-8,'Trace/probe completion endpoints differ')
