#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic compile-database tests; these do not execute a sanitizer harness."""
import json
from pathlib import Path
import tempfile
import unittest
from painter_sanitizer_scope import bridge_rtti_sources, CXX_SUFFIXES


class DiscoveryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'project'
        self.build = self.root / 'build'
        self.build.mkdir(parents=True)

    def discover(self, entries):
        (self.build / 'compile_commands.json').write_text(json.dumps(entries))
        return bridge_rtti_sources(self.root, self.build)

    def entry(self, file, flags='-fno-rtti'):
        return {'directory': str(self.build), 'file': '../' + file,
                'command': 'c++ ' + flags + ' -c ../' + file}

    def test_all_supported_cpp_suffixes(self):
        files = {'app/paint/loops' + suffix for suffix in CXX_SUFFIXES}
        self.assertEqual(self.discover([self.entry(f) for f in files]), files)

    def test_production_and_explicit_no_rtti_only(self):
        accepted = 'app/paint/gimpbrushcore-loops.cc'
        entries = [self.entry(accepted), self.entry('app/tests/native.cpp'),
                   self.entry('app/painter/tests/unit.cpp'), self.entry('libgimp/helper.cpp'),
                   self.entry('app/paint/ordinary.cpp', ''), self.entry('app/paint/c.c'),
                   self.entry('../external/app/outside.cpp')]
        self.assertEqual(self.discover(entries), {accepted})

    def test_arguments_form_and_deduplication(self):
        file = 'app/paint/owner.cxx'
        entry = self.entry(file)
        del entry['command']
        entry['arguments'] = ['c++', '-fno-rtti', '-c', '../' + file]
        self.assertEqual(self.discover([entry, self.entry(file)]), {file})


if __name__ == '__main__':
    unittest.main()
