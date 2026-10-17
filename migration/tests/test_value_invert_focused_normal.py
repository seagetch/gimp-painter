#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Pure Python runner controls; no native validation."""
import contextlib
import tempfile
from pathlib import Path
import subprocess
import unittest
from unittest.mock import Mock, patch
import run_value_invert_focused_normal as m
import run_filter_owner_context_fixture_test as fixtures

class Checks(unittest.TestCase):
    def test_exact_separate_scopes(self):
        self.assertEqual(len(m.selected_paths()), 21)
        self.assertEqual(len(set(m.selected_paths())), 21)
        self.assertEqual(len(m.selected_paths(True)), 18)
        self.assertTrue(set(m.selected_paths(True)) < set(m.selected_paths()))

    def test_exact_success(self):
        path = '/gimp-filter-layer/value_invert_integer_native_route'
        self.assertTrue(m.passed_result(0, '1..1\nok 1 '+path+'\n', '', path)[0])
        for code, out, err in ((1, '1..1\nok 1 '+path+'\n', ''),
                                (0, '', ''), (0, '1..0\n', ''),
                                (0, '1..1\nok 1 '+path+' # SKIP\n', ''),
                                (0, '1..1\nok 1 '+path+'\n', 'CRITICAL'),
                                (0, '1..1\nok 1 '+path+'\n', 'WARNING'),
                                (0, '1..1\nok 1 '+path+'\n', 'runtime error: failure')):
            self.assertFalse(m.passed_result(code, out, err, path)[0])

    def test_timeout_kills_owned_process_group(self):
        process = Mock(pid=12345, returncode=-9)
        process.communicate.side_effect = [subprocess.TimeoutExpired('mock', 1), ('out', 'err')]
        with patch.object(m.subprocess, 'Popen', return_value=process), patch.object(m.os, 'killpg') as kill:
            result = m.execute(['mock'], Path('/tmp'), {}, 1)
        kill.assert_called_once_with(12345, m.signal.SIGKILL)
        self.assertEqual(result['process_status'], 'timeout')
        self.assertEqual(result['stdout'], 'out')

    def test_changed_and_missing_seals(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'source'; path.write_text('before')
            before = m.seals([path]); path.write_text('after')
            self.assertEqual(m.reseal(before)['changed'], [str(path)])
            path.unlink()
            self.assertEqual(m.reseal(before)['changed'], [str(path)])
            self.assertIn(str(path), m.reseal(before)['read_errors'])

    def test_fixture_snapshot_rejects_empty_or_symlink(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with self.assertRaisesRegex(RuntimeError, 'empty'):
                m.fixture_snapshot(root)
            target = root/'capture'; target.write_text('sealed')
            self.assertEqual(len(m.fixture_snapshot(root)), 1)
            (root/'link').symlink_to(target)
            with self.assertRaisesRegex(RuntimeError, 'link'):
                m.fixture_snapshot(root)

    def test_fixture_roots_match_sealed_materialization(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            environment = {}
            for i, variable in enumerate(m.FIXTURES):
                directory = root/str(i); directory.mkdir()
                (directory/'capture').write_text('sealed')
                environment[variable] = str(directory)
            def verifier(extract):
                extract.mkdir(); (extract/'capture').write_text('sealed')
                return {'verified': True}
            with patch('check_value_invert_evidence.verify', side_effect=verifier), patch('check_value_invert_projection_evidence.verify', side_effect=verifier):
                summaries, before = m.verify_fixture_roots(environment)
                self.assertEqual(len(summaries), 2); self.assertEqual(len(before), 2)
                (root/'1'/'capture').write_text('changed')
                with self.assertRaisesRegex(RuntimeError, 'differs from sealed corpus'):
                    m.verify_fixture_roots(environment)

    def fixture_mock_stack(self, stack):
        stack.enter_context(patch.object(fixtures, 'materialize_owner_context_fixtures', return_value={}))
        for name in dir(fixtures):
            if name.startswith('verify_') and not name.startswith('verify_value_invert'):
                stack.enter_context(patch.object(fixtures, name))

    def test_fixture_launcher_exports_both_verified_roots(self):
        with contextlib.ExitStack() as stack:
            self.fixture_mock_stack(stack)
            base = stack.enter_context(patch.object(fixtures, 'verify_value_invert'))
            history = stack.enter_context(patch.object(fixtures, 'verify_value_invert_projection'))
            launch = stack.enter_context(patch.object(fixtures.subprocess, 'run', return_value=Mock(returncode=0)))
            self.assertEqual(fixtures.main(['MOCK-NATIVE']), 0)
            env = launch.call_args.kwargs['env']
            self.assertEqual(env['GIMP_TEST_VALUE_INVERT_FIXTURE_DIR'], str(base.call_args.kwargs['extract']))
            self.assertEqual(env['GIMP_TEST_VALUE_INVERT_PROJECTION_FIXTURE_DIR'], str(history.call_args.kwargs['extract']))

    def test_fixture_launcher_fails_closed_before_native_execution(self):
        with contextlib.ExitStack() as stack:
            self.fixture_mock_stack(stack)
            stack.enter_context(patch.object(fixtures, 'verify_value_invert', side_effect=ValueError('unsealed')))
            history = stack.enter_context(patch.object(fixtures, 'verify_value_invert_projection'))
            launch = stack.enter_context(patch.object(fixtures.subprocess, 'run'))
            with self.assertRaisesRegex(ValueError, 'unsealed'):
                fixtures.main(['MOCK-NATIVE'])
            launch.assert_not_called(); history.assert_not_called()

if __name__ == '__main__':
    unittest.main(verbosity=2)
