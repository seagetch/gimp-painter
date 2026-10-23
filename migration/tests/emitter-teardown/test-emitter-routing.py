#!/usr/bin/env python3
"""Positive and adversarial controls for the bounded original 07.008 proof."""
import copy
import importlib.util
from pathlib import Path
import sys
import tempfile
import types
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('emitter_routing', HERE / 'reproduce-emitter-routing.py')
proof = importlib.util.module_from_spec(spec)
spec.loader.exec_module(proof)
from legacy_assignment_rules import PROFILES, route


class RoutingProof(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.baseline = proof.load_baseline()
        cls.cases = proof.load_cases()
        cls.sources = proof.load_sources(cls.cases)
        cls.sections, cls.trees = proof.verified_sections(cls.cases, cls.baseline, cls.sources)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'tasks.md'; path.write_bytes(proof.prior('tasks.md'))
            cls.catalog = proof.assignment.task_catalog(path)
        cls.outputs, cls.report = proof.regenerate(cls.baseline, cls.cases, cls.sections, cls.catalog)

    def test_exact_delta_preserves_all_surviving_rows(self):
        self.assertEqual(self.report['removed_todo_count'], 90)
        self.assertEqual(self.report['added_work_count'], 0)
        self.assertEqual(self.report['retained_work_items'], 22778)
        self.assertEqual(self.report['preserved_done_work_items'], 569)
        self.assertEqual(self.report['done_added_by_routing'], 0)
        self.assertEqual(self.report['remaining_original_07008_assignments'], 4)
        before = {r['work_id']: r for r in proof.rows(self.baseline[proof.WORK])}
        after = proof.rows(self.outputs[proof.WORK])
        self.assertTrue(all(r == before[r['work_id']] for r in after))
        self.assertEqual({r['source_id'] for r in after if r['wbs_task'] == proof.TASK}, proof.KEEP)
        self.assertEqual(len(self.sources), 49)
        self.assertEqual(len(self.sections), 94)
        self.assertEqual(len({c['path'] for c in self.cases}), 27)

    def test_exact_three_widget_triggers_and_common_provider(self):
        for path, index in [('app/widgets/gimpeditor-cxx.cpp', '1'),
                            ('app/widgets/gimpcontainertreeview.c', '8'),
                            ('app/widgets/gimplayertreeview.c', '7'),
                            ('app/base/delegators.hpp', '1')]:
            with self.subTest(path=path):
                self.assertIn(proof.TASK, route({'path': path, 'index': index}, [], [])['tests'])
        for path, index in [('app/widgets/gimpeditor-cxx.cpp', '2'),
                            ('app/widgets/gimpcontainertreeview.c', '7'),
                            ('app/widgets/gimpcontainertreeview.c', '9'),
                            ('app/widgets/gimplayertreeview.c', '6'),
                            ('app/widgets/gimplayertreeview.c', '8'),
                            ('app/widgets/gimpeditor-cxx.h', '1'),
                            ('app/base/glib-cxx-def-utils.hpp', '1')]:
            with self.subTest(path=path, index=index):
                self.assertNotIn(proof.TASK, route({'path': path, 'index': index}, [], [])['tests'])
        provider = route({'path': 'app/base/delegators.hpp', 'index': '1'}, [], [])
        self.assertEqual(provider['tests'], ['07.007', '07.008', '07.009', '07.013'])
        self.assertIn('07.008/untracked-signals', provider['tasks'])
        self.assertIn('07.008/connection-name', provider['tasks'])

    def test_prior_policy_comparison_changes_only_original_gate(self):
        old_policy = types.ModuleType('prior_assignment_rules')
        exec(compile(proof.prior('tools/legacy_assignment_rules.py'), 'prior_assignment_rules.py', 'exec'),
             old_policy.__dict__)
        snapshot = copy.deepcopy(PROFILES)
        hunks = proof.rows(self.baseline['changed-hunks.tsv'])
        differences = set()
        # Only byte-verified source payloads can prove a historical hunk route.
        # Empty synthetic input would miss the declaration-only header override.
        for hunk in hunks:
            section = self.sections.get((hunk['path'], hunk['index']))
            if section is None:
                continue
            before = old_policy.route(hunk, section['added'], section['removed'])
            after = route(hunk, section['added'], section['removed'])
            if before != after:
                differences.add(hunk['child_id'])
                self.assertIn(before['profile'], ('widget-helpers', 'cpp-gtk'))
                self.assertEqual(after, dict(before, tests=[t for t in before['tests'] if t != proof.TASK]))
        self.assertEqual(differences, {c['source_id'] for c in self.cases} - proof.KEEP)
        self.assertEqual(PROFILES, snapshot, 'route() must not mutate shared profile lists')
        self.assertEqual(PROFILES, old_policy.PROFILES, 'No existing profile may change')
        # Synthetic controls cover other indexed paths and the header override;
        # they do not claim a full historical-source regeneration.
        for hunk in hunks:
            for added in ([], ['__DECLARE_GTK_WIDGET(GimpEditor);']):
                before = old_policy.route(hunk, added, [])
                after = route(hunk, added, [])
                if before['profile'] not in ('widget-helpers', 'cpp-gtk'):
                    self.assertEqual(after, before)

    def test_changed_source_or_base_bytes_are_rejected(self):
        for name in ('source/app/base/delegators.hpp', 'base/app/widgets/gimplayertreeview.c'):
            with self.subTest(name=name):
                sources = dict(self.sources); sources[name] += b'\nchanged\n'
                with self.assertRaisesRegex(ValueError, 'Changed source/base blob'):
                    proof.verified_sections(self.cases, self.baseline, sources)

    def test_changed_case_hash_or_scope_is_rejected(self):
        for field, value in [('payload_sha256', '0' * 64), ('source_scope', 'wrong scope')]:
            with self.subTest(field=field):
                cases = copy.deepcopy(self.cases); cases[0][field] = value
                with self.assertRaisesRegex(ValueError, 'changed reviewed ' + field):
                    proof.verified_sections(cases, self.baseline, self.sources)

    def test_source_reconstruction_rejects_correlated_manifest_hash_tampering(self):
        cases = copy.deepcopy(self.cases); cases[0]['payload_sha256'] = '0' * 64
        baseline = dict(self.baseline); assignments = proof.rows(baseline[proof.ASSIGN])
        next(r for r in assignments if r['hunk_id'] == cases[0]['source_id'])['payload_sha256'] = '0' * 64
        baseline[proof.ASSIGN] = proof.encode(assignments)
        with self.assertRaisesRegex(ValueError, 'changed zero-context payload'):
            proof.verified_sections(cases, baseline, self.sources)

    def test_missing_source_or_reviewed_hunk_is_rejected(self):
        sources = dict(self.sources); sources.pop(next(iter(sources)))
        with self.assertRaisesRegex(ValueError, 'Evidence source coverage'):
            proof.verified_sections(self.cases, self.baseline, sources)
        with self.assertRaisesRegex(ValueError, 'review coverage differs'):
            proof.verified_sections(self.cases[1:], self.baseline, self.sources)

    def test_unknown_duplicate_and_wrong_work_assignments_are_rejected(self):
        for mode, expected in [('unknown', 'unassigned work item'), ('duplicate', 'duplicate work item'),
                               ('wrong_task', 'changed source/action contract wbs_task')]:
            with self.subTest(mode=mode):
                baseline = dict(self.baseline); work = proof.rows(baseline[proof.WORK])
                if mode == 'unknown':
                    work[0]['work_id'] = 'legacy-unknown'
                elif mode == 'duplicate':
                    work.append(dict(work[0]))
                else:
                    work[0]['wbs_task'] = proof.TASK
                baseline[proof.WORK] = proof.encode(work)
                with self.assertRaisesRegex(ValueError, expected):
                    proof.regenerate(baseline, self.cases, self.sections, self.catalog)

    def test_removal_cannot_discard_done_evidence(self):
        baseline = dict(self.baseline); work = proof.rows(baseline[proof.WORK])
        row = next(r for r in work if r['work_id'] in self.report['removed_todo_work_ids'])
        row.update(status='DONE', owner='Codex', artifact='prior.json', test='prior test', result='PASS')
        baseline[proof.WORK] = proof.encode(work)
        with self.assertRaisesRegex(ValueError, 'Public parent work/status counts differ'):
            proof.regenerate(baseline, self.cases, self.sections, self.catalog)

    def test_wrong_route_is_rejected(self):
        cases = copy.deepcopy(self.cases); cases[0]['verification_tasks_proposed'] = '29.020'
        with self.assertRaisesRegex(ValueError, 'wrong routed verification'):
            proof.regenerate(self.baseline, cases, self.sections, self.catalog)

    def test_acceptance_changes_only_four_exact_rows(self):
        current = dict(self.baseline, **self.outputs); work = proof.rows(current[proof.WORK])
        accepted = [r for r in work if r['work_id'] in self.report['retained_gate_work_ids']]
        self.assertEqual(len(accepted), 4)
        for row in accepted:
            row.update(status='DONE', owner='Codex', artifact='acceptance.json', test='native emitter teardown', result='PASS')
        current[proof.WORK] = proof.encode(work)
        proof.check_completed_output(self.baseline, self.outputs, current)
        with self.assertRaisesRegex(ValueError, 'Refusing to overwrite later inventory work'):
            proof.guard_write(self.baseline, self.outputs, current)
        accepted[0]['source_blob'] = '0' * 40
        current[proof.WORK] = proof.encode(work)
        with self.assertRaisesRegex(ValueError, 'changed source/action contract'):
            proof.check_completed_output(self.baseline, self.outputs, current)

    def test_acceptance_cannot_change_other_duty_on_retained_source(self):
        current = dict(self.baseline, **self.outputs); work = proof.rows(current[proof.WORK])
        row = next(r for r in work if r['source_id'] in proof.KEEP and r['wbs_task'] != proof.TASK)
        row['limitations'] += '; changed unrelated duty'
        current[proof.WORK] = proof.encode(work)
        with self.assertRaisesRegex(ValueError, 'changed unrelated work'):
            proof.check_completed_output(self.baseline, self.outputs, current)

    def test_acceptance_cannot_reorder_or_add_work(self):
        for mode in ('reorder', 'add'):
            with self.subTest(mode=mode):
                current = dict(self.baseline, **self.outputs); work = proof.rows(current[proof.WORK])
                if mode == 'reorder':
                    work[0], work[1] = work[1], work[0]
                else:
                    work.append(dict(work[0]))
                current[proof.WORK] = proof.encode(work)
                with self.assertRaises(ValueError):
                    proof.check_completed_output(self.baseline, self.outputs, current)

    def test_done_without_evidence_is_rejected(self):
        current = dict(self.baseline, **self.outputs); work = proof.rows(current[proof.WORK])
        row = next(r for r in work if r['work_id'] in self.report['retained_gate_work_ids'])
        row.update(status='DONE', artifact='', test='', result='')
        current[proof.WORK] = proof.encode(work)
        with self.assertRaisesRegex(ValueError, 'DONE requires'):
            proof.check_completed_output(self.baseline, self.outputs, current)

    def test_idempotent_write_guard_and_later_edit_rejection(self):
        proof.guard_write(self.baseline, self.outputs, self.baseline)
        proof.guard_write(self.baseline, self.outputs, dict(self.baseline, **self.outputs))
        proof.check_completed_output(self.baseline, self.outputs, dict(self.baseline, **self.outputs))
        for name in (proof.WORK, proof.ASSIGN, 'changed-files.tsv'):
            with self.subTest(name=name):
                current = dict(self.baseline, **self.outputs); current[name] += b'later edit\n'
                with self.assertRaisesRegex(ValueError, 'Refusing to overwrite later inventory work'):
                    proof.guard_write(self.baseline, self.outputs, current)
                with self.assertRaises(ValueError):
                    proof.check_completed_output(self.baseline, self.outputs, current)
        current = dict(self.baseline); current['later.tsv'] = b'new work\n'
        with self.assertRaisesRegex(ValueError, 'added/removed inventory'):
            proof.guard_write(self.baseline, self.outputs, current)
        current = dict(self.baseline); current.pop('changed-files.tsv')
        with self.assertRaisesRegex(ValueError, 'added/removed inventory'):
            proof.guard_write(self.baseline, self.outputs, current)


if __name__ == '__main__':
    unittest.main(verbosity=2)
