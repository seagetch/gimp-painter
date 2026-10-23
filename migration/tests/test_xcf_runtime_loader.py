#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the actual relocated-runner path setup without building GIMP."""
import ast
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

RUNNER = Path(__file__).with_name('run_xcf_field_checkpoint_sanitizers.py')


def runtime_path(build, inherited=None):
    tree = ast.parse(RUNNER.read_text())
    block = next(node for node in tree.body if isinstance(node, ast.If)
                 and ast.unparse(node.test) == 'a.run')
    statements = block.body[:3]
    assert ast.unparse(statements[0].targets[0]) == 'runtime_dirs'
    assert ast.unparse(statements[2].targets[0]) == 'runtime_path'
    code = compile(ast.fix_missing_locations(ast.Module(body=statements, type_ignores=[])), str(RUNNER), 'exec')
    context = {'build': build, 'os': os}
    with patch.dict(os.environ, {} if inherited is None else {'LD_LIBRARY_PATH': inherited}, clear=True):
        exec(code, context)
    return context['runtime_path']


class RuntimePathTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.build = Path(self.temp.name)

    def library(self, directory, name):
        path = self.build / directory / name
        path.parent.mkdir(exist_ok=True)
        path.touch()

    def test_missing_build_libraries_refuses_host_fallback(self):
        with self.assertRaisesRegex(RuntimeError, 'Missing built libgimp'):
            runtime_path(self.build, '/host/lib')

    def test_exact_build_directories_precede_inherited_loader_path(self):
        self.library('libgimpconfig', 'libgimpconfig-3.0.so')
        self.library('libgimpbase', 'libgimpbase-3.0.so')
        self.assertEqual(runtime_path(self.build, '/host/lib:/other/lib').split(os.pathsep),
                         [str(self.build / 'libgimpbase'), str(self.build / 'libgimpconfig'), '/host/lib', '/other/lib'])

    def test_deduplicates_and_ignores_unrelated_directories(self):
        self.library('libgimpconfig', 'first.so')
        self.library('libgimpconfig', 'second.so')
        self.library('unrelated', 'host.so')
        self.assertEqual(runtime_path(self.build), str(self.build / 'libgimpconfig'))


if __name__ == '__main__':
    unittest.main()
