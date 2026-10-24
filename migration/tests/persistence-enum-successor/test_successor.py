#!/usr/bin/env python3
"""Focused enum metadata negatives; no runtime/preview diagnostics."""
import copy
import hashlib
import io
import json
from pathlib import Path
import shutil
import sys
import tarfile
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'migration/tests'))
import persistence_enum_successor as gate
import persistence_successors as predecessor_gate


class EnumSuccessor(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.table = gate.reviewed(ROOT)
        with tarfile.open(ROOT / gate.PACKAGE / 'sources.tar.gz') as archive:
            cls.before = {r['path']: archive.extractfile('predecessor/' + r['path']).read().decode() for r in cls.table['providers']}
            cls.after = {r['path']: archive.extractfile('successor/' + r['path']).read().decode() for r in cls.table['providers']}

    def test_exact_four_wire_tuples_and_property_bodies(self):
        self.assertEqual(gate.semantic_proof(self.before, self.after), gate.VALUES)

    def test_numeric_name_nick_default_flags_and_export_mutants(self):
        cases = [
            (gate.HEADER, 'GIMP_MYPAINT_COLORIZE = 3', 'GIMP_MYPAINT_COLORIZE = 4'),
            (gate.REGISTRAR, '"GIMP_MYPAINT_COLORIZE", "colorize"', '"GIMP_MYPAINT_COLORIZE", "changed"'),
            (gate.REGISTRAR, '"GIMP_MYPAINT_COLORIZE", "colorize"', '"GIMP_MYPAINT_NORMAL", "colorize"'),
            (gate.REGISTRAR, 'g_enum_register_static ("GimpMypaintBrushMode", values)', 'g_enum_register_static ("OtherMode", values)'),
            (gate.OPTIONS, 'GIMP_TYPE_MYPAINT_BRUSH_MODE,0,flags)', 'GIMP_TYPE_MYPAINT_BRUSH_MODE,1,flags)'),
            (gate.OPTIONS, 'GIMP_TYPE_MYPAINT_BRUSH_MODE,0,flags)', 'GIMP_TYPE_MYPAINT_BRUSH_MODE,0,G_PARAM_READABLE)'),
            (gate.OPTIONS, 'i.legacy_brush_mode=g_value_get_enum(value)', 'i.legacy_brush_mode=0'),
            (gate.OPTIONS, 'g_value_set_enum(value,i.legacy_brush_mode)', 'g_value_set_enum(value,0)'),
            ('libgimpbase/gimpbase.def', '\tgimp_mypaint_brush_mode_get_type\n', ''),
        ]
        for path, old, new in cases:
            with self.subTest(path=path, mutation=new):
                candidate = self.after.copy()
                self.assertIn(old, candidate[path])
                candidate[path] = candidate[path].replace(old, new, 1)
                with self.assertRaises(AssertionError):
                    gate.semantic_proof(self.before, candidate)

    def test_exact_two_evidence_relocations_only(self):
        options = self.after[gate.OPTIONS]
        for row in self.table['evidence_successors']:
            self.assertEqual(gate.resolve_evidence(ROOT, row['field_id'], row['predecessor'], options), row['successor'])
            changed = dict(row['predecessor'], expected='unreviewed evidence')
            self.assertEqual(gate.resolve_evidence(ROOT, row['field_id'], changed, options), changed)
            self.assertEqual(gate.resolve_evidence(ROOT, 'another.field', row['predecessor'], options), row['predecessor'])
        self.assertEqual(gate.resolve_evidence(ROOT, 'mypaint.option.mode', self.table['evidence_successors'][0]['predecessor'], self.before[gate.OPTIONS]), self.table['evidence_successors'][0]['predecessor'])

    def test_exact_chain_and_wrong_intermediate_rejected(self):
        earlier = json.loads((ROOT / 'migration/tests/persistence-source-successor/transitions.json').read_text())
        row = next(r for r in earlier['transitions'] if r['path'] == gate.OPTIONS)
        current = next(r for r in self.table['providers'] if r['path'] == gate.OPTIONS)['successor_sha256']
        result = gate.resolve_source(ROOT, row, current)
        self.assertEqual(result['intermediate_sha256'], row['successor_sha256'])
        self.assertEqual(result['commits'][-1], self.table['successor_commit'])
        with self.assertRaisesRegex(AssertionError, 'predecessor mismatch'):
            gate.resolve_source(ROOT, dict(row, successor_sha256='0'*64), current)
        with self.assertRaisesRegex(AssertionError, 'successor mismatch'):
            gate.resolve_source(ROOT, row, '0'*64)

    def test_original_evidence_drift_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            shutil.copytree(ROOT / predecessor_gate.PACKAGE, root / predecessor_gate.PACKAGE)
            (root / 'migration/inventory').mkdir(parents=True)
            for family in ('xcf', 'mypaint', 'config'):
                path = 'migration/inventory/persistence-' + family + '-fields.json'
                shutil.copyfile(ROOT / path, root / path)
            path = root / 'migration/inventory/persistence-mypaint-fields.json'
            data = json.loads(path.read_text())
            row = next(f for f in data['fields'] if f['id'] == 'mypaint.option.mode')
            row['evidence'][0]['expected'] = 'unreviewed evidence replacement'
            path.write_text(json.dumps(data))
            with self.assertRaisesRegex(AssertionError, 'predecessor family changed'):
                predecessor_gate.reviewed(root)

    def test_all_seven_current_providers_bound(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            shutil.copytree(ROOT / gate.PACKAGE, root / gate.PACKAGE)
            for path, text in self.after.items():
                (root / path).parent.mkdir(parents=True, exist_ok=True)
                (root / path).write_text(text)
            gate.reviewed(root)
            for path, text in self.after.items():
                with self.subTest(path=path):
                    (root / path).write_text(text + '\n/* unreviewed source */\n')
                    with self.assertRaisesRegex(AssertionError, 'enum provider drift'):
                        gate.reviewed(root)
                    (root / path).write_text(text)
            table = root / gate.PACKAGE / 'transition.json'
            table.write_bytes(table.read_bytes() + b' ')
            with self.assertRaisesRegex(AssertionError, 'enum successor proof changed'):
                gate.reviewed(root)


if __name__ == '__main__':
    unittest.main(verbosity=2)
