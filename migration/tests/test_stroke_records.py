"""Synthetic format tests only: these are never legacy output fixtures."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('validator', ROOT / 'tools/validate_stroke_record.py')
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


def example():
    artifact = {'path': 'synthetic.bin', 'sha256': hashlib.sha256(b'synthetic').hexdigest()}
    coords = dict.fromkeys(('x','y','pressure','xtilt','ytilt','wheel','velocity','direction'), 0.0)
    return {
        'schema_version': 1, 'evidence_kind': 'synthetic-format-test',
        'source_commit': 'afa43fae3e920210146abed514f136fd49f671b5',
        'capture_boundary': 'GimpMypaintCore::stroke_to-before-Brush::stroke_to',
        'provenance': {'operator': 'format-test', 'captured_utc': 'not-a-real-capture',
                       'build_manifest': dict(artifact), 'instrumentation_patch': dict(artifact),
                       'capture_log': dict(artifact)},
        'brush': {'source_path': 'data/mypaint-brushes/classic/brush.myb',
                  'sha256': '0'*64, 'effective_settings': dict(artifact)},
        'surface': {'width': 16, 'height': 16, 'pixel_format': 'R8G8B8A8',
                    'color_space': 'sRGB', 'alpha_convention': 'straight',
                    'initial_pixels': dict(artifact), 'resource_manifest': dict(artifact)},
        'engine_state': {'state_names': ['STATE_RNG_SEED'], 'state_f32_bits': ['00000000'],
                         'rng_seed_uint32': 0, 'reset_requested': True,
                         'stroke_total_painting_time': 0, 'stroke_current_idling_time': 0},
        'events': [{'sequence': i, 'monotonic_ns': i*1000, 'dtime_s': 0.000001,
                    'coords': dict(coords), 'phase': phase, 'modifier_state': 0,
                    'engine_split_after': False} for i,phase in enumerate(('begin','motion','end'))],
        'results': [],
    }


class StrokeFormatTests(unittest.TestCase):
    def test_synthetic_round_trip(self):
        record = example()
        record['events'][1]['coords'].update(pressure=0.75, xtilt=-0.3, ytilt=0.2, direction=0.75)
        self.assertEqual(validator.validate(json.loads(json.dumps(record))), [])

    def test_stationary_pressure_zero_delta_and_raw_negative_time(self):
        record = example()
        record['events'][1]['coords']['pressure'] = 0.7
        record['events'][1]['dtime_s'] = -0.01
        record['events'][1]['monotonic_ns'] = 0
        self.assertEqual(validator.validate(record), [])

    def test_missing_fields(self):
        for field in ('pressure','xtilt','ytilt','direction'):
            record = example()
            del record['events'][0]['coords'][field]
            self.assertTrue(validator.validate(record), field)

    def test_finite_only(self):
        for value in (float('nan'), float('inf'), -float('inf')):
            record = example()
            record['events'][0]['coords']['x'] = value
            self.assertTrue(validator.validate(record))

    def test_event_order(self):
        for field,value in (('sequence',10), ('monotonic_ns',10000), ('phase','end')):
            record = example()
            record['events'][1][field] = value
            self.assertTrue(validator.validate(record), field)

    def test_boolean_is_not_seed(self):
        record = example()
        record['engine_state']['rng_seed_uint32'] = True
        self.assertTrue(validator.validate(record))

    def test_seed_range_and_state_count(self):
        record = example()
        record['engine_state']['rng_seed_uint32'] = 2**32
        self.assertTrue(validator.validate(record))
        record = example()
        record['engine_state']['state_f32_bits'].append('00000000')
        self.assertTrue(validator.validate(record))

    def test_measured_record_requires_outputs(self):
        record = example()
        record['evidence_kind'] = 'legacy-runtime-capture'
        self.assertTrue(validator.validate(record))

    def test_relative_sidecars_only(self):
        for path in ('../x','/tmp/x','C:\\x','dir\\x'):
            record = example()
            record['provenance']['capture_log']['path'] = path
            self.assertTrue(validator.validate(record), path)

    def test_sidecar_integrity(self):
        with tempfile.TemporaryDirectory() as temp:
            record = example()
            self.assertTrue(validator.validate(record, temp))
            path = Path(temp) / 'synthetic.bin'
            path.write_bytes(b'synthetic')
            self.assertEqual(validator.validate(record, temp), [])
            path.write_bytes(b'changed')
            self.assertTrue(validator.validate(record, temp))

    def test_unknown_fields_rejected(self):
        record = example()
        record['events'][0]['omitted_input'] = 1
        self.assertTrue(validator.validate(record))


if __name__ == '__main__':
    unittest.main()
