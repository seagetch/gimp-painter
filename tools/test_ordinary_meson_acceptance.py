#!/usr/bin/env python3
"""Synthetic negative controls; optional real positive acceptance is explicit.

Synthetic containers are parser fixtures only, never native-execution evidence.
No source/build directory is modified and no native program is executed.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import importlib.util
import io
import json
import lzma
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
path = Path(__file__).with_name('check_ordinary_meson_acceptance.py')
spec = importlib.util.spec_from_file_location('ordinary_acceptance_under_test', path)
gate = importlib.util.module_from_spec(spec)
exec(compile(path.read_bytes(), str(path), 'exec'), gate.__dict__)


def tar_bytes(entries, tail=b''):
    stream = io.BytesIO()
    with tarfile.open(fileobj=stream, mode='w', format=tarfile.USTAR_FORMAT) as archive:
        for name, data, kind, link, mode, uid, stamp in entries:
            entry = tarfile.TarInfo(name)
            entry.type, entry.linkname, entry.mode = kind, link, mode
            entry.uid, entry.mtime, entry.size = uid, stamp, len(data)
            archive.addfile(entry, io.BytesIO(data) if kind == tarfile.REGTYPE else None)
    return stream.getvalue() + tail


def entry(name=gate.POLICY_PREFIX + '/policy-disclosure.json', data=b'{}', kind=tarfile.REGTYPE, link='', mode=0o644, uid=0, stamp=0):
    return (name, data, kind, link, mode, uid, stamp)


class SyntheticNegativeControls(unittest.TestCase):
    def reject_tar(self, data, message=None):
        with tempfile.TemporaryDirectory(prefix='ordinary-parser-control-') as temporary:
            with self.assertRaises((ValueError, tarfile.TarError), msg=message):
                gate.extract_tar(io.BytesIO(data), Path(temporary))

    def reject_xz(self, data, expected=None):
        with tempfile.TemporaryDirectory(prefix='ordinary-xz-control-') as temporary:
            archive = Path(temporary) / 'control.tar.xz'
            archive.write_bytes(data)
            with self.assertRaises((ValueError, lzma.LZMAError)):
                gate.decompress(archive, expected or hashlib.sha256(data).hexdigest(), io.BytesIO())

    def test_origin_requires_exact_archive_pin(self):
        with self.assertRaisesRegex(ValueError, 'exact failed origin'):
            gate.decompress(Path('/not-used'), '0' * 64, io.BytesIO(), origin='run-002')

    def test_origin_path_traversal_rejected(self):
        data = tar_bytes([entry('run-002/public/../receipt.json')])
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, 'unsafe archive member'):
                gate.extract_tar(io.BytesIO(data), Path(temporary), origin='run-002')

    def test_origin_original_member_order_required(self):
        names = list(gate.ORIGIN_ORDER)
        names[0], names[1] = names[1], names[0]
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, 'archive ordering'):
                gate.extract_tar(io.BytesIO(tar_bytes([entry(name) for name in names])), Path(temporary), origin='run-002')

    def test_origin_pass_credit_rejected(self):
        receipt = {'origin': {'archive_file': gate.ORIGIN_FILE, 'archive_sha256': gate.ORIGIN_ARCHIVE_SHA256,
            'receipt_sha256': gate.ORIGIN_RECEIPT_SHA256, 'runner_sha256': gate.ORIGIN_RUNNER_SHA256,
            'status': 'failed', 'historical_commands': [], 'provenance_only': True,
            'inherited_test_pass_credit': True, 'fresh_configure_or_compile_in_this_attempt': False}}
        with self.assertRaisesRegex(ValueError, 'inherited pass credit'):
            gate.build_origin_claim_gate(receipt, {'commands': []})

    def test_resource_origin_requires_exact_archive_pin(self):
        with self.assertRaisesRegex(ValueError, 'exact failed resource origin'):
            gate.decompress(Path('/not-used'), '0' * 64, io.BytesIO(), origin='run-003')

    def test_resource_origin_path_traversal_rejected(self):
        data = tar_bytes([entry('run-003/public/../receipt.json')])
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, 'unsafe archive member'):
                gate.extract_tar(io.BytesIO(data), Path(temporary), origin='run-003')

    def test_resource_origin_original_member_order_required(self):
        names = list(gate.RESOURCE_ORIGIN_ORDER)
        names[0], names[1] = names[1], names[0]
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, 'archive ordering'):
                gate.extract_tar(io.BytesIO(tar_bytes([entry(name) for name in names])), Path(temporary), origin='run-003')

    def resource_claim_fixture(self):
        return {'origin': {'archive_file': gate.RESOURCE_ORIGIN_FILE,
            'archive_sha256': gate.RESOURCE_ORIGIN_ARCHIVE_SHA256,
            'receipt_sha256': gate.RESOURCE_ORIGIN_RECEIPT_SHA256,
            'runner_sha256': gate.RESOURCE_ORIGIN_RUNNER_SHA256, 'status': 'failed',
            'historical_commands': [], 'provenance_only': True, 'inherited_test_pass_credit': False,
            'build_commands_in_this_attempt': False, 'nested_build_archive_sha256': gate.ORIGIN_ARCHIVE_SHA256,
            'nested_build_receipt_sha256': gate.ORIGIN_RECEIPT_SHA256}}

    def test_resource_origin_pass_credit_rejected(self):
        receipt = self.resource_claim_fixture()
        receipt['origin']['inherited_test_pass_credit'] = True
        with self.assertRaisesRegex(ValueError, 'inherited pass credit'):
            gate.origin_claim_gate(receipt, {'commands': []})

    def test_current_build_claim_rejected(self):
        for claim in (True, 0, None):
            with self.subTest(claim=claim):
                receipt = self.resource_claim_fixture()
                receipt['origin']['build_commands_in_this_attempt'] = claim
                with self.assertRaisesRegex(ValueError, 'current build claim'):
                    gate.origin_claim_gate(receipt, {'commands': []})

    def test_changed_nested_build_provenance_rejected(self):
        receipt = self.resource_claim_fixture()
        receipt['origin']['nested_build_archive_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'exact failed resource/build provenance'):
            gate.origin_claim_gate(receipt, {'commands': []})

    def test_current_resource_build_command_rejected(self):
        commands = [{'log': name} for name in sorted(gate.COMMANDS)]
        commands.append({'log': 'sanitizer-declared-menu-resources.log'})
        with self.assertRaisesRegex(ValueError, 'no build command'):
            gate.supplemental_facts(Path('/not-used'), {'commands': commands})

    def test_execution_archive_requires_exact_pin(self):
        with self.assertRaisesRegex(ValueError, 'exact blocked run-004'):
            gate.decompress(Path('/not-used'), '0' * 64, io.BytesIO(), origin='run-004')

    def test_execution_archive_path_traversal_rejected(self):
        data = tar_bytes([entry('run-004/public/../receipt.json')])
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, 'unsafe archive member'):
                gate.extract_tar(io.BytesIO(data), Path(temporary), origin='run-004')

    def test_execution_archive_original_order_required(self):
        names = list(gate.EXECUTION_ORDER)
        names[0], names[1] = names[1], names[0]
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, 'archive ordering'):
                gate.extract_tar(io.BytesIO(tar_bytes([entry(name) for name in names])), Path(temporary), origin='run-004')

    def incomplete_disclosure_route_fixture(self):
        # Intentionally incomplete recovery report, never a positive package.
        return {'archive': gate.ARCHIVE, 'archive_sha256': '0' * 64,
            'receipt_sha256': gate.EXECUTION_RECEIPT_SHA256, 'source_commit': gate.COMMIT,
            'source_tree': gate.TREE, 'runner_sha256': gate.RUNNER_SHA256,
            'terminal_exit_code': 1, 'terminal_validation': {'passed': False, 'errors': gate.POLICY_FAILURE,
                'receipt_sha256': gate.EXECUTION_RECEIPT_SHA256}, 'normal_counts': gate.NORMAL,
            'sanitizer_counts': gate.SANITIZER, 'integration_files_sha256': {name: '0' * 64 for name in gate.NEW_FILES},
            'policy_disclosure': {'schema': gate.POLICY_SCHEMA, 'validator_sha256': gate.POLICY_HELPER_SHA256,
                'attestation_sha256': gate.POLICY_ATTESTATION_SHA256, 'validation_sha256': gate.POLICY_VALIDATION_SHA256,
                'terminal_exit_code': 0, 'terminal_validation': {}}}

    def test_original_runner_exit_relabel_rejected(self):
        route = self.incomplete_disclosure_route_fixture()
        route['terminal_exit_code'] = 0
        with self.assertRaisesRegex(ValueError, 'Original runner exit 1'):
            gate.route_gate({'ordinary_meson_supplement': route})

    def test_original_validation_relabel_rejected(self):
        route = self.incomplete_disclosure_route_fixture()
        route['terminal_validation'] = {'passed': True, 'errors': [], 'receipt_sha256': gate.EXECUTION_RECEIPT_SHA256}
        with self.assertRaisesRegex(ValueError, 'failure must remain explicit'):
            gate.route_gate({'ordinary_meson_supplement': route})

    def test_missing_policy_disclosure_route_rejected(self):
        route = self.incomplete_disclosure_route_fixture()
        del route['policy_disclosure']
        with self.assertRaisesRegex(ValueError, 'ordinary supplement route'):
            gate.route_gate({'ordinary_meson_supplement': route})

    def test_changed_policy_disclosure_helper_pin_rejected(self):
        route = self.incomplete_disclosure_route_fixture()
        route['policy_disclosure']['validator_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'disclosure pins differ'):
            gate.route_gate({'ordinary_meson_supplement': route})

    def test_noninteger_recovery_exit_rejected(self):
        route = self.incomplete_disclosure_route_fixture()
        route['policy_disclosure']['terminal_exit_code'] = False
        with self.assertRaisesRegex(ValueError, 'integer exit zero'):
            gate.route_gate({'ordinary_meson_supplement': route})

    def test_claimed_recovery_pass_without_exact_report_rejected(self):
        route = self.incomplete_disclosure_route_fixture()
        route['policy_disclosure']['terminal_validation'] = {'passed': True, 'policy_adapter_uses': 2}
        with self.assertRaisesRegex(ValueError, 'exact observed successful disclosure report'):
            gate.route_gate({'ordinary_meson_supplement': route})

    def test_changed_policy_helper_rejected_before_import(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            recovery = root / gate.POLICY_PREFIX
            recovery.mkdir()
            for name in gate.POLICY_FILES:
                (recovery / name).write_bytes(b'invalid fixture')
            with patch.object(gate, 'load_verified', side_effect=AssertionError('Must not import changed helper')):
                with self.assertRaisesRegex(ValueError, 'Changed policy-disclosure member'):
                    gate.package_gate(root, Path('/not-used'), {})

    def test_changed_compile_database_raw_seal_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'normal-compile_commands.json').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'exact raw metadata seal'):
                gate.metadata_seals_gate(root, 'normal', {'compile_commands.json': '0' * 64},
                    {'normal-compile_commands.json': {'raw_sha256': '0' * 64}}, {'build_metadata': {}})

    def test_other_metadata_still_requires_freshness_seal(self):
        with self.assertRaises(KeyError):
            gate.metadata_seals_gate(Path('/not-used'), 'normal', {'config.h': '0' * 64},
                {'normal-config.h': {'raw_sha256': '0' * 64}}, {'build_metadata': {}})

    def test_loader_mapping_mismatch_rejected(self):
        loader = {'synthetic-target': {'method': 'ldd with exact private Meson-result environment',
                  'ldd_output': 'synthetic line', 'libraries': {'libfixture.so': {'path': '/expected'}}}}
        with self.assertRaisesRegex(ValueError, 'recorded ldd output'):
            gate.loader_text_gate(loader, lambda line: ('libfixture.so', '/changed'))

    def test_missing_toolchain_entry_rejected(self):
        with self.assertRaisesRegex(ValueError, 'toolchain entry'):
            gate.toolchain_gate({name: {} for name in ('cc', 'c++', 'meson', 'ninja', 'ar', 'nm', 'git')})

    def test_missing_result_transform_rejected(self):
        transforms = {name: {} for name in gate.TRANSFORM_MEMBERS if name != 'normal-results.json'}
        with self.assertRaisesRegex(ValueError, 'transformation provenance'):
            gate.transformations_gate(Path('/not-used'), {}, transforms)

    def test_shortened_runtime_object_scope_rejected(self):
        inst = {'objects': {'synthetic-runtime-%d' % i: {} for i in range(1372)},
                'build_only_objects': {'synthetic-build-%d' % i: {} for i in range(3)},
                'role_evidence': {'roles': {}}}
        database = [{'output': name} for name in inst['objects'] | inst['build_only_objects']]
        with self.assertRaisesRegex(ValueError, '1373 runtime'):
            gate.role_inventory_gate(inst, database)

    def test_missing_ordinary_route_even_with_focused_evidence(self):
        errors = gate.validate(matrix={}, current_evidence=Path('/nonexistent-focused-receipt'))
        self.assertTrue(errors)
        self.assertIn('ordinary supplement route', errors[0])

    def test_green_historical_claim_does_not_supply_ordinary_route(self):
        with self.assertRaises(ValueError):
            gate.route_gate({'current_core_receipt': {'status': 'passed'}})

    def test_pending_route_rejected(self):
        with self.assertRaises(ValueError):
            gate.route_gate({'ordinary_meson_supplement': {'status': 'pending'}})

    def test_failed_route_rejected(self):
        with self.assertRaises(ValueError):
            gate.route_gate({'ordinary_meson_supplement': {'status': 'failed'}})

    def test_duplicate_json_keys_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'duplicate.json'
            path.write_text('{"status":"failed","status":"passed"}')
            with self.assertRaises(ValueError):
                gate.read(path)

    def test_failed_terminal_receipt_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'receipt.json'
            path.write_text('{"status":"failed","errors":["synthetic negative control"]}')
            with self.assertRaisesRegex(ValueError, 'failed/pending'):
                gate.execution_package_gate(Path(temporary), Path('/not-used'), {'receipt_sha256': gate.sha(path)})

    def test_running_terminal_receipt_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'receipt.json'
            path.write_text('{"status":"running","errors":[]}')
            with self.assertRaisesRegex(ValueError, 'failed/pending'):
                gate.execution_package_gate(Path(temporary), Path('/not-used'), {'receipt_sha256': gate.sha(path)})

    def test_changed_receipt_pin_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            (Path(temporary) / 'receipt.json').write_text('{"status":"failed"}')
            with self.assertRaisesRegex(ValueError, 'receipt pin'):
                gate.execution_package_gate(Path(temporary), Path('/not-used'), {'receipt_sha256': '0' * 64})

    def test_missing_members_rejected(self):
        self.reject_tar(tar_bytes([entry()]))

    def test_unknown_member_rejected(self):
        self.reject_tar(tar_bytes([entry('extra.json')]))

    def test_parent_traversal_rejected(self):
        self.reject_tar(tar_bytes([entry('../receipt.json')]))

    def test_absolute_path_rejected(self):
        self.reject_tar(tar_bytes([entry('/receipt.json')]))

    def test_dot_alias_rejected(self):
        self.reject_tar(tar_bytes([entry('./receipt.json')]))

    def test_double_separator_rejected(self):
        self.reject_tar(tar_bytes([entry('helpers//tools/clone_evidence.py')]))

    def test_backslash_rejected(self):
        self.reject_tar(tar_bytes([entry('helpers\\tools\\clone_evidence.py')]))

    def test_duplicate_member_rejected(self):
        self.reject_tar(tar_bytes([entry(), entry()]))

    def test_symlink_rejected(self):
        self.reject_tar(tar_bytes([entry(kind=tarfile.SYMTYPE, link='outside')]))

    def test_hardlink_rejected(self):
        self.reject_tar(tar_bytes([entry(kind=tarfile.LNKTYPE, link='outside')]))

    def test_device_rejected(self):
        self.reject_tar(tar_bytes([entry(kind=tarfile.CHRTYPE)]))

    def test_directory_header_rejected(self):
        self.reject_tar(tar_bytes([entry('helpers/', kind=tarfile.DIRTYPE)]))

    def test_executable_member_rejected(self):
        self.reject_tar(tar_bytes([entry(mode=0o755)]))

    def test_nondeterministic_owner_rejected(self):
        self.reject_tar(tar_bytes([entry(uid=1)]))

    def test_nondeterministic_time_rejected(self):
        self.reject_tar(tar_bytes([entry(stamp=1)]))

    def test_oversize_member_rejected(self):
        with patch.object(gate, 'MAX_MEMBER', 1):
            self.reject_tar(tar_bytes([entry(data=b'ab')]))

    def test_truncated_header_rejected(self):
        self.reject_tar(b'x' * 511)

    def test_extra_tar_data_rejected(self):
        self.reject_tar(tar_bytes([], b'not-padding'))

    def test_concatenated_xz_rejected(self):
        self.reject_xz(lzma.compress(b'one') + lzma.compress(b'two'))

    def test_xz_trailing_bytes_rejected(self):
        self.reject_xz(lzma.compress(b'one') + b'extra')

    def test_truncated_xz_rejected(self):
        self.reject_xz(lzma.compress(b'one')[:-4])

    def test_changed_archive_pin_rejected(self):
        self.reject_xz(lzma.compress(b'one'), '0' * 64)

    def test_compressed_limit_rejected(self):
        with patch.object(gate, 'MAX_COMPRESSED', 1):
            self.reject_xz(lzma.compress(b'one'))

    def test_expanded_limit_rejected(self):
        with patch.object(gate, 'MAX_EXPANDED', 1):
            self.reject_xz(lzma.compress(b'one'))

    def test_changed_source_blob_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'source.c'
            path.write_bytes(b'changed\n')
            path.chmod(0o644)
            with self.assertRaisesRegex(ValueError, 'frozen Git object'):
                gate.file_seal(path, '100644', hashlib.sha1(b'blob 9\0original\n').hexdigest())

    def test_source_mode_change_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'source.c'
            path.write_bytes(b'content')
            path.chmod(0o755)
            with self.assertRaisesRegex(ValueError, 'executable mode'):
                gate.file_seal(path, '100644', hashlib.sha1(b'blob 7\0content').hexdigest())

    def test_extra_publication_file_rejected(self):
        responses = {('rev-parse', gate.COMMIT + '^{tree}'): gate.TREE.encode(),
                     ('ls-tree', '-r', '-z', gate.COMMIT): b'100644 blob ' + b'a' * 40 + b'\tfile.c\0',
                     ('ls-files', '--stage', '-z'): b'100644 ' + b'a' * 40 + b' 0\tfile.c\0'
                        + b'100644 ' + b'b' * 40 + b' 0\textra.c\0'}
        with patch.object(gate, 'git', side_effect=lambda root, *args: responses[args]):
            with self.assertRaisesRegex(ValueError, 'extra tracked'):
                gate.source_gate(Path('/synthetic'), gate.COMMIT, gate.TREE, current=True, pins={})

    def test_duplicate_command_phase_rejected(self):
        commands = [{'log': name} for name in sorted(gate.COMMANDS)]
        commands.append(commands[0])
        with self.assertRaisesRegex(ValueError, 'exactly once'):
            gate.supplemental_facts(Path('/not-used'), {'commands': commands})

    def test_omitted_command_phase_rejected(self):
        commands = [{'log': name} for name in sorted(gate.COMMANDS)[:-1]]
        with self.assertRaisesRegex(ValueError, 'exactly once'):
            gate.supplemental_facts(Path('/not-used'), {'commands': commands})





class SourcePhaseNegativeControls(unittest.TestCase):
    def setUp(self):
        self.source = {label: {phase: {'synthetic': 'source seal fixture'} for phase in gate.PHASES}
                       for label in ('normal', 'sanitizer')}
        self.observations = {label: {'observed_phases': sorted(gate.PHASES), 'missing_phases': [],
                                    'observed_sources_unchanged': True} for label in self.source}

    def test_missing_source_phase(self):
        del self.source['sanitizer']['before_tests']
        with self.assertRaises(ValueError):
            gate.source_phases_gate(self.source, self.observations)

    def test_changed_source_phase(self):
        self.source['sanitizer']['after'] = {'synthetic': 'changed seal'}
        with self.assertRaises(ValueError):
            gate.source_phases_gate(self.source, self.observations)

    def test_extra_source_phase(self):
        self.source['normal']['unapproved'] = self.source['normal']['before_build']
        with self.assertRaises(ValueError):
            gate.source_phases_gate(self.source, self.observations)

    def test_incomplete_phase_observation(self):
        self.observations['sanitizer']['missing_phases'] = ['before_tests']
        with self.assertRaises(ValueError):
            gate.source_phases_gate(self.source, self.observations)


class DocumentarySeparationControls(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='ordinary-documentary-control-')
        self.addCleanup(self.temporary.cleanup)
        self.frozen = Path(self.temporary.name) / 'frozen'
        self.current = Path(self.temporary.name) / 'current'
        for root in (self.frozen, self.current):
            (root / 'migration').mkdir(parents=True)
        baseline = ('| 25.001 | [ ] | Covered task | 02.015 | Exact criterion |\n'
                    '| 02.015 | [x] | Prerequisite | - | Prerequisite criterion |\n'
                    '| 99.999 | [ ] | Unrelated task | - | Other criterion |\n')
        (self.frozen / 'tasks.md').write_text(baseline)
        (self.current / 'tasks.md').write_text(baseline.replace('| 25.001 | [ ] |', '| 25.001 | [x] |'))
        (self.current / 'migration/progress.tsv').write_text('\t'.join(
            ['25.001', 'DONE', 'Owner', gate.ARCHIVE, 'synthetic fixture', 'synthetic fixture', '-', 'synthetic fixture']) + '\n')
        self.patch = patch.object(gate, 'TASKS', {'25.001'})
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def test_unrelated_future_documentary_changes_allowed(self):
        path = self.current / 'tasks.md'
        path.write_text(path.read_text().replace('| 99.999 | [ ] | Unrelated task |', '| 99.999 | [x] | Updated unrelated task |'))
        with (self.current / 'migration/progress.tsv').open('a') as stream:
            stream.write('99.999\tDONE\tNew unrelated progress\n')
        gate.covered_tasks_gate(self.current, self.frozen)

    def test_closure_audit_rejects_unrelated_change(self):
        path = self.current / 'tasks.md'
        path.write_text(path.read_text().replace('Unrelated task', 'Edited unrelated task'))
        with self.assertRaisesRegex(ValueError, 'exact 17 task checkboxes'):
            gate.closure_audit(self.current, self.frozen, {}, {})

    def test_covered_criterion_change_rejected(self):
        path = self.current / 'tasks.md'
        path.write_text(path.read_text().replace('Exact criterion', 'Changed criterion'))
        with self.assertRaisesRegex(ValueError, 'criterion changed'):
            gate.covered_tasks_gate(self.current, self.frozen)

    def test_unchecked_covered_dependency_rejected(self):
        path = self.current / 'tasks.md'
        path.write_text(path.read_text().replace('| 02.015 | [x] |', '| 02.015 | [ ] |'))
        with self.assertRaisesRegex(ValueError, 'Unmet covered dependency'):
            gate.covered_tasks_gate(self.current, self.frozen)


# These controls invoke the exact pinned runner/helper logic with deliberately
# invalid in-memory TAP/flag fixtures. They are not native test observations.
RUNNER = CLONE = AGGREGATE = EXPECTED = None


class ReviewedRunnerNegativeControls(unittest.TestCase):
    def setUp(self):
        self.name = 'painter-navigation'
        self.registry = [{'name': self.name, 'suite': [], 'cmd': ['/synthetic/never-executed']}]
        self.result = {'name': self.name, 'command': self.registry[0]['cmd'], 'result': 'OK',
                       'returncode': 0, 'stdout': '1..6\n' + ''.join(
                           'ok %d %s\n' % (i, name) for i, name in enumerate(EXPECTED[self.name], 1)), 'stderr': ''}

    def reject(self, results):
        with self.assertRaises(RuntimeError):
            RUNNER.tap_gate(CLONE, AGGREGATE, self.registry, results, EXPECTED, [self.name])

    def test_omitted_tap_case(self):
        self.result['stdout'] = self.result['stdout'].rsplit('ok 6 ', 1)[0]
        self.reject([self.result])

    def test_extra_tap_case(self):
        self.result['stdout'] += 'ok 7 /extra\n'
        self.reject([self.result])

    def test_duplicate_tap_case(self):
        names = EXPECTED[self.name]
        self.result['stdout'] = self.result['stdout'].replace(names[1], names[0])
        self.reject([self.result])

    def test_skipped_tap_case(self):
        self.result['stdout'] = self.result['stdout'].rstrip() + ' # SKIP\n'
        self.reject([self.result])

    def test_todo_tap_case(self):
        self.result['stdout'] = self.result['stdout'].rstrip() + ' # TODO\n'
        self.reject([self.result])

    def test_wrong_tap_plan(self):
        self.result['stdout'] = self.result['stdout'].replace('1..6', '1..5')
        self.reject([self.result])

    def test_wrong_tap_number(self):
        self.result['stdout'] = self.result['stdout'].replace('ok 2 ', 'ok 3 ')
        self.reject([self.result])

    def test_duplicate_result(self):
        self.reject([self.result, copy.deepcopy(self.result)])

    def test_wrong_registered_command(self):
        self.result['command'] = ['/different/never-executed']
        self.reject([self.result])

    def test_timeout_result(self):
        self.result['result'] = 'TIMEOUT'
        self.reject([self.result])

    def test_crash_diagnostic(self):
        self.result['stderr'] = 'AddressSanitizer: synthetic rejection diagnostic'
        self.reject([self.result])

    def test_noninteger_exit(self):
        self.result['returncode'] = False
        self.reject([self.result])

    def test_resource_transition_rejects_unapproved_output(self):
        names = set(gate.MENU_OUTPUTS) | {'unapproved-output'}
        before = {'build_outputs': {name: {'missing': True} for name in names}, 'install_inputs': {}}
        after = copy.deepcopy(before)
        after['build_outputs']['unapproved-output'] = {'sha256': '0' * 64, 'bytes': 1, 'mode': '0o644'}
        with self.assertRaises(RuntimeError):
            RUNNER.require_resource_transition(before, after, Path('/synthetic-build'), sorted(gate.MENU_OUTPUTS))

    def test_missing_existing_resource_rejected(self):
        origin = {name: {label: {'after': {'synthetic': 'unchanged'}} for label in ('normal', 'sanitizer')}
                  for name in ('source-integrity.json', 'elf-integrity.json', 'freshness.json')}
        origin['freshness.json']['sanitizer']['after'] = {'build_outputs': {
            name: {'sha256': '0' * 64, 'bytes': 1, 'mode': '0o644'} for name in gate.MENU_OUTPUTS}}
        states = {key: {label: {'before_build': copy.deepcopy(origin[artifact][label]['after'])}
                       for label in ('normal', 'sanitizer')}
                  for key, artifact in (('source', 'source-integrity.json'), ('elf', 'elf-integrity.json'),
                                        ('freshness', 'freshness.json'))}
        states['freshness']['sanitizer']['before_build']['build_outputs'][sorted(gate.MENU_OUTPUTS)[0]] = {'missing': True}
        with self.assertRaisesRegex(RuntimeError, 'compiled/build inputs differ'):
            RUNNER.require_origin_identity(origin, **states)

    def test_missing_ubsan_flag(self):
        self.assertFalse(RUNNER.has_sanitizers(['cc', '-fsanitize=address']))

    def test_disabled_vptr(self):
        with self.assertRaises(RuntimeError):
            RUNNER.has_sanitizers(['c++', '-fsanitize=address,undefined', '-fno-sanitize=vptr'])

    def test_instrumentation_suppression(self):
        with self.assertRaises(RuntimeError):
            RUNNER.has_sanitizers(['cc', '-fsanitize=address,undefined', '-fsanitize-ignorelist=synthetic'])


def actual_controls(root, frozen, audit_closure=False):
    """Only caller-supplied, genuine terminal evidence can establish a positive."""
    errors = gate.validate(root=root, frozen_source=frozen, audit_closure=audit_closure)
    if errors:
        raise SystemExit('\n'.join(errors))
    print('Actual independent archive/current/frozen acceptance verified.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--audit-closure', action='store_true', help='Include exact one-time closure audit with --actual-root')
    parser.add_argument('--actual-root', type=Path, help='Optional real completed publication; never synthesized')
    parser.add_argument('--frozen-source', type=Path)
    parser.add_argument('--runner-source', type=Path, help='Optional exact reviewed runner for synthetic semantic controls')
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(SyntheticNegativeControls)
    suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(DocumentarySeparationControls))
    suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(SourcePhaseNegativeControls))
    if args.runner_source is not None:
        if args.frozen_source is None:
            parser.error('--runner-source requires --frozen-source')
        gate.require(gate.sha(args.runner_source) == gate.RUNNER_SHA256, 'Runner pin differs before control import')
        global RUNNER, CLONE, AGGREGATE, EXPECTED
        RUNNER = gate.load_verified(args.runner_source.resolve(), 'reviewed_runner_negative_controls')
        CLONE, AGGREGATE, _ = RUNNER.load_helpers(args.frozen_source.resolve(), gate.COMMIT)
        EXPECTED = RUNNER.expected_tests(args.frozen_source.resolve())
        suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(ReviewedRunnerNegativeControls))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.wasSuccessful():
        return 1
    print('Synthetic rejection controls only: no native result or positive terminal package established.')
    if args.actual_root is not None:
        if args.frozen_source is None:
            parser.error('--actual-root requires --frozen-source')
        actual_controls(args.actual_root.resolve(), args.frozen_source.resolve(), args.audit_closure)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
