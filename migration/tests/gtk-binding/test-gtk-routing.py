#!/usr/bin/env python3
"""Negative controls for the bounded 06.023 source and work-identity proof."""
import copy
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('gtk_routing', HERE / 'reproduce-gtk-routing.py')
proof = importlib.util.module_from_spec(spec)
spec.loader.exec_module(proof)
from legacy_assignment_rules import uses_gtk_dsl, route


class RoutingProof(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.baseline = proof.load_baseline()
        cls.cases = proof.load_cases()
        cls.sources = proof.load_sources()
        cls.sections, cls.trees = proof.verified_sections(cls.cases, cls.baseline, cls.sources)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'tasks.md'
            path.write_bytes(proof.prior('tasks.md'))
            cls.catalog = proof.assignment.task_catalog(path)
        cls.outputs, cls.report = proof.regenerate(cls.baseline, cls.cases, cls.sections, cls.catalog)

    def test_exact_bounded_delta_and_idempotent_guard(self):
        self.assertEqual(self.report['removed_todo_count'], 78)
        self.assertEqual(self.report['added_todo_count'], 3)
        self.assertEqual(self.report['preserved_other_reviewed_work_items'], 372)
        self.assertEqual(self.report['preserved_done_work_items'], 535)
        self.assertEqual(self.report['remaining_06023_assignments'], 18)
        proof.guard_write(self.baseline, self.outputs, self.baseline)
        proof.guard_write(self.baseline, self.outputs, dict(self.baseline, **self.outputs))

    def test_native_constructor_and_include_only_are_not_dsl(self):
        prefix = ['#include "base/glib-cxx-def-utils.hpp"', 'using namespace GLib;']
        self.assertFalse(uses_gtk_dsl('app/widgets/example.cpp', prefix))
        self.assertFalse(uses_gtk_dsl('app/widgets/example.cpp', prefix + ['gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);']))
        self.assertFalse(uses_gtk_dsl('app/widgets/example.cpp', ['with(widget);']))
        self.assertFalse(uses_gtk_dsl('app/widgets/example.cpp', ['// GLib::with(widget);', 'const char *s = "GLib::Definer<T>";']))
        self.assertTrue(uses_gtk_dsl('app/widgets/example.cpp', prefix + ['with(widget, callback);']))
        self.assertTrue(uses_gtk_dsl('app/widgets/example.cpp', ['GLib::with(widget, callback);']))

    def test_support_is_pinned_to_reviewed_hunks(self):
        self.assertIn('06.023', route({'path': 'app/widgets/gimpitemtreeview.c', 'index': '4'}, ['#if 1'], [])['tasks'])
        self.assertNotIn('06.023', route({'path': 'app/widgets/gimpitemtreeview.c', 'index': '3'}, ['gtk_box_new(0, 0);'], [])['tasks'])

    def test_changed_source_payload_is_rejected(self):
        sources = dict(self.sources)
        sources['source/app/widgets/gimplayerpopup.cpp'] += b'\n// changed payload\n'
        with self.assertRaisesRegex(ValueError, 'Changed source/base blob'):
            proof.verified_sections(self.cases, self.baseline, sources)

    def test_changed_hunk_hash_is_rejected(self):
        cases = copy.deepcopy(self.cases)
        cases[0]['payload_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'changed reviewed payload_sha256'):
            proof.verified_sections(cases, self.baseline, self.sources)

    def test_unknown_work_id_is_rejected(self):
        baseline = dict(self.baseline)
        work = proof.rows(baseline[proof.WORK]); work[0]['work_id'] = 'legacy-unknown'
        baseline[proof.WORK] = proof.encode(work)
        with self.assertRaisesRegex(ValueError, 'unassigned work item'):
            proof.regenerate(baseline, self.cases, self.sections, self.catalog)

    def test_duplicate_work_id_is_rejected(self):
        baseline = dict(self.baseline)
        work = proof.rows(baseline[proof.WORK]); work.append(dict(work[0]))
        baseline[proof.WORK] = proof.encode(work)
        with self.assertRaisesRegex(ValueError, 'duplicate work item'):
            proof.regenerate(baseline, self.cases, self.sections, self.catalog)

    def test_wrong_work_assignment_is_rejected(self):
        baseline = dict(self.baseline)
        work = proof.rows(baseline[proof.WORK]); work[0]['wbs_task'] = '06.023'
        baseline[proof.WORK] = proof.encode(work)
        with self.assertRaisesRegex(ValueError, 'changed source/action contract wbs_task'):
            proof.regenerate(baseline, self.cases, self.sections, self.catalog)

    def test_wrong_route_is_rejected(self):
        cases = copy.deepcopy(self.cases)
        cases[0]['proposed_implementation_tasks'] = '06.023'
        with self.assertRaisesRegex(ValueError, 'wrong routed assignment'):
            proof.regenerate(self.baseline, cases, self.sections, self.catalog)

    def test_later_emitter_verification_route_preserves_historical_output(self):
        # Current original 07.008 routing drops the unrelated GTK DSL provider,
        # while this proof must still reproduce exactly the historical outputs.
        current = route({'path': 'app/base/glib-cxx-def-utils.hpp', 'index': '1'}, [], [])
        self.assertNotIn('07.008', current['tests'])
        before = next(r for r in proof.rows(self.baseline[proof.ASSIGN])
                      if r['hunk_id'] == '01.002/000044')
        after = next(r for r in proof.rows(self.outputs[proof.ASSIGN])
                     if r['hunk_id'] == '01.002/000044')
        self.assertIn('07.008', before['verification_tasks'].split(','))
        self.assertEqual(after['verification_tasks'], before['verification_tasks'])
        import json
        recorded = json.loads((HERE / 'routing-proof.json').read_text())
        self.assertEqual(self.report['output_sha256'], recorded['output_sha256'])

    def test_acceptance_check_limits_mutable_fields_to_exact_duties(self):
        current = dict(self.outputs)
        work = proof.rows(current[proof.WORK])
        row = next(r for r in work if r['wbs_task'] == '06.023')
        row.update(status='DONE', owner='Codex', artifact='report.json', test='native ownership', result='PASS')
        current[proof.WORK] = proof.encode(work)
        proof.check_completed_output(self.outputs, current)
        row['source_blob'] = '0' * 40
        current[proof.WORK] = proof.encode(work)
        with self.assertRaisesRegex(ValueError, 'changed source/action contract'):
            proof.check_completed_output(self.outputs, current)
        work = proof.rows(self.outputs[proof.WORK])
        row = next(r for r in work if r['wbs_task'] != '06.023')
        row['limitations'] += 'changed'
        current[proof.WORK] = proof.encode(work)
        with self.assertRaisesRegex(ValueError, 'changed unrelated'):
            proof.check_completed_output(self.outputs, current)

    def test_done_without_evidence_is_rejected(self):
        current = dict(self.outputs)
        work = proof.rows(current[proof.WORK])
        row = next(r for r in work if r['wbs_task'] == '06.023')
        row.update(status='DONE', artifact='', test='', result='')
        current[proof.WORK] = proof.encode(work)
        with self.assertRaises(ValueError):
            proof.check_completed_output(self.outputs, current)

    def test_write_guard_refuses_later_work_or_source_edits(self):
        for name in [proof.WORK, proof.ASSIGN, 'changed-files.tsv']:
            with self.subTest(name=name):
                current = dict(self.baseline, **self.outputs)
                current[name] += b'changed after the routing correction\n'
                with self.assertRaisesRegex(ValueError, 'Refusing to overwrite later inventory work'):
                    proof.guard_write(self.baseline, self.outputs, current)
        current = dict(self.baseline); current['later.tsv'] = b'new work\n'
        with self.assertRaisesRegex(ValueError, 'added/removed inventory'):
            proof.guard_write(self.baseline, self.outputs, current)


if __name__ == '__main__':
    unittest.main(verbosity=2)
