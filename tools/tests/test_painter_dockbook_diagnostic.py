#!/usr/bin/env python3
"""Bounded local Git negative controls for the diagnostic-only test overlay."""
from __future__ import annotations

import contextlib
import copy
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock


spec = importlib.util.spec_from_file_location(
    'dockbook_diagnostic', Path(__file__).parents[1] / 'prepare-painter-dockbook-diagnostic.py')
diagnostic = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diagnostic)

ORIGINAL = b'void original_assertions_stay_identical ();\n'
INSTRUMENTED = ORIGINAL + b'// Reviewed opt-in diagnostic instrumentation.\n'
PRODUCTION = 'app/widgets/production.c'
OTHER_TEST = 'app/tests/other-test.cpp'


def run_git(repository, *args):
    return subprocess.check_output(
        ['git', '-C', str(repository), '-c', 'user.name=Diagnostic Tests',
         '-c', 'user.email=diagnostic-tests@example.invalid', *args],
        stderr=subprocess.PIPE)


class PinnedIdentitiesTests(unittest.TestCase):
    def test_production_and_original_test_pins_are_immutable(self):
        self.assertEqual(diagnostic.SOURCE_COMMIT, '3d6f62235c6bc9e2505d2c2b6ad1d42e15d081af')
        self.assertEqual(diagnostic.SOURCE_TREE, 'a5a14a81a10857834301888a0cd2b3cf902bf17d')
        self.assertEqual(diagnostic.DONOR_COMMIT, '1618cc5958148c7d534f9e3893681e14fcc94d80')
        self.assertEqual(diagnostic.DONOR_TREE, '253fcbdeb7c39391ddf8bad89b7877e1e18911b8')
        self.assertEqual(diagnostic.DONOR_TEST_BLOB, 'e3b3b28d1da397282dface38c04ed43d9fe1f0c7')
        self.assertEqual(diagnostic.ORIGINAL_TEST_SHA256,
                         '49eb5a701b4f0a082c1f88f6a2f3a34546c738dfec4de1d28bf7989b12d5d664')
        self.assertEqual(diagnostic.INSTRUMENTED_TEST_SHA256,
                         '78079b8209f0bb4a06f9c44ef15a2ee393c3d917da7ecad213cee8b259c61152')
        self.assertEqual(diagnostic.TEST_PATH, 'app/tests/test-common-layer-popup.cpp')
        self.assertEqual(diagnostic.DIAGNOSTIC_MODES, ('passive', 'controlled-order'))
        self.assertEqual(diagnostic.RUNTIME_FLAG, 'GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES')

    def test_unreviewed_or_invalid_instrumentation_pin_fails_closed(self):
        for value in (None, '', 'f' * 63, 'G' * 64, diagnostic.ORIGINAL_TEST_SHA256, 42):
            with self.subTest(value=value), mock.patch.object(diagnostic, 'INSTRUMENTED_TEST_SHA256', value):
                with self.assertRaises(ValueError):
                    diagnostic.expected_receipt()


class DiagnosticOverlayTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='dockbook-diagnostic-tests-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / 'source'
        self.source.mkdir()
        self.donor = self.root / 'donor'
        self.donor.mkdir()
        self.output = self.root / 'diagnostic-receipt.json'
        for relative, data in ((diagnostic.TEST_PATH, ORIGINAL),
                               (PRODUCTION, b'production stays unchanged\n'),
                               (OTHER_TEST, b'other assertions stay unchanged\n'),
                               ('.gitignore', b'ignored-file\n')):
            target = self.source / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        (self.source / 'tracked-link').symlink_to(PRODUCTION)
        run_git(self.source, 'init', '-q')
        run_git(self.source, 'add', '.')
        run_git(self.source, 'commit', '-qm', 'immutable test fixture')
        self.commit = run_git(self.source, 'rev-parse', 'HEAD').decode().strip()
        self.tree = run_git(self.source, 'rev-parse', 'HEAD^{tree}').decode().strip()
        donor_test = self.donor / diagnostic.TEST_PATH
        donor_test.parent.mkdir(parents=True)
        donor_test.write_bytes(INSTRUMENTED)
        # These intentionally different donor sources must never be imported.
        (self.donor / PRODUCTION).parent.mkdir(parents=True)
        (self.donor / PRODUCTION).write_bytes(b'not approved production\n')
        (self.donor / OTHER_TEST).write_bytes(b'not approved assertions\n')
        run_git(self.donor, 'init', '-q')
        run_git(self.donor, 'add', '.')
        # A pinned but uninitialized donor gitlink must not be needed or read.
        run_git(self.donor, 'update-index', '--add', '--cacheinfo', '160000,' + 'a' * 40 + ',gimp-data')
        run_git(self.donor, 'commit', '-qm', 'immutable diagnostic donor fixture')
        self.donor_commit = run_git(self.donor, 'rev-parse', 'HEAD').decode().strip()
        self.donor_tree = run_git(self.donor, 'rev-parse', 'HEAD^{tree}').decode().strip()
        self.donor_blob = run_git(self.donor, 'rev-parse', 'HEAD:' + diagnostic.TEST_PATH).decode().strip()
        pins = mock.patch.multiple(diagnostic, SOURCE_COMMIT=self.commit,
                                   SOURCE_TREE=self.tree,
                                   DONOR_COMMIT=self.donor_commit,
                                   DONOR_TREE=self.donor_tree,
                                   DONOR_TEST_BLOB=self.donor_blob,
                                   ORIGINAL_TEST_SHA256=diagnostic.sha(ORIGINAL),
                                   INSTRUMENTED_TEST_SHA256=diagnostic.sha(INSTRUMENTED))
        pins.start()
        self.addCleanup(pins.stop)

    def prepare(self):
        return diagnostic.prepare(self.source, self.donor, self.output)

    def rejected_before_overlay(self):
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertEqual((self.source / diagnostic.TEST_PATH).read_bytes(), ORIGINAL)
        self.assertFalse(self.output.exists())

    def add_pinned_submodule(self):
        submodule = self.source / 'gimp-data'
        submodule.mkdir()
        (submodule / 'asset.txt').write_bytes(b'pinned asset\n')
        (submodule / '.gitignore').write_bytes(b'ignored-asset\n')
        run_git(submodule, 'init', '-q')
        run_git(submodule, 'add', '.')
        run_git(submodule, 'commit', '-qm', 'pinned submodule fixture')
        run_git(self.source, 'add', 'gimp-data')
        run_git(self.source, 'commit', '-qm', 'pin submodule')
        diagnostic.SOURCE_COMMIT = run_git(self.source, 'rev-parse', 'HEAD').decode().strip()
        diagnostic.SOURCE_TREE = run_git(self.source, 'rev-parse', 'HEAD^{tree}').decode().strip()
        return submodule

    def test_round_trip_is_diagnostic_only_and_imports_only_exact_test(self):
        receipt = self.prepare()
        self.assertEqual(receipt, json.loads(self.output.read_text()))
        self.assertTrue(diagnostic.verify_worktree(self.source, receipt))
        self.assertIs(receipt['diagnostic_only'], True)
        self.assertIs(receipt['acceptance_eligible'], False)
        self.assertIs(receipt['source_guard_suppressed'], False)
        self.assertEqual(receipt['original_test_sha256'], diagnostic.sha(ORIGINAL))
        self.assertEqual(receipt['instrumented_test_sha256'], diagnostic.sha(INSTRUMENTED))
        self.assertEqual(receipt['donor_commit'], self.donor_commit)
        self.assertEqual(receipt['donor_tree'], self.donor_tree)
        self.assertEqual(receipt['donor_test_blob'], self.donor_blob)
        self.assertIn('donor production', receipt['donor_scope'])
        self.assertIn('not imported or attested', receipt['donor_scope'])
        self.assertEqual(receipt['only_allowed_path'], diagnostic.TEST_PATH)
        self.assertEqual(run_git(self.source, 'diff', '--name-only', 'HEAD').decode().splitlines(),
                         [diagnostic.TEST_PATH])
        self.assertEqual((self.source / PRODUCTION).read_bytes(), b'production stays unchanged\n')
        self.assertEqual((self.source / OTHER_TEST).read_bytes(), b'other assertions stay unchanged\n')
        self.assertFalse(run_git(self.source, 'ls-files', '--others', '-z'))

    def test_default_receipt_declares_passive_and_requires_flag_unset(self):
        receipt = self.prepare()
        self.assertEqual(receipt['format'], 3)
        self.assertEqual(receipt['diagnostic_mode'], 'passive')
        self.assertEqual(receipt['runtime_flag'], 'GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES')
        self.assertIsNone(receipt['runtime_flag_required_value'])
        self.assertTrue(diagnostic.verify_worktree(self.source, receipt, 'passive'))

    def test_controlled_order_round_trip_has_distinct_exact_receipt(self):
        receipt = diagnostic.prepare(self.source, self.donor, self.output, 'controlled-order')
        self.assertEqual(receipt['diagnostic_mode'], 'controlled-order')
        self.assertEqual(receipt['runtime_flag_required_value'], '1')
        self.assertIs(receipt['diagnostic_only'], True)
        self.assertIs(receipt['acceptance_eligible'], False)
        self.assertIs(receipt['source_guard_suppressed'], False)
        self.assertEqual(receipt, json.loads(self.output.read_text()))
        self.assertTrue(diagnostic.verify_worktree(self.source, receipt, 'controlled-order'))
        self.assertEqual((self.source / PRODUCTION).read_bytes(), b'production stays unchanged\n')
        self.assertEqual((self.source / OTHER_TEST).read_bytes(), b'other assertions stay unchanged\n')

    def test_receipt_modes_are_not_interchangeable(self):
        receipt = self.prepare()
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt, 'controlled-order')
        controlled = diagnostic.expected_receipt('controlled-order')
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, controlled, 'passive')
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, controlled)

    def test_rejects_partial_mode_or_runtime_flag_tampering(self):
        self.prepare()
        for mode in diagnostic.DIAGNOSTIC_MODES:
            original = diagnostic.expected_receipt(mode)
            invalid_flags = ('', '0', 'true', True, 1)
            invalid_flags += (None,) if mode == 'controlled-order' else ('1',)
            for value in invalid_flags:
                changed = {**original, 'runtime_flag_required_value': value}
                with self.subTest(mode=mode, flag=value), self.assertRaises(ValueError):
                    diagnostic.verify_worktree(self.source, changed, mode)
            for changed in ({**original, 'runtime_flag': 'OTHER_FLAG'},
                            {**original, 'diagnostic_mode': 'controlled-order' if mode == 'passive' else 'passive'}):
                with self.subTest(mode=mode, receipt=changed), self.assertRaises(ValueError):
                    diagnostic.verify_worktree(self.source, changed, mode)

    def test_rejects_legacy_receipt_without_runtime_mode(self):
        receipt = self.prepare()
        legacy = {key: value for key, value in receipt.items()
                  if key not in {'diagnostic_mode', 'runtime_flag', 'runtime_flag_required_value'}}
        legacy['format'] = 1
        for mode in diagnostic.DIAGNOSTIC_MODES:
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                diagnostic.verify_worktree(self.source, legacy, mode)

    def test_rejects_unknown_mode_before_overlay(self):
        for mode in ('', 'active', 'controlled', 'PASSIVE', None, 1, True):
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                diagnostic.prepare(self.source, self.donor, self.output, mode)
            self.assertEqual((self.source / diagnostic.TEST_PATH).read_bytes(), ORIGINAL)
            self.assertFalse(self.output.exists())

    def test_preparation_declares_but_does_not_enable_runtime_flag(self):
        with mock.patch.dict(os.environ, {}, clear=True):
            diagnostic.prepare(self.source, self.donor, self.output, 'controlled-order')
            self.assertNotIn(diagnostic.RUNTIME_FLAG, os.environ)
        with mock.patch.dict(os.environ, {diagnostic.RUNTIME_FLAG: 'sentinel'}):
            diagnostic.verify_worktree(self.source, diagnostic.expected_receipt('controlled-order'),
                                       'controlled-order')
            self.assertEqual(os.environ[diagnostic.RUNTIME_FLAG], 'sentinel')

    def test_controlled_order_rejects_production_drift_after_overlay(self):
        receipt = diagnostic.prepare(self.source, self.donor, self.output, 'controlled-order')
        (self.source / PRODUCTION).write_bytes(b'controlled build modified production\n')
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt, 'controlled-order')

    def test_controlled_order_rejects_unpinned_instrumentation(self):
        with mock.patch.object(diagnostic, 'INSTRUMENTED_TEST_SHA256', None):
            with self.assertRaises(ValueError):
                diagnostic.prepare(self.source, self.donor, self.output, 'controlled-order')
            with self.assertRaises(ValueError):
                diagnostic.verify_worktree(self.source, {}, 'controlled-order')
        self.assertEqual((self.source / diagnostic.TEST_PATH).read_bytes(), ORIGINAL)
        self.assertFalse(self.output.exists())

    def test_rejects_wrong_commit(self):
        with mock.patch.object(diagnostic, 'SOURCE_COMMIT', '0' * 40):
            self.rejected_before_overlay()

    def test_rejects_wrong_tree(self):
        with mock.patch.object(diagnostic, 'SOURCE_TREE', '0' * 40):
            self.rejected_before_overlay()

    def test_rejects_wrong_original_hash(self):
        with mock.patch.object(diagnostic, 'ORIGINAL_TEST_SHA256', '0' * 64):
            self.rejected_before_overlay()

    def test_rejects_wrong_instrumented_hash(self):
        with mock.patch.object(diagnostic, 'INSTRUMENTED_TEST_SHA256', '0' * 64):
            self.rejected_before_overlay()

    def test_rejects_wrong_donor_commit(self):
        with mock.patch.object(diagnostic, 'DONOR_COMMIT', '0' * 40):
            self.rejected_before_overlay()

    def test_rejects_wrong_donor_tree(self):
        with mock.patch.object(diagnostic, 'DONOR_TREE', '0' * 40):
            self.rejected_before_overlay()

    def test_rejects_wrong_donor_test_blob(self):
        with mock.patch.object(diagnostic, 'DONOR_TEST_BLOB', '0' * 40):
            self.rejected_before_overlay()

    def test_rejects_new_donor_commit_with_same_test_bytes(self):
        run_git(self.donor, 'commit', '--allow-empty', '-qm', 'evolving normal candidate')
        self.rejected_before_overlay()

    def test_rejects_modified_donor_test_bytes(self):
        (self.donor / diagnostic.TEST_PATH).write_bytes(INSTRUMENTED + b'unreviewed change\n')
        self.rejected_before_overlay()

    def test_rejects_missing_donor_test(self):
        (self.donor / diagnostic.TEST_PATH).unlink()
        self.rejected_before_overlay()

    def test_rejects_executable_donor_test(self):
        (self.donor / diagnostic.TEST_PATH).chmod(0o755)
        self.rejected_before_overlay()

    def test_rejects_donor_test_index_drift_even_if_raw_file_restored(self):
        donor_test = self.donor / diagnostic.TEST_PATH
        donor_test.write_bytes(b'unreviewed staged test\n')
        run_git(self.donor, 'add', diagnostic.TEST_PATH)
        donor_test.write_bytes(INSTRUMENTED)
        self.rejected_before_overlay()

    def test_rejects_donor_test_index_mode_drift(self):
        run_git(self.donor, 'update-index', '--chmod=+x', diagnostic.TEST_PATH)
        self.rejected_before_overlay()

    def test_rejects_unrelated_donor_index_drift(self):
        (self.donor / PRODUCTION).write_bytes(b'staged unrelated production drift\n')
        run_git(self.donor, 'add', PRODUCTION)
        self.rejected_before_overlay()

    def test_rejects_assume_unchanged_donor_test_drift(self):
        run_git(self.donor, 'update-index', '--assume-unchanged', diagnostic.TEST_PATH)
        (self.donor / diagnostic.TEST_PATH).write_bytes(b'hidden unreviewed test\n')
        self.rejected_before_overlay()

    def test_rejects_skip_worktree_donor_test_drift(self):
        run_git(self.donor, 'update-index', '--skip-worktree', diagnostic.TEST_PATH)
        (self.donor / diagnostic.TEST_PATH).write_bytes(b'hidden unreviewed test\n')
        self.rejected_before_overlay()

    def test_donor_unrelated_worktree_bytes_and_uninitialized_submodule_are_not_imported(self):
        (self.donor / PRODUCTION).unlink()
        (self.donor / OTHER_TEST).write_bytes(b'unrelated donor worktree bytes\n')
        (self.donor / 'untracked').write_bytes(b'unrelated donor file\n')
        self.assertFalse((self.donor / 'gimp-data').exists())
        receipt = self.prepare()
        self.assertTrue(diagnostic.verify_worktree(self.source, receipt))
        self.assertFalse((self.source / 'untracked').exists())
        self.assertEqual((self.source / PRODUCTION).read_bytes(), b'production stays unchanged\n')
        self.assertEqual((self.source / OTHER_TEST).read_bytes(), b'other assertions stay unchanged\n')

    def test_rejects_previous_mode_receipt_without_donor_identity(self):
        receipt = self.prepare()
        prior = {key: value for key, value in receipt.items() if not key.startswith('donor_')}
        prior['format'] = 2
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, prior)

    def test_rejects_nested_donor_path(self):
        with self.assertRaises(ValueError):
            diagnostic.prepare(self.source, self.donor / 'app', self.output)
        self.assertEqual((self.source / diagnostic.TEST_PATH).read_bytes(), ORIGINAL)
        self.assertFalse(self.output.exists())

    def test_rejects_receipt_inside_donor(self):
        with self.assertRaises(ValueError):
            diagnostic.prepare(self.source, self.donor, self.donor / 'receipt.json')
        self.assertEqual((self.source / diagnostic.TEST_PATH).read_bytes(), ORIGINAL)
        self.assertFalse((self.donor / 'receipt.json').exists())

    def test_cli_rejects_obsolete_candidate_option(self):
        with mock.patch('sys.argv', ['prepare', '--source', str(self.source),
                                    '--candidate', str(self.donor), '--output', str(self.output)]):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                diagnostic.main()
            self.assertEqual(error.exception.code, 2)

    def test_rejects_unpinned_instrumentation_before_any_write(self):
        with mock.patch.object(diagnostic, 'INSTRUMENTED_TEST_SHA256', None):
            self.rejected_before_overlay()

    def test_rejects_dirty_original_test_before_overlay(self):
        (self.source / diagnostic.TEST_PATH).write_bytes(b'unreviewed original\n')
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertEqual((self.source / diagnostic.TEST_PATH).read_bytes(), b'unreviewed original\n')
        self.assertFalse(self.output.exists())

    def test_rejects_tracked_production_drift_before_overlay(self):
        (self.source / PRODUCTION).write_bytes(b'drift\n')
        self.rejected_before_overlay()

    def test_rejects_other_test_drift_before_overlay(self):
        (self.source / OTHER_TEST).write_bytes(b'drift\n')
        self.rejected_before_overlay()

    def test_rejects_untracked_before_overlay(self):
        (self.source / 'unexpected').write_bytes(b'drift\n')
        self.rejected_before_overlay()

    def test_rejects_ignored_before_overlay(self):
        (self.source / 'ignored-file').write_bytes(b'drift\n')
        self.rejected_before_overlay()

    def test_rejects_index_changes_even_when_worktree_is_original(self):
        production = self.source / PRODUCTION
        original = production.read_bytes()
        production.write_bytes(b'staged drift\n')
        run_git(self.source, 'add', PRODUCTION)
        production.write_bytes(original)
        self.rejected_before_overlay()

    def test_rejects_assume_unchanged_hidden_drift(self):
        run_git(self.source, 'update-index', '--assume-unchanged', PRODUCTION)
        (self.source / PRODUCTION).write_bytes(b'hidden drift\n')
        self.rejected_before_overlay()

    def test_rejects_skip_worktree_hidden_drift(self):
        run_git(self.source, 'update-index', '--skip-worktree', PRODUCTION)
        (self.source / PRODUCTION).write_bytes(b'hidden drift\n')
        self.rejected_before_overlay()

    def test_rejects_missing_sparse_tracked_file(self):
        run_git(self.source, 'update-index', '--skip-worktree', PRODUCTION)
        (self.source / PRODUCTION).unlink()
        self.rejected_before_overlay()

    def test_rejects_wrong_source_root(self):
        with self.assertRaises(ValueError):
            diagnostic.prepare(self.source / 'app', self.donor, self.output)
        self.assertFalse(self.output.exists())

    def test_rejects_donor_as_source(self):
        with self.assertRaises(ValueError):
            diagnostic.prepare(self.source, self.source, self.output)
        self.assertFalse(self.output.exists())

    def test_rejects_receipt_inside_source(self):
        with self.assertRaises(ValueError):
            diagnostic.prepare(self.source, self.donor, self.source / 'receipt.json')
        self.assertEqual((self.source / diagnostic.TEST_PATH).read_bytes(), ORIGINAL)
        self.assertFalse((self.source / 'receipt.json').exists())

    def test_refuses_to_overwrite_existing_receipt(self):
        self.output.write_text('keep existing evidence\n')
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertEqual(self.output.read_text(), 'keep existing evidence\n')
        self.assertEqual((self.source / diagnostic.TEST_PATH).read_bytes(), ORIGINAL)

    def test_rejects_donor_symlink(self):
        donor_test = self.donor / diagnostic.TEST_PATH
        donor_test.unlink()
        outside = self.root / 'outside.cpp'
        outside.write_bytes(INSTRUMENTED)
        donor_test.symlink_to(outside)
        self.rejected_before_overlay()

    def test_rejects_donor_symlink_parent(self):
        moved = self.root / 'moved-donor-app'
        (self.donor / 'app').rename(moved)
        (self.donor / 'app').symlink_to(moved, target_is_directory=True)
        self.rejected_before_overlay()

    def test_rejects_hardlinked_target(self):
        os.link(self.source / diagnostic.TEST_PATH, self.root / 'hardlinked.cpp')
        self.rejected_before_overlay()

    def test_rejects_production_drift_after_overlay(self):
        receipt = self.prepare()
        (self.source / PRODUCTION).write_bytes(b'build modified production\n')
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_other_test_drift_after_overlay(self):
        receipt = self.prepare()
        (self.source / OTHER_TEST).write_bytes(b'build modified test\n')
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_instrumented_test_drift_after_overlay(self):
        receipt = self.prepare()
        (self.source / diagnostic.TEST_PATH).write_bytes(INSTRUMENTED + b'drift\n')
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_original_test_restored_after_overlay(self):
        receipt = self.prepare()
        (self.source / diagnostic.TEST_PATH).write_bytes(ORIGINAL)
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_untracked_after_overlay(self):
        receipt = self.prepare()
        (self.source / 'unexpected').write_bytes(b'drift\n')
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_ignored_after_overlay(self):
        receipt = self.prepare()
        (self.source / 'ignored-file').write_bytes(b'drift\n')
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_staging_instrumented_overlay(self):
        receipt = self.prepare()
        run_git(self.source, 'add', diagnostic.TEST_PATH)
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_new_commit_after_overlay(self):
        receipt = self.prepare()
        run_git(self.source, 'commit', '--allow-empty', '-qm', 'unexpected commit')
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_mode_drift(self):
        receipt = self.prepare()
        (self.source / PRODUCTION).chmod(0o755)
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_tracked_symlink_drift(self):
        receipt = self.prepare()
        (self.source / 'tracked-link').unlink()
        (self.source / 'tracked-link').symlink_to(OTHER_TEST)
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_instrumented_source_symlink(self):
        receipt = self.prepare()
        target = self.source / diagnostic.TEST_PATH
        target.unlink()
        target.symlink_to(self.donor / diagnostic.TEST_PATH)
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_every_receipt_identity_or_claim_tamper(self):
        receipt = self.prepare()
        for key in receipt:
            with self.subTest(field=key):
                changed = copy.deepcopy(receipt)
                changed[key] = 'tampered'
                with self.assertRaises(ValueError):
                    diagnostic.verify_worktree(self.source, changed)
        for changed in ({**receipt, 'unexpected': True},
                        {key: value for key, value in receipt.items() if key != 'source_tree'},
                        {**receipt, 'acceptance_eligible': 0},
                        {**receipt, 'diagnostic_only': 1},
                        {**receipt, 'format': True},
                        {**receipt, 'only_allowed_path': '../production.c'}):
            with self.subTest(receipt=changed), self.assertRaises(ValueError):
                diagnostic.verify_worktree(self.source, changed)

    def test_rejects_duplicate_receipt_fields(self):
        with self.assertRaises(ValueError):
            json.loads('{"acceptance_eligible":true,"acceptance_eligible":false}',
                       object_pairs_hook=diagnostic.unique_object)

    def test_rolls_back_if_post_overlay_verification_fails(self):
        with mock.patch.object(diagnostic, 'verify_worktree', side_effect=ValueError('forced failure')):
            self.rejected_before_overlay()

    def test_initialized_pinned_submodule_passes(self):
        self.add_pinned_submodule()
        receipt = self.prepare()
        self.assertTrue(diagnostic.verify_worktree(self.source, receipt))

    def test_rejects_uninitialized_submodule(self):
        submodule = self.add_pinned_submodule()
        shutil.rmtree(submodule)
        submodule.mkdir()
        self.rejected_before_overlay()

    def test_rejects_wrong_submodule_commit(self):
        submodule = self.add_pinned_submodule()
        run_git(submodule, 'commit', '--allow-empty', '-qm', 'wrong submodule commit')
        self.rejected_before_overlay()

    def test_rejects_submodule_tracked_drift_before_overlay(self):
        submodule = self.add_pinned_submodule()
        (submodule / 'asset.txt').write_bytes(b'changed asset\n')
        self.rejected_before_overlay()

    def test_rejects_submodule_untracked_before_overlay(self):
        submodule = self.add_pinned_submodule()
        (submodule / 'unexpected').write_bytes(b'untracked asset\n')
        self.rejected_before_overlay()

    def test_rejects_submodule_ignored_before_overlay(self):
        submodule = self.add_pinned_submodule()
        (submodule / 'ignored-asset').write_bytes(b'ignored asset\n')
        self.rejected_before_overlay()

    def test_rejects_submodule_tracked_drift_after_overlay(self):
        submodule = self.add_pinned_submodule()
        receipt = self.prepare()
        (submodule / 'asset.txt').write_bytes(b'build modified asset\n')
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_submodule_untracked_after_overlay(self):
        submodule = self.add_pinned_submodule()
        receipt = self.prepare()
        (submodule / 'unexpected').write_bytes(b'build added asset\n')
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_rejects_submodule_hidden_byte_drift_after_overlay(self):
        submodule = self.add_pinned_submodule()
        receipt = self.prepare()
        run_git(submodule, 'update-index', '--assume-unchanged', 'asset.txt')
        (submodule / 'asset.txt').write_bytes(b'hidden modified asset\n')
        with self.assertRaises(ValueError):
            diagnostic.verify_worktree(self.source, receipt)

    def test_cli_prepare_and_verify(self):
        with mock.patch('sys.argv', ['prepare', '--source', str(self.source),
                                    '--donor', str(self.donor), '--output', str(self.output)]):
            with contextlib.redirect_stdout(io.StringIO()) as output:
                diagnostic.main()
            self.assertIn('acceptance_eligible=false', output.getvalue())
        with mock.patch('sys.argv', ['prepare', '--source', str(self.source),
                                    '--verify', str(self.output)]):
            with contextlib.redirect_stdout(io.StringIO()) as output:
                diagnostic.main()
            self.assertIn('acceptance_eligible=false', output.getvalue())

    def test_cli_controlled_order_prepare_and_verify(self):
        base = ['prepare', '--source', str(self.source), '--diagnostic-mode', 'controlled-order']
        with mock.patch('sys.argv', [*base, '--donor', str(self.donor), '--output', str(self.output)]):
            with contextlib.redirect_stdout(io.StringIO()) as output:
                diagnostic.main()
            self.assertIn('controlled-order diagnostic-only', output.getvalue())
        with mock.patch('sys.argv', [*base, '--verify', str(self.output)]):
            with contextlib.redirect_stdout(io.StringIO()) as output:
                diagnostic.main()
            self.assertIn('controlled-order diagnostic-only', output.getvalue())
        with mock.patch('sys.argv', ['prepare', '--source', str(self.source), '--verify', str(self.output)]):
            with self.assertRaises(ValueError):
                diagnostic.main()

    def test_cli_rejects_unknown_mode(self):
        with mock.patch('sys.argv', ['prepare', '--source', str(self.source), '--diagnostic-mode', 'active']):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                diagnostic.main()
            self.assertEqual(error.exception.code, 2)

    def test_cli_rejects_ambiguous_or_missing_mode_arguments(self):
        for extras in ([], ['--donor', str(self.donor)],
                       ['--verify', str(self.output), '--donor', str(self.donor)],
                       ['--verify', str(self.output), '--output', str(self.output)]):
            with self.subTest(extras=extras), mock.patch('sys.argv',
                    ['prepare', '--source', str(self.source), *extras]):
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                    diagnostic.main()
                self.assertEqual(error.exception.code, 2)


if __name__ == '__main__':
    unittest.main()
