"""Compare two saved local AI training results without changing either file.

Usage: python Scripts/compare-ai-results.py previous.json current.json
Exit codes: 0 = descriptive deltas available, 2 = comparison blocked,
1 = invalid/unreadable result. No game, controller, or network access is used.
"""
import argparse
from datetime import datetime, timedelta
import json
import math
from pathlib import Path
import re
import sys


MAX_RESULT_BYTES = 65536  # Same report-size limit as the native result loader.
STOP_REASONS = {
    'manual_stop', 'time_limit', 'reset', 'application_exit', 'mode_changed',
    'defender_grounded', 'defender_unavailable', 'recovery_failed',
}
COMPLETION_REASONS = {'landed', 'reset', 'application_exit', 'mode_changed'}
LIMITATIONS = [
    'Only recorded conditions are checked. Input/calibration, rates/expo, '
    'battery state, and the full starting scene are not recorded in these results.',
    'Deltas describe local game results, not a skill score, verified identical '
    'training conditions, physical flight, or an authenticated online record.',
    'Guard values are experimental model observations at the stop request; '
    'AI landing time is not the human pilot landing time.',
]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def number(row, key, low, high, integral=False):
    value = row.get(key)
    require(isinstance(value, (int, float)) and not isinstance(value, bool),
            key + ' must be a number')
    require(low <= value <= high and math.isfinite(value), key + ' is out of range')
    require(not integral or value == math.floor(value), key + ' must be an integer')
    return int(value) if integral else value


def text_field(row, key, limit=128):
    # Native FString length limits count UTF-16 units, including surrogate pairs.
    value = row.get(key)
    require(isinstance(value, str) and bool(value.strip()) and len(value.encode('utf-16-le')) // 2 <= limit,
            key + ' must be a nonempty string within its length limit')
    return value


def utc_time(row, key):
    value = text_field(row, key)
    require('T' in value, key + ' must be a UTC ISO timestamp')
    try:
        parsed = datetime.fromisoformat(value.replace('Z', '+00:00'))
    except ValueError as error:
        raise ValueError(key + ' must be a UTC ISO timestamp') from error
    require(parsed.utcoffset() == timedelta(0), key + ' must include UTC timezone')
    return value, parsed


def summarize(result):
    """Validate the native v1/v2 report schema; never invent missing metrics."""
    require(isinstance(result, dict), 'Result must be a JSON object')
    schema = number(result, 'schema', 1, 2, integral=True)
    count = number(result, 'defenders' if schema == 1 else 'opponents', 3, 4, integral=True)
    mode = 'defence' if schema == 1 else text_field(result, 'session_mode')
    require(mode in ('defence', 'duel'), 'Unknown session_mode')
    session_id = text_field(result, 'session_id')
    require(re.fullmatch(r'[0-9a-fA-F]{32}', session_id) is not None, 'Invalid session_id')
    source_hash = text_field(result, 'source_sha256')
    require(re.fullmatch(r'[0-9a-fA-F]{64}', source_hash) is not None, 'Invalid source_sha256')
    started, started_time = utc_time(result, 'started_utc')
    ended, ended_time = utc_time(result, 'ended_utc')
    require(ended_time >= started_time, 'ended_utc precedes started_utc')
    stop = text_field(result, 'stop_reason')
    completion = text_field(result, 'completion_reason')
    require(stop in STOP_REASONS, 'Unknown stop_reason')
    require(completion in COMPLETION_REASONS, 'Unknown completion_reason')
    require(type(result.get('test_session')) is bool, 'test_session must be a boolean')
    conditions = dict(
        schema=schema, source_sha256=source_hash.lower(),
        build_version=text_field(result, 'build_version', 64), session_mode=mode,
        opponents=count, difficulty=number(result, 'difficulty', 0, 2, integral=True),
        seed=number(result, 'seed', 1, 4294967295, integral=True),
        test_session=result['test_session'],
    )
    metrics = dict(
        goals=number(result, 'goals', 0, 100000, integral=True),
        practice_seconds=number(result, 'practice_seconds', 0, 604800),
        landing_seconds=number(result, 'landing_seconds', 0, 604800),
    )
    if schema == 2:
        metrics['opponent_goals'] = number(result, 'opponent_goals', 0, 100000, integral=True)
    if 'recovery_attempts' in result:
        metrics['recovery_attempts'] = number(result, 'recovery_attempts', 0, 100000, integral=True)
    aircraft = result.get('aircraft')
    require(isinstance(aircraft, list) and len(aircraft) == count + 1, 'Invalid aircraft roster size')
    first_id = 5 if mode == 'duel' else 6
    expected_ids = {0, *range(first_id, first_id + count)}
    seen = set()
    for row in aircraft:
        require(isinstance(row, dict), 'Aircraft must be an object')
        aircraft_id = number(row, 'id', 0, 9, integral=True)
        require(aircraft_id in expected_ids and aircraft_id not in seen,
                'Unexpected or duplicate aircraft id')
        seen.add(aircraft_id)
        impacts = number(row, 'guard_impacts', 0, 1000000, integral=True)
        remaining = number(row, 'guard_remaining_percent', 0, 100)
        if aircraft_id == 0:
            metrics['player_guard_impacts'] = impacts
            metrics['player_guard_remaining_percent'] = remaining
    require(seen == expected_ids, 'Incomplete aircraft roster')
    return dict(session_id=session_id.lower(), started_utc=started, ended_utc=ended,
                stop_reason=stop, completion_reason=completion,
                conditions=conditions, metrics=metrics)


def compare_results(reference, candidate):
    """Return current-minus-reference deltas only for qualified local sessions."""
    reference, candidate = summarize(reference), summarize(candidate)
    reasons = []
    for field, value in reference['conditions'].items():
        if value != candidate['conditions'][field]:
            reasons.append('Recorded condition differs: ' + field)
    matched = not reasons
    if reference['session_id'] == candidate['session_id']:
        reasons.append('Both files identify the same session (including a save retry or copy)')
    for label, result in (('reference', reference), ('candidate', candidate)):
        if result['conditions']['test_session']:
            reasons.append(label + ': automated test session')
        if result['stop_reason'] != 'time_limit':
            reasons.append(label + ': session did not reach the time limit')
        if result['completion_reason'] != 'landed':
            reasons.append(label + ': AI landing did not complete')
        # Match the existing native three-minute diagnostic's duration tolerance.
        if not 179 <= result['metrics']['practice_seconds'] <= 181:
            reasons.append(label + ': duration is outside the three-minute window (179..181 s)')
    deltas = {}
    if not reasons:
        deltas = {key: round(candidate['metrics'][key] - value, 6)
                  for key, value in reference['metrics'].items() if key in candidate['metrics']}
    return dict(comparison_schema=1, reference=reference, candidate=candidate,
                recorded_conditions_match=matched, same_training_conditions_verified=False,
                eligible_for_deltas=not reasons, reasons=reasons, deltas=deltas,
                delta_direction='candidate minus reference', limitations=list(LIMITATIONS))


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate JSON key: ' + key)
        result[key] = value
    return result


def reject_constant(value):
    raise ValueError('Nonfinite JSON number: ' + value)


def read_result(path):
    with Path(path).open('rb') as stream:
        data = stream.read(MAX_RESULT_BYTES + 1)
    require(len(data) <= MAX_RESULT_BYTES, 'Result exceeds 65536 bytes')
    return json.loads(data.decode('utf-8-sig'), object_pairs_hook=unique_object,
                      parse_constant=reject_constant)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=Path, help='Earlier saved TrainingResults/AI JSON')
    parser.add_argument('candidate', type=Path, help='Newer saved TrainingResults/AI JSON')
    args = parser.parse_args(argv)
    try:
        result = compare_results(read_result(args.reference), read_result(args.candidate))
    except (OSError, ValueError, RecursionError) as error:
        print('Cannot compare AI results: ' + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False))
    return 0 if result['eligible_for_deltas'] else 2


if __name__ == '__main__':
    # JSON pipes stay UTF-8 even when Windows inherits a legacy console encoding.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, 'reconfigure'):
            stream.reconfigure(encoding='utf-8')
    sys.exit(main())
