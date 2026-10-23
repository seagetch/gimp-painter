#!/usr/bin/env python3
"""Bounded offline controls for the generic-route comparison harness."""
import contextlib
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import compare_owned_generic as comparator


class OwnedGenericComparator(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='owned-generic-control-')
        self.addCleanup(self.temporary.cleanup)
        self.build = Path(self.temporary.name)
        self.report = self.build / 'report.json'
        self.executables = {}
        for family in comparator.FAMILIES:
            executable = self.build / 'app/tests' / ('painter-' + family + '-trace')
            executable.parent.mkdir(parents=True, exist_ok=True)
            executable.write_bytes(b'not an executable; subprocess is mocked\n')
            self.executables[family] = executable
        self.profiles = []

    def expected(self, family, route):
        return gzip.decompress(
            (comparator.ROOT / 'migration/fixtures/legacy-owned-generic' /
             (family + '-' + route + '.tsv.gz')).read_bytes())

    def fake_run(self, command, *, env, stdout, stderr):
        self.assertEqual(stdout, subprocess.PIPE)
        self.assertEqual(stderr, subprocess.PIPE)
        family = next(name for name, path in self.executables.items()
                      if command == [str(path)])
        route = env['PAINTER_OWNED_GENERIC_ROUTE']
        self.assertIn(route, comparator.ROUTES)
        self.assertEqual(env['GIMP_TESTING_ABS_TOP_BUILDDIR'], str(self.build))
        self.assertEqual(env['GIMP_TESTING_ABS_TOP_SRCDIR'], str(comparator.ROOT))
        self.assertEqual(env['GIMP_TESTING_PLUGINDIRS'], str(self.build / 'plug-ins/common'))
        self.assertEqual(env['GIMP3_DIRECTORY'], str(comparator.ROOT / 'app/tests/gimpdir'))
        self.assertEqual(env['GSETTINGS_BACKEND'], 'memory')
        self.assertEqual(env['UI_TEST'], 'yes')
        variables = ('HOME', 'XDG_CONFIG_HOME', 'XDG_CACHE_HOME', 'XDG_DATA_HOME',
                     'XDG_STATE_HOME', 'XDG_RUNTIME_DIR', 'TMPDIR')
        paths = [Path(env[name]) for name in variables]
        self.assertEqual(len(set(paths)), len(paths))
        self.assertTrue(all(path.is_dir() and os.access(path, os.W_OK) for path in paths))
        profile = paths[0].parent
        self.assertTrue(all(path.parent == profile for path in paths))
        self.assertNotIn(profile, self.profiles)
        self.profiles.append(profile)
        if os.name == 'posix':
            self.assertEqual(Path(env['XDG_RUNTIME_DIR']).stat().st_mode & 0o777, 0o700)
        return subprocess.CompletedProcess(command, 0,
                                           b'ignored startup text\n' + self.expected(family, route),
                                           b'captured diagnostic\n')

    def invoke(self, extra=(), runner=None):
        stderr = io.TextIOWrapper(io.BytesIO(), encoding='utf-8')
        with mock.patch.object(comparator.subprocess, 'run', side_effect=runner or self.fake_run) as run:
            with contextlib.redirect_stdout(io.StringIO()), mock.patch.object(sys, 'stderr', stderr):
                status = comparator.main([str(self.build), '--report', str(self.report), *extra])
        self.assertTrue(all(not profile.exists() for profile in self.profiles))
        runs = json.loads(self.report.read_text())['runs']
        self.assertEqual(stderr.buffer.getvalue(), b''.join(record['stderr'].encode() for record in runs))
        return status, runs, run.call_count

    def selected(self, family='fill-brush', route='raw'):
        return ['--family', family, '--route', route, '--binary', str(self.executables[family])]

    def test_all_six_backward_compatible(self):
        status, runs, count = self.invoke()
        self.assertEqual(status, 0)
        self.assertEqual(count, 6)
        self.assertEqual([(run['family'], run['route']) for run in runs],
                         [(family, route) for family in comparator.FAMILIES for route in comparator.ROUTES])
        self.assertEqual(sum(run['bytes'] for run in runs), 14303286)
        self.assertEqual(sum(run['records'] for run in runs), 690)
        for run in runs:
            expected = self.expected(run['family'], run['route'])
            self.assertTrue(run['exact'])
            self.assertEqual(run['exit_code'], 0)
            self.assertEqual(run['stderr'], 'captured diagnostic\n')
            self.assertEqual(run['sha256'], hashlib.sha256(expected).hexdigest())
            self.assertEqual(run['executable_sha256'], hashlib.sha256(
                self.executables[run['family']].read_bytes()).hexdigest())

    def test_each_selected_route_runs_only_explicit_binary(self):
        for family in comparator.FAMILIES:
            # A nonstandard target path proves --binary is used, not rebuilt
            # from BUILD and the family name as in the all-six interface.
            self.executables[family] = self.executables[family].rename(
                self.build / ('explicit-' + family + '-target'))
            for route in comparator.ROUTES:
                with self.subTest(family=family, route=route):
                    status, runs, count = self.invoke(self.selected(family, route))
                    self.assertEqual((status, count, len(runs)), (0, 1, 1))
                    self.assertEqual((runs[0]['family'], runs[0]['route']), (family, route))
                    self.assertTrue(runs[0]['exact'])

    def test_changed_missing_extra_and_incomplete_records_fail(self):
        for defect in ('changed-byte', 'missing-record', 'extra-record', 'missing-completion'):
            with self.subTest(defect=defect):
                def damaged(command, **kwargs):
                    result = self.fake_run(command, **kwargs)
                    lines = result.stdout.splitlines()
                    if defect == 'changed-byte':
                        lines[1] = lines[1][:-1] + (b'1' if lines[1][-1:] != b'1' else b'0')
                    elif defect == 'missing-record':
                        del lines[1]
                    elif defect == 'extra-record':
                        lines.insert(1, lines[1])
                    else:
                        lines.pop()
                    result.stdout = b'\n'.join(lines) + b'\n'
                    return result
                status, runs, count = self.invoke(self.selected(), damaged)
                self.assertEqual((status, count), (1, 1))
                self.assertFalse(runs[0]['exact'])

    def test_nonzero_and_signal_exit_fail_even_with_exact_bytes(self):
        for code in (3, -6):
            with self.subTest(code=code):
                def failed(command, **kwargs):
                    result = self.fake_run(command, **kwargs)
                    result.returncode = code
                    return result
                status, runs, count = self.invoke(self.selected(), failed)
                self.assertEqual((status, count, runs[0]['exit_code']), (1, 1, code))
                self.assertTrue(runs[0]['exact'])

    def test_incomplete_or_unknown_selection_is_rejected_before_launch(self):
        for extra in (['--family', 'fill-brush'], ['--route', 'raw'],
                      ['--binary', str(self.executables['fill-brush'])],
                      ['--family', 'fill-brush', '--route', 'raw'],
                      self.selected(route='unknown')):
            with self.subTest(extra=extra), mock.patch.object(comparator.subprocess, 'run') as run:
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as failure:
                    comparator.main([str(self.build), '--report', str(self.report), *extra])
                self.assertEqual(failure.exception.code, 2)
                run.assert_not_called()

    def test_required_family_comparators_forward_stderr_on_success_and_failure(self):
        for script, fixture in (
                ('compare_fill_brush.py', 'legacy-fill-brush'),
                ('compare_smudge_strokes.py', 'legacy-smudge')):
            script = comparator.ROOT / 'migration/tests' / script
            fixture = comparator.ROOT / 'migration/fixtures' / fixture / 'pixels.tsv.gz'
            for code in (0, 3):
                with self.subTest(script=script.name, code=code):
                    result = subprocess.CompletedProcess(
                        ['mock-native'], code, gzip.decompress(fixture.read_bytes()),
                        b'native diagnostic, including successful runs\n')
                    stderr = io.TextIOWrapper(io.BytesIO(), encoding='utf-8')
                    with mock.patch.object(subprocess, 'run', return_value=result) as run, \
                            mock.patch.object(sys, 'argv', [str(script), str(fixture), 'mock-native']), \
                            mock.patch.object(sys, 'stderr', stderr), \
                            contextlib.redirect_stdout(io.StringIO()):
                        if code:
                            with self.assertRaises(SystemExit) as failure:
                                runpy.run_path(str(script), run_name='__main__')
                            self.assertEqual(failure.exception.code, code)
                        else:
                            runpy.run_path(str(script), run_name='__main__')
                    self.assertEqual(stderr.buffer.getvalue(), result.stderr)
                    run.assert_called_once_with(['mock-native'], stdout=subprocess.PIPE,
                                                stderr=subprocess.PIPE)


if __name__ == '__main__':
    unittest.main()
