#!/usr/bin/env python3
"""Bounded parser and frozen-evidence rejection checks for 04.009."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location('archive_order', ROOT / 'tools/check_painter_archive_order.py')
CHECKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECKER)


class ArchiveOrderChecks(unittest.TestCase):
    def test_gnu_map_keeps_thin_inline_wrapped_and_whole_archive(self):
        text = '''Archive member included to satisfy reference by file (symbol)

app/libapp.a.p/app.c.o        app/main.c.o (app_init)
app/core/libcore.a.p/type.cpp.o
                              app/libapp.a.p/app.c.o (type_get_type)
libfixture.a(register.o)     (--whole-archive)

Discarded input sections
app/libapp.a.p/app.c.o        discarded duplicate (not_a_root)
'''
        found = CHECKER.extraction_map(text)
        self.assertEqual(len(found), 3)
        self.assertEqual(found['app/libapp.a.p/app.c.o'], 'app/main.c.o (app_init)')
        self.assertEqual(found['libfixture.a(register.o)'], '(--whole-archive)')
        self.assertIn('type_get_type', found['app/core/libcore.a.p/type.cpp.o'])

    def test_strong_components_distinguish_dag_and_cycle(self):
        nodes = ['core', 'paint', 'painter', 'gui']
        self.assertEqual(CHECKER.strong_components(nodes, [('gui', 'core'), ('core', 'painter')]), [])
        self.assertEqual(CHECKER.strong_components(nodes, [('gui', 'core'), ('core', 'paint'),
                                                          ('paint', 'core'), ('paint', 'painter')]), [['core', 'paint']])

    def reject_modified_report(self, modify, message):
        data = json.loads(CHECKER.REPORT.read_text())
        modify(data)
        with tempfile.TemporaryDirectory(prefix='painter-archive-order-') as directory:
            path = Path(directory) / 'negative.json'
            path.write_text(json.dumps(data))
            with self.assertRaisesRegex(RuntimeError, message):
                CHECKER.check(path, CHECKER.EVIDENCE)

    def test_frozen_check_rejects_missing_production_root(self):
        self.reject_modified_report(lambda d: d['production'][0]['explicit_roots'].pop(), 'root coverage')

    def test_frozen_check_rejects_false_negative_link_success(self):
        def modify(data):
            next(c for c in data['controls'] if c['kind'] == 'real-production-archive-order')['negative']['exit_code'] = 0
        self.reject_modified_report(modify, 'Real archive-order control failed')

    def test_frozen_check_rejects_source_drift(self):
        self.reject_modified_report(lambda d: d['source_sha256'].__setitem__('app/meson.build', '0' * 64), 'source/evidence drift')


if __name__ == '__main__':
    unittest.main()
