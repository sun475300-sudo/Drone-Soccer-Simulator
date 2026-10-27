"""Independent force/impulse oracle for the isolated Chaos bridge fixture.

The prescribed nine-stage schedule is the oracle, not the callback's applied
flag. This validates simulated-age leases, not hardware or wall-clock latency.
"""
import copy
import math

RATES = (20, 30, 45, 60, 120, 240)
DECISIONS = ('disarmed', 'applied', 'disarmed', 'stale', 'generation',
             'nonfinite', 'sequence', 'applied', 'disarmed')
FORCE = [2.0, -1.0, .5]
OFFSET = [0.0, .05, 0.0]
MASS = 2.0
INERTIA = .4 * MASS * .25 ** 2
VELOCITY_TOLERANCE = 5e-6
TOTAL_TOLERANCE = 5e-5


def require(condition, message):
    if not condition:
        raise ValueError(message)


def finite(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def vector(value, size=3):
    require(isinstance(value, list) and len(value) == size and all(map(finite, value)), 'invalid vector')
    return value


def subtract(a, b):
    return [x-y for x, y in zip(a, b)]


def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def distance(a, b):
    return math.sqrt(sum(x*x for x in subtract(a, b)))


def analyze(data, rate, source):
    require(data['ok'] is True and not data['error'], 'runtime failed')
    require(data['build']['source_sha256'] == source, 'source mismatch')
    require(data['mode'] == 'sync' and data['requested_fps'] == rate and rate in RATES, 'timing mode')
    for key in ('fixture_only', 'normal_drones_disarmed', 'callback_unregistered_before_body_release',
                'callback_deleted_before_body_release'):
        require(data[key] is True, key)
    require(data['post_shutdown_callback_calls'] == 0, 'callback ran after unregister')
    for key in ('coast_velocity_error_mps', 'coast_angular_velocity_error_radps'):
        require(finite(data[key]) and 0 <= data[key] < TOTAL_TOLERANCE, 'force/torque remained after callback teardown')
    require(data['drone_force_path_changed'] is False, 'production flight path changed')
    require(data['bound_generation'] == 1 and data['ttl_s'] == .1, 'fixture safety constants')
    require(data['configured_mass_kg'] == MASS and data['radius_m'] == .25, 'fixture dimensions')
    require(data['command_force_n'] == FORCE and data['command_offset_m'] == OFFSET, 'prescribed force')
    for key in ('game_min_dt_s', 'game_max_dt_s'):
        require(finite(data[key]) and abs(data[key]-1/rate) < 1e-7, 'actual game frame cadence')
    commands = data['commands']
    require(commands and len(commands) < 3000, 'missing/unbounded input ledger')
    by_publish = {}
    previous_publish = 0
    first_replay_publish = next(cmd['publish_index'] for cmd in commands if cmd['phase'] == 6)
    for cmd in commands:
        pub = cmd['publish_index']
        require(pub == previous_publish+1, 'input publication sequence')
        previous_publish = pub
        expected_seq = (pub-1 if pub == first_replay_publish else 1) if cmd['phase'] == 6 else pub
        require(cmd['command_seq'] == expected_seq, 'prescribed command sequence')
        replay_kind = ('same_sequence_new_snapshot' if pub == first_replay_publish else 'older_sequence') if cmd['phase'] == 6 else 'none'
        require(cmd['replay_kind'] == replay_kind, 'prescribed replay case')
        require(0 <= cmd['phase'] <= 8 and cmd['phase'] == int(cmd['phase']), 'input phase')
        for key in ('issued_s', 'publish_solver_s'):
            require(finite(cmd[key]), 'nonfinite command timestamp')
        require(cmd['input_generation'] == (2 if cmd['phase'] == 4 else 1), 'prescribed generation')
        require(cmd['armed'] is (cmd['phase'] not in (0, 2, 8)), 'prescribed ARM state')
        require(cmd['input_numbers_finite'] is (cmd['phase'] != 5), 'prescribed invalid input')
        require(cmd['command_force_n'] == ([0., 0., 0.] if cmd['phase'] == 5 else FORCE), 'input force changed')
        require(cmd['offset_m'] == OFFSET, 'input offset changed')
        require(abs(cmd['issued_s'] - cmd['publish_solver_s'] + (1 if cmd['phase'] == 3 else 0)) < 1e-8, 'input clock or lease refreshed')
        by_publish[pub] = cmd
    samples = data['samples']
    require(100 < len(samples) < 3000, 'missing/unbounded step records')
    phase_counts = [0]*9
    phase_times = [0.]*9
    seen_publish = set()
    previous = None
    expected_v = [0., 0., 0.]
    expected_w = [0., 0., 0.]
    highest_cmd = 0
    phase_previous = 0
    publish_previous = 0
    min_dt, max_dt, elapsed = math.inf, 0., 0.
    max_v_error = max_w_error = max_v_total = max_w_total = 0.
    applied_count = 0
    for index, row in enumerate(samples, 1):
        require(row['sequence'] == index, 'missing/duplicate/reordered solver step')
        dt, sim, phase = row['dt_s'], row['sim_s'], row['phase']
        require(finite(dt) and 0 < dt < .009 and finite(sim), 'step clock')
        require(phase in range(9) and phase_previous <= phase <= phase_previous+1, 'phase ordering')
        phase_previous = phase
        phase_counts[phase] += 1
        phase_times[phase] += dt
        min_dt, max_dt, elapsed = min(min_dt, dt), max(max_dt, dt), elapsed+dt
        require(row['pre_simulates'] == row['pre_integrates'] == row['post_integrates'] == 1, 'hook multiplicity')
        cmd = by_publish.get(row['publish_index'])
        require(cmd is not None, 'input absent from game-thread ledger')
        require(row['publish_index'] >= publish_previous, 'out of order snapshot publication')
        publish_previous = row['publish_index']
        require(sim >= cmd['publish_solver_s']-1e-7, 'input consumed before publication')
        seen_publish.add(row['publish_index'])
        for key in ('phase', 'command_seq', 'input_generation', 'issued_s', 'armed',
                    'input_numbers_finite', 'command_force_n', 'offset_m', 'replay_kind'):
            require(row[key] == cmd[key], 'consumer differs from published snapshot: '+key)
        age = sim-row['issued_s']
        require(age >= -1e-7, 'future command applied')
        require((age > data['ttl_s']) == (phase == 3), 'stale lease decision')
        if phase == 6:
            if row['publish_index'] == first_replay_publish:
                require(row['command_seq'] == highest_cmd, 'missing same-sequence changed-payload attempt')
            else:
                require(row['command_seq'] == 1 and row['command_seq'] < highest_cmd, 'missing older replay attempt')
        else:
            require(row['command_seq'] >= highest_cmd, 'command ordering')
            highest_cmd = row['command_seq']
        active = phase in (1, 7)
        require(row['decision'] == DECISIONS[phase] and row['applied'] is active, 'wrong force gate')
        require(row['force_calls'] == row['torque_calls'] == int(active), 'duplicate/missing force application')
        require(abs(row['mass_kg']-MASS) < 1e-6, 'physical mass')
        require(distance(vector(row['inertia_kgm2']), [INERTIA]*3) < 1e-6, 'physical spherical inertia')
        require(abs(sum(x*x for x in vector(row['pre_q_xyzw'], 4))-1) < 1e-6, 'quaternion norm')
        before, after = vector(row['pre_v_mps']), vector(row['post_v_mps'])
        w_before, w_after = vector(row['pre_w_radps']), vector(row['post_w_radps'])
        origin, com, point = vector(row['pre_origin_m']), vector(row['pre_com_m']), vector(row['application_point_m'])
        require(distance(origin, com) < 1e-7, 'fixture COM is expected centered')
        require(distance(subtract(point, com), OFFSET) < 1e-7, 'application point/COM units')
        force = FORCE if active else [0., 0., 0.]
        torque = cross(OFFSET, force)
        require(distance(vector(row['force_n']), force) < 1e-10, 'wrong applied force or SI conversion')
        require(distance(vector(row['torque_nm']), torque) < 1e-10, 'wrong applied torque or SI conversion')
        if previous:
            require(abs(sim-previous['sim_s']-previous['dt_s']) < 1e-7, 'solver clock discontinuity')
            require(distance(before, previous['post_v_mps']) < VELOCITY_TOLERANCE, 'velocity changed outside callback integration')
            require(distance(w_before, previous['post_w_radps']) < VELOCITY_TOLERANCE, 'angular velocity changed outside callback integration')
        else:
            require(distance(before, [0., 0., 0.]) < 1e-10 and distance(w_before, [0., 0., 0.]) < 1e-10, 'nonzero initial velocity')
        delta_v = [f/MASS*dt for f in force]
        delta_w = [t/INERTIA*dt for t in torque]
        v_error = distance(subtract(after, before), delta_v)
        w_error = distance(subtract(w_after, w_before), delta_w)
        max_v_error, max_w_error = max(max_v_error, v_error), max(max_w_error, w_error)
        require(v_error < VELOCITY_TOLERANCE and w_error < VELOCITY_TOLERANCE, 'analytical per-step impulse mismatch')
        expected_v = [a+b for a, b in zip(expected_v, delta_v)]
        expected_w = [a+b for a, b in zip(expected_w, delta_w)]
        max_v_total = max(max_v_total, distance(after, expected_v))
        max_w_total = max(max_w_total, distance(w_after, expected_w))
        require(max_v_total < TOTAL_TOLERANCE and max_w_total < TOTAL_TOLERANCE, 'cumulative impulse mismatch')
        applied_count += active
        previous = row
    require(all(phase_counts), 'untested command gate')
    require(all(abs(duration-.5) < 1/rate+1e-6 for duration in phase_times), 'incorrect stage exposure duration')
    require(phase_previous == 8 and applied_count > 0, 'unfinished fixture')
    require(4.4 < elapsed < 5.2, 'incomplete/unbounded simulated duration')
    require(len(seen_publish) >= 9, 'snapshot transport missing')
    require(first_replay_publish in seen_publish and first_replay_publish+1 in seen_publish, 'both replay cases not observed')
    require(data['steps'] == len(samples) and data['force_calls'] == data['torque_calls'] == applied_count, 'summary counts differ from steps')
    require(data['order_errors'] == 0, 'callback order error')
    require(abs(data['solver_start_s']-samples[0]['sim_s']) < 1e-7, 'summary start clock')
    require(abs(data['solver_end_s']-samples[-1]['sim_s']-samples[-1]['dt_s']) < 1e-7, 'summary end clock')
    require(abs(data['solver_elapsed_s']-elapsed) < 1e-7 and abs(data['game_elapsed_s']-elapsed) < 1e-5, 'summary duration')
    require(abs(data['game_elapsed_s']-data['game_frames']/rate) < 1e-5, 'game time ledger')
    require(abs(data['solver_min_dt_s']-min_dt) < 1e-9 and abs(data['solver_max_dt_s']-max_dt) < 1e-9, 'summary step timing')
    return dict(ok=True, steps=len(samples), applied_steps=applied_count, phase_counts=phase_counts, phase_times_s=phase_times,
                completed_simulated_s=elapsed, min_dt_s=min_dt, max_dt_s=max_dt,
                input_snapshots_consumed=len(seen_publish), max_step_velocity_error_mps=max_v_error,
                max_step_angular_velocity_error_radps=max_w_error,
                max_cumulative_velocity_error_mps=max_v_total, max_cumulative_angular_velocity_error_radps=max_w_total,
                final_velocity_mps=samples[-1]['post_v_mps'], final_angular_velocity_radps=samples[-1]['post_w_radps'],
                expected_final_velocity_mps=expected_v, expected_final_angular_velocity_radps=expected_w)


def corruption_checks(data, rate, source):
    active = next(i for i, row in enumerate(data['samples']) if row['phase'] == 1)
    blocked = next(i for i, row in enumerate(data['samples']) if row['phase'] == 3)
    cases = {
        'lost_step': lambda d: d['samples'].pop(active),
        'duplicated_step': lambda d: d['samples'].insert(active, copy.deepcopy(d['samples'][active])),
        'missing_final_stage': lambda d: d.update(samples=[r for r in d['samples'] if r['phase'] != 8]),
        'double_force': lambda d: d['samples'][active].update(force_calls=2),
        'no_actual_acceleration': lambda d: d['samples'][active].update(post_v_mps=d['samples'][active]['pre_v_mps'][:]),
        'extra_blocked_impulse': lambda d: d['samples'][blocked]['post_v_mps'].__setitem__(0, 10.),
        'wrong_force_units': lambda d: d['samples'][active].update(force_n=[200., -100., 50.]),
        'wrong_torque_units': lambda d: d['samples'][active].update(torque_nm=[250., 0., -1000.]),
        'wrong_com': lambda d: d['samples'][active]['pre_com_m'].__setitem__(0, 20.),
        'nan_velocity': lambda d: d['samples'][active]['post_v_mps'].__setitem__(0, float('nan')),
        'lease_silently_refreshed': lambda d: d['samples'][blocked].update(issued_s=d['samples'][blocked]['sim_s']),
        'replayed_input_accepted': lambda d: next(r for r in d['samples'] if r['phase'] == 6).update(applied=True),
        'unsafe_body_release': lambda d: d.update(callback_unregistered_before_body_release=False),
        'callback_survives_body_release': lambda d: d.update(callback_deleted_before_body_release=False),
        'callback_runs_after_shutdown': lambda d: d.update(post_shutdown_callback_calls=1),
        'invalid_coast_error': lambda d: d.update(coast_velocity_error_mps=-1),
        'wrong_summary_count': lambda d: d.update(steps=1),
        'wrong_frame_rate': lambda d: d.update(game_max_dt_s=.1),
        'wrong_source': lambda d: d['build'].update(source_sha256='0'*64),
    }
    checks = []
    for label, mutate in cases.items():
        damaged = copy.deepcopy(data)
        mutate(damaged)
        try:
            analyze(damaged, rate, source)
        except ValueError as error:
            checks.append(dict(case=label, rejected=True, reason=str(error)))
        else:
            raise ValueError('Fault escaped independent analysis: '+label)
    return checks
