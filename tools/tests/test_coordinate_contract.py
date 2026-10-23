#!/usr/bin/env python3
"""Boundary model and negative controls for the 09.002 contract checker."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('coordinate_contract', ROOT / 'tools/check_coordinate_contract.py')
contract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contract)


class CoordinateContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads(contract.MANIFEST.read_text())

    def reject_change(self, change):
        manifest = copy.deepcopy(self.manifest)
        change(manifest)
        with self.assertRaises((ValueError, KeyError)):
            contract.validate(manifest)

    def test_complete_pinned_source_set(self):
        sources = contract.validate(self.manifest)
        self.assertIn('app/base/pixel-processor.c', sources)
        self.assertIn('app/display/gimpdisplayshell-transform.c', sources)

    def test_missing_duty(self):
        self.reject_change(lambda m: m['duties'].pop())

    def test_duplicate_duty(self):
        self.reject_change(lambda m: m['duties'].__setitem__(0, copy.deepcopy(m['duties'][1])))

    def test_wrong_source_hunk(self):
        self.reject_change(lambda m: m['duties'][0].__setitem__('source_range', '-0,0 -> 1,1'))

    def test_wrong_blob_hash(self):
        self.reject_change(lambda m: m['duties'][0].__setitem__('blob_sha256', '0' * 64))

    def test_wrong_legacy_revision(self):
        self.reject_change(lambda m: m.__setitem__('legacy_commit', 'HEAD'))

    def test_missing_context_source(self):
        self.reject_change(lambda m: m['legacy_context'].pop())

    def test_missing_space(self):
        self.reject_change(lambda m: m['spaces'].pop('screen'))

    def test_zero_based_buffer_assumption(self):
        self.reject_change(lambda m: m['spaces']['buffer'].__setitem__('origin', [0, 0]))

    def test_physical_screen_assumption(self):
        self.reject_change(lambda m: m['spaces']['screen'].__setitem__('units', 'physical pixels'))

    def test_wrong_layer_translation(self):
        self.reject_change(lambda m: m['spaces']['layer'].__setitem__('to_image', 'image = layer - item_offset'))

    def test_wrong_transform_order(self):
        self.reject_change(lambda m: m['spaces']['screen'].__setitem__('to_image', 'divide by zoom, then inverse rotation'))

    def test_wrong_axis_direction(self):
        self.reject_change(lambda m: m['spaces']['image'].__setitem__('axes', ['right', 'up']))

    def test_closed_interval(self):
        self.reject_change(lambda m: m['boundaries'].__setitem__('interval', 'closed'))

    def test_abyss_clamp_substitution(self):
        self.reject_change(lambda m: m['boundaries'].__setitem__('read_abyss', 'GEGL_ABYSS_CLAMP'))

    def test_wrong_offset_sign(self):
        self.reject_change(lambda m: m['offset_cases'][0].__setitem__('buffer', [-5, 7]))

    def test_touching_edge_is_not_pixel(self):
        self.reject_change(lambda m: m['intersections'][3].__setitem__('expected', [2, 7, 1, 1]))

    def test_all_small_rectangle_intersections(self):
        # Independent set-of-pixels oracle: 20,736 negative/positive/empty pairs.
        rectangles = [(x, y, w, h) for x in range(-2, 2) for y in range(-2, 2)
                      for w in range(3) for h in range(3)]
        for a in rectangles:
            aset = {(x, y) for x in range(a[0], a[0] + a[2]) for y in range(a[1], a[1] + a[3])}
            for b in rectangles:
                expected = aset & {(x, y) for x in range(b[0], b[0] + b[2]) for y in range(b[1], b[1] + b[3])}
                result = contract.intersection(a, b)
                actual = set() if result is None else {
                    (x, y) for x in range(result[0], result[0] + result[2])
                    for y in range(result[1], result[1] + result[3])}
                self.assertEqual(actual, expected, (a, b))

    def test_widened_endpoint_admission(self):
        maximum, minimum = contract.I32_MAX, contract.I32_MIN
        self.assertEqual(contract.checked_rect([minimum, minimum, 1, 1]), (minimum, minimum, 1, 1))
        self.assertEqual(contract.checked_rect([maximum - 1, maximum - 1, 1, 1]), (maximum - 1, maximum - 1, 1, 1))
        for rectangle in ([maximum, 0, 1, 1], [0, maximum, 1, 1], [minimum - 1, 0, 1, 1],
                          [0, 0, maximum + 1, 1], [0, 0, -1, 1], [True, 0, 1, 1], [0.5, 0, 1, 1]):
            with self.subTest(rectangle=rectangle), self.assertRaises(ValueError):
                contract.checked_rect(rectangle)


if __name__ == '__main__':
    unittest.main(verbosity=2)
