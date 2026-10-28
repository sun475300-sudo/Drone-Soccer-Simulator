"""Synthetic result-comparison tests; no game, radio, or real pilot data is used."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / 'Scripts' / 'compare-ai-results.py'
SPEC = importlib.util.spec_from_file_location('ai_result_comparison', SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def fixture(session=1, schema=2, mode='defence', opponents=3):
    """Legal persisted shape with conspicuously synthetic build provenance."""
    first = 5 if mode == 'duel' else 6
    result = dict(
        schema=schema, session_id=f'{session:032x}',
        started_utc='2026-01-01T00:00:00.000Z',
        ended_utc='2026-01-01T00:03:10.000Z',
        stop_reason='time_limit', completion_reason='landed',
        source_sha256='a' * 64, build_version='SYNTHETIC_TEST_ONLY',
        test_session=False, difficulty=1, seed=17, goals=1,
        practice_seconds=180.0, landing_seconds=10.0,
        recovery_attempts=2,
        aircraft=[dict(id=aircraft, guard_impacts=3 if aircraft == 0 else 0,
                       guard_remaining_percent=95.0 if aircraft == 0 else 100.0)
                  for aircraft in [0, *range(first, first + opponents)]])
    if schema == 1:
        result['defenders'] = opponents
    else:
        result.update(opponents=opponents, session_mode=mode, opponent_goals=0)
    return result


class ComparisonTests(unittest.TestCase):
    def compare(self, reference=None, candidate=None):
        return MODULE.compare_results(
            fixture(1) if reference is None else reference,
            fixture(2) if candidate is None else candidate)

    def assert_blocked(self, reference, candidate, conditions_match=None):
        result = self.compare(reference, candidate)
        self.assertFalse(result['eligible_for_deltas'])
        self.assertEqual(result['deltas'], {})
        self.assertTrue(result['reasons'])
        self.assertTrue(all(isinstance(reason, str) and reason
                            for reason in result['reasons']))
        if conditions_match is not None:
            self.assertEqual(result['recorded_conditions_match'], conditions_match)
        return result

    def test_matching_completed_records_include_provenance(self):
        result = self.compare()
        self.assertTrue(result['recorded_conditions_match'])
        self.assertTrue(result['eligible_for_deltas'])
        self.assertEqual(result['reasons'], [])
        for side, session in [('reference', 1), ('candidate', 2)]:
            self.assertIsInstance(result[side], dict)
            self.assertEqual(result[side]['session_id'], f'{session:032x}')

    def test_deltas_are_candidate_minus_reference(self):
        old, new = fixture(1), fixture(2)
        new.update(goals=4, opponent_goals=2, practice_seconds=181,
                   landing_seconds=8.5, recovery_attempts=1)
        new['aircraft'][0].update(guard_impacts=1, guard_remaining_percent=98)
        result = self.compare(old, new)
        self.assertEqual(result['deltas'], dict(
            goals=3, opponent_goals=2, practice_seconds=1,
            landing_seconds=-1.5, recovery_attempts=-1,
            player_guard_impacts=-2, player_guard_remaining_percent=3))

    def test_player_is_identified_by_id_not_roster_order(self):
        new = fixture(2)
        new['aircraft'][0]['guard_impacts'] = 8
        new['aircraft'].reverse()
        self.assertEqual(self.compare(candidate=new)['deltas']['player_guard_impacts'], 5)

    def test_v1_records_do_not_invent_opponent_goals(self):
        result = self.compare(fixture(1, schema=1), fixture(2, schema=1))
        self.assertTrue(result['eligible_for_deltas'])
        self.assertNotIn('opponent_goals', result['deltas'])

    def test_optional_recovery_delta_requires_both_values(self):
        for side in ('reference', 'candidate', 'both'):
            with self.subTest(side=side):
                old, new = fixture(1), fixture(2)
                if side in ('reference', 'both'):
                    del old['recovery_attempts']
                if side in ('candidate', 'both'):
                    del new['recovery_attempts']
                result = self.compare(old, new)
                self.assertTrue(result['eligible_for_deltas'])
                self.assertNotIn('recovery_attempts', result['deltas'])

    def test_duel_and_four_opponent_rosters_are_supported(self):
        for mode in ('defence', 'duel'):
            for count in (3, 4):
                with self.subTest(mode=mode, count=count):
                    result = self.compare(fixture(1, mode=mode, opponents=count),
                                          fixture(2, mode=mode, opponents=count))
                    self.assertTrue(result['eligible_for_deltas'])

    def test_recorded_condition_mismatches_block_deltas(self):
        for field, value in [('source_sha256', 'b' * 64),
                             ('build_version', 'SYNTHETIC_OTHER_BUILD'),
                             ('difficulty', 2), ('seed', 99)]:
            with self.subTest(field=field):
                new = fixture(2)
                new[field] = value
                self.assert_blocked(fixture(1), new, conditions_match=False)

    def test_schema_mode_and_count_mismatches_block_deltas(self):
        for candidate in (fixture(2, schema=1), fixture(2, mode='duel'),
                          fixture(2, opponents=4)):
            with self.subTest(candidate=candidate):
                self.assert_blocked(fixture(1), candidate, conditions_match=False)

    def test_same_session_cannot_be_compared_with_itself(self):
        self.assert_blocked(fixture(1), fixture(1))

    def test_automated_records_cannot_be_personal_progress(self):
        for side in ('reference', 'candidate', 'both'):
            with self.subTest(side=side):
                old, new = fixture(1), fixture(2)
                if side in ('reference', 'both'):
                    old['test_session'] = True
                if side in ('candidate', 'both'):
                    new['test_session'] = True
                self.assert_blocked(old, new)

    def test_valid_noncompletion_reasons_block_deltas(self):
        for field, values in [
                ('stop_reason', ['manual_stop', 'reset', 'application_exit',
                                 'mode_changed', 'defender_grounded',
                                 'defender_unavailable', 'recovery_failed']),
                ('completion_reason', ['reset', 'application_exit', 'mode_changed'])]:
            for value in values:
                for side in ('reference', 'candidate'):
                    with self.subTest(field=field, value=value, side=side):
                        old, new = fixture(1), fixture(2)
                        (old if side == 'reference' else new)[field] = value
                        self.assert_blocked(old, new)

    def test_duration_window_is_inclusive(self):
        for seconds in (179, 180, 181):
            with self.subTest(seconds=seconds):
                new = fixture(2)
                new['practice_seconds'] = seconds
                self.assertTrue(self.compare(candidate=new)['eligible_for_deltas'])

    def test_duration_outside_window_blocks_either_record(self):
        for seconds in (0, 178.999, 181.001, 604800):
            for side in ('reference', 'candidate'):
                with self.subTest(seconds=seconds, side=side):
                    old, new = fixture(1), fixture(2)
                    (old if side == 'reference' else new)['practice_seconds'] = seconds
                    self.assert_blocked(old, new)

    def test_comparison_does_not_mutate_inputs(self):
        old, new = fixture(1), fixture(2)
        before = copy.deepcopy((old, new))
        self.compare(old, new)
        self.assertEqual((old, new), before)

    def test_missing_required_fields_are_rejected(self):
        for field in fixture():
            if field == 'recovery_attempts':
                continue
            with self.subTest(field=field):
                new = fixture(2)
                del new[field]
                with self.assertRaises(ValueError):
                    self.compare(candidate=new)

    def test_bad_top_level_shapes_are_rejected(self):
        for value in (None, [], 'record', 1, True):
            with self.subTest(value=value), self.assertRaises(ValueError):
                MODULE.compare_results(fixture(1), value)

    def test_invalid_schema_and_typed_fields_are_rejected(self):
        changes = [('schema', value) for value in (True, 0, 3, '2', None)]
        changes += [(field, value) for field, value in [
            ('test_session', 0), ('test_session', 'false'), ('goals', True),
            ('goals', 1.5), ('opponent_goals', False), ('opponents', 3.0 + .5),
            ('difficulty', True), ('seed', True), ('recovery_attempts', False),
            ('build_version', ''), ('build_version', 12), ('session_mode', 'match'),
            ('stop_reason', 'win'), ('completion_reason', 'success')]]
        for field, value in changes:
            with self.subTest(field=field, value=value):
                new = fixture(2)
                new[field] = value
                with self.assertRaises(ValueError):
                    self.compare(candidate=new)

    def test_nonfinite_and_negative_metrics_are_rejected(self):
        for field in ('goals', 'opponent_goals', 'practice_seconds',
                      'landing_seconds', 'recovery_attempts'):
            for value in (-1, float('nan'), float('inf'), -float('inf')):
                with self.subTest(field=field, value=value):
                    new = fixture(2)
                    new[field] = value
                    with self.assertRaises(ValueError):
                        self.compare(candidate=new)

    def test_invalid_setting_ranges_are_rejected(self):
        for field, values in [('opponents', [0, 2, 5]), ('difficulty', [-1, 3]),
                              ('seed', [-1, 0, 4294967296])]:
            for value in values:
                with self.subTest(field=field, value=value):
                    new = fixture(2)
                    new[field] = value
                    with self.assertRaises(ValueError):
                        self.compare(candidate=new)

    def test_invalid_guid_sha_and_utc_are_rejected(self):
        for field, values in [
                ('session_id', ['', 'not-a-guid', 'g' * 32, '0' * 31]),
                ('source_sha256', ['', 'g' * 64, 'a' * 63]),
                ('started_utc', ['', '2026-99-99T00:00:00Z', 'yesterday']),
                ('ended_utc', ['', '2026-01-01T25:00:00Z', 'tomorrow'])]:
            for value in values:
                with self.subTest(field=field, value=value):
                    new = fixture(2)
                    new[field] = value
                    with self.assertRaises(ValueError):
                        self.compare(candidate=new)

    def test_backwards_utc_is_rejected(self):
        new = fixture(2)
        new['ended_utc'] = '2025-12-31T23:59:59.000Z'
        with self.assertRaises(ValueError):
            self.compare(candidate=new)

    def test_duplicate_missing_extra_and_wrong_roster_are_rejected(self):
        for change in ('duplicate', 'missing_player', 'missing_opponent', 'extra', 'wrong_team'):
            with self.subTest(change=change):
                new = fixture(2)
                roster = new['aircraft']
                if change == 'duplicate':
                    roster[-1] = copy.deepcopy(roster[0])
                elif change == 'missing_player':
                    roster.pop(0)
                elif change == 'missing_opponent':
                    roster.pop()
                elif change == 'extra':
                    roster.append(dict(id=9, guard_impacts=0, guard_remaining_percent=100))
                else:
                    roster[-1]['id'] = 5
                with self.assertRaises(ValueError):
                    self.compare(candidate=new)

    def test_malformed_aircraft_rows_are_rejected(self):
        for field, value in [('id', True), ('id', 6.5), ('guard_impacts', True),
                             ('guard_impacts', -1), ('guard_impacts', .5),
                             ('guard_remaining_percent', -1),
                             ('guard_remaining_percent', 101),
                             ('guard_remaining_percent', float('nan'))]:
            with self.subTest(field=field, value=value):
                new = fixture(2)
                new['aircraft'][0][field] = value
                with self.assertRaises(ValueError):
                    self.compare(candidate=new)
        for value in (None, [], [None], 'aircraft'):
            with self.subTest(value=value):
                new = fixture(2)
                new['aircraft'] = value
                with self.assertRaises(ValueError):
                    self.compare(candidate=new)


    def test_build_version_limit_uses_native_utf16_units(self):
        for count in (31, 32):
            with self.subTest(accepted_emoji_count=count):
                old, new = fixture(1), fixture(2)
                old['build_version'] = new['build_version'] = '🚁' * count
                self.assertTrue(self.compare(old, new)['eligible_for_deltas'])
        for value in ('🚁' * 33, 'A' * 63 + '🚁'):
            with self.subTest(rejected_version=value), self.assertRaises(ValueError):
                new = fixture(2)
                new['build_version'] = value
                self.compare(candidate=new)


class CommandLineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='synthetic-ai-comparison-')
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.reference = root / 'SYNTHETIC-reference.json'
        self.candidate = root / 'SYNTHETIC-candidate.json'
        self.reference.write_text(json.dumps(fixture(1)), encoding='utf-8')
        self.candidate.write_text(json.dumps(fixture(2)), encoding='utf-8')

    def run_cli(self, env=None):
        return subprocess.run([sys.executable, str(SCRIPT), str(self.reference),
                               str(self.candidate)], encoding='utf-8', capture_output=True,
                              timeout=10, check=False, env=env)

    def assert_invalid_cli(self):
        result = self.run_cli()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertTrue(result.stderr.strip())
        self.assertNotIn('Traceback', result.stderr)
        self.assertEqual(result.stdout, '')

    def test_eligible_cli_preserves_both_files(self):
        before = (self.reference.read_bytes(), self.candidate.read_bytes())
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(result.stdout)['eligible_for_deltas'])
        self.assertEqual(result.stderr, '')
        self.assertEqual((self.reference.read_bytes(), self.candidate.read_bytes()), before)

    def test_blocked_cli_returns_two_and_json(self):
        candidate = fixture(2)
        candidate['seed'] += 1
        self.candidate.write_text(json.dumps(candidate), encoding='utf-8')
        before = self.candidate.read_bytes()
        result = self.run_cli()
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertFalse(json.loads(result.stdout)['eligible_for_deltas'])
        self.assertEqual(self.candidate.read_bytes(), before)

    def test_utf8_bom_is_accepted(self):
        self.candidate.write_text(json.dumps(fixture(2)), encoding='utf-8-sig')
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_invalid_json_is_a_clean_error(self):
        self.candidate.write_text('{broken JSON', encoding='utf-8')
        self.assert_invalid_cli()

    def test_missing_input_file_is_a_clean_error(self):
        self.candidate.unlink()
        self.assert_invalid_cli()

    def test_duplicate_top_level_and_nested_keys_are_rejected(self):
        original = json.dumps(fixture(2))
        for text in (original.replace('"goals": 1', '"goals": 1, "goals": 9'),
                     original.replace('"id": 0', '"id": 0, "id": 1')):
            with self.subTest(text=text):
                self.candidate.write_text(text, encoding='utf-8')
                self.assert_invalid_cli()

    def test_invalid_utf8_is_a_clean_error(self):
        self.candidate.write_bytes(b'\xff\xfeinvalid synthetic input')
        self.assert_invalid_cli()


    def test_exact_size_limit_is_accepted_without_modification(self):
        data = json.dumps(fixture(2)).encode('utf-8')
        data += b' ' * (MODULE.MAX_RESULT_BYTES - len(data))
        self.assertEqual(len(data), 65536)
        self.candidate.write_bytes(data)
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(result.stdout)['eligible_for_deltas'])
        self.assertEqual(self.candidate.read_bytes(), data)

    def test_one_byte_oversize_is_rejected_for_either_input(self):
        original = (self.reference.read_bytes(), self.candidate.read_bytes())
        for path in (self.reference, self.candidate):
            with self.subTest(path=path.name):
                self.reference.write_bytes(original[0])
                self.candidate.write_bytes(original[1])
                data = path.read_bytes()
                data += b' ' * (MODULE.MAX_RESULT_BYTES + 1 - len(data))
                self.assertEqual(len(data), 65537)
                path.write_bytes(data)
                result = self.run_cli()
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertIn('65536', result.stderr)
                self.assertEqual(result.stdout, '')
                self.assertNotIn('Traceback', result.stderr)
                self.assertEqual(path.read_bytes(), data)

    def test_utf8_bom_counts_toward_the_byte_limit(self):
        data = b'\xef\xbb\xbf' + json.dumps(fixture(2)).encode('utf-8')
        data += b' ' * (MODULE.MAX_RESULT_BYTES - len(data))
        self.candidate.write_bytes(data)
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.candidate.write_bytes(data + b' ')
        self.assert_invalid_cli()

    def test_unicode_paths_and_json_output_with_legacy_stdio(self):
        folder = self.reference.parent / '한글 폴더 🚁'
        folder.mkdir()
        self.reference, self.candidate = folder / '이전.json', folder / '이번.json'
        version = 'SYNTHETIC_한글_🚁'
        for path, session in ((self.reference, 1), (self.candidate, 2)):
            value = fixture(session)
            value['build_version'] = version
            path.write_bytes(json.dumps(value, ensure_ascii=False).encode('utf-8'))
        before = (self.reference.read_bytes(), self.candidate.read_bytes())
        env = os.environ.copy()
        env['PYTHONIOENCODING'] = 'cp949'
        result = self.run_cli(env=env)
        self.assertEqual(result.returncode, 0, result.stderr)
        value = json.loads(result.stdout)
        self.assertEqual(value['candidate']['conditions']['build_version'], version)
        self.assertIn('🚁', result.stdout)
        self.assertEqual((self.reference.read_bytes(), self.candidate.read_bytes()), before)

    def test_missing_unicode_path_error_is_clean_utf8(self):
        self.candidate = self.candidate.parent / '없는 기록 🚁.json'
        env = os.environ.copy()
        env['PYTHONIOENCODING'] = 'cp949'
        result = self.run_cli(env=env)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, '')
        self.assertNotIn('Traceback', result.stderr)
        self.assertIn('🚁', result.stderr)

    def test_unpaired_unicode_is_a_clean_input_error(self):
        candidate = fixture(2)
        candidate['build_version'] = '\ud800'
        self.candidate.write_bytes(json.dumps(candidate).encode('utf-8'))
        self.assert_invalid_cli()


if __name__ == '__main__':
    unittest.main(verbosity=2)
