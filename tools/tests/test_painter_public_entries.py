#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('entries', Path(__file__).parents[1] / 'audit_painter_public_entries.py')
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)

class Census(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'migration/contracts').mkdir(parents=True)
        (self.root / 'app').mkdir()
        self.map = self.root / module.MAP
        self.map.write_text('c_entry\timplementation\nentry_one\tTODO\nentry_two\tTODO\n')

    def test_absence_is_reported_without_acceptance(self):
        report = module.audit(self.root, [])
        self.assertEqual(report['entry_count'], 2)
        self.assertEqual(report['entries'][0]['source_tokens'], [])
        self.assertIn('not acceptance', report['scope'])

    def test_exact_names_and_lines(self):
        (self.root / 'app/test.c').write_text('entry_one_extra();\nentry_one(); entry_one();\nentry_two();\n')
        report = module.audit(self.root, [])
        self.assertEqual(report['entries'][0]['source_tokens'], [{'path': 'app/test.c', 'line': 2}])
        self.assertEqual(report['entries'][1]['source_tokens'][0]['line'], 3)

    def test_duplicate_map_fails(self):
        self.map.write_text('c_entry\nentry_one\nentry_one\n')
        with self.assertRaises(ValueError):
            module.audit(self.root, [])

    def test_symbol_spelling_not_substring(self):
        binary = self.root / 'native'
        binary.write_bytes(b'fixture identity')
        class Result:
            stdout = '0000 T entry_one\n0000 T entry_two_extra\n'
        with patch.object(module.subprocess, 'run', return_value=Result()) as run:
            link = module.audit(self.root, [binary])['links'][0]
        self.assertEqual(link['present'], ['entry_one'])
        self.assertEqual(link['absent'], ['entry_two'])
        self.assertEqual(link['sha256'], module.digest(binary))
        self.assertTrue(run.call_args.kwargs['check'])

if __name__ == '__main__':
    unittest.main()
