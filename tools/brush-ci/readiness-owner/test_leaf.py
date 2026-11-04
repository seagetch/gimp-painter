"""Real-GObject / synthetic method ABI proofs; no GTK, GUI, ptrace or babl."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('leaf_records', HERE / 'leaf_records.py')
leaf_records = importlib.util.module_from_spec(spec)
spec.loader.exec_module(leaf_records)


class LeafTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        path = Path(cls.directory.name)
        source = path / 'leaf-proof.c'
        source.write_text('\n'.join((HERE / name).read_text() for name in (
            'synthetic-leaf-prefix.c', 'leaf.inc', 'synthetic-leaf-suffix.c')))
        cls.executable = path / 'leaf-proof'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        str(source), '-o', str(cls.executable),
                        '-l:libgobject-2.0.so.0', '-l:libglib-2.0.so.0'], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_case(self, mode):
        result = subprocess.run([str(self.executable), mode], check=True,
                                text=True, capture_output=True, timeout=10)
        self.assertEqual(result.stderr, '')
        lines = result.stdout.splitlines()
        self.assertEqual(len(lines), 21)
        self.assertEqual(lines[-1], 'LEAF_DIAG complete')
        for row in lines[:-1]:
            self.assertRegex(row, r'^LEAF_DIAG (summary|family|metric|union)( [0-9]+)+$')
        self.assertEqual(len(lines[0].split()[2:]), 5)
        for row in lines[1:4]:
            self.assertEqual(len(row.split()[2:]), 11)
        for row in lines[4:19]:
            self.assertEqual(len(row.split()[2:]), 9)
        self.assertEqual(len(lines[19].split()[2:]), 7)
        parsed = leaf_records.LeafRecords()
        for line in lines:
            parsed.consume(line)
        expected = 'captured' if mode in ('normal', 'override', 'late-override') else 'inconclusive'
        self.assertEqual(parsed.result()['status'], expected)
        if expected == 'captured':
            self.assertEqual(parsed.result()['coverage_complete'], mode == 'normal')
        return [int(value) for value in lines[0].split()[2:]], lines

    def test_inheritance_late_copy_exact_forwarding_and_restore(self):
        summary, rows = self.run_case('normal')
        self.assertEqual(summary, [1, 1, 1, 1, 0])
        metrics = [[int(v) for v in row.split()[4:]] for row in rows[4:19]]
        union = [int(v) for v in rows[19].split()[2:]]
        self.assertEqual(union[0], sum(m[0] for m in metrics))
        self.assertEqual(union[1], sum(m[1] for m in metrics))
        self.assertLessEqual(union[5], sum(m[5] for m in metrics))
        self.assertGreater(sum(m[5] for m in metrics), union[5])

    def test_unknown_overrides_and_parent_calls_remain_partial(self):
        for mode in ('override', 'late-override'):
            with self.subTest(mode=mode):
                summary, _ = self.run_case(mode)
                self.assertEqual(summary, [1, 1, 0, 1, 4])

    def test_pre_audit_bound_installs_nothing(self):
        summary, _ = self.run_case('pre-cap')
        self.assertEqual(summary, [1, 0, 0, 1, 2])

    def test_late_bound_rejects_restore_and_restores_direct_slots(self):
        summary, _ = self.run_case('late-cap')
        self.assertEqual(summary, [1, 1, 0, 0, 2])

    def test_unknown_mutation_is_not_clobbered(self):
        summary, _ = self.run_case('conflict')
        self.assertEqual(summary, [1, 1, 0, 0, 16])

    def test_null_base_disables_without_initializing_dormant_classes(self):
        summary, _ = self.run_case('null-base')
        self.assertEqual(summary, [1, 0, 0, 1, 1])

    def test_owner_and_counter_bounds_fail_closed(self):
        for mode, flag in (('owner-bound', 128), ('saturation', 32)):
            with self.subTest(mode=mode):
                summary, _ = self.run_case(mode)
                self.assertEqual(summary[:4], [1, 1, 0, 1])
                self.assertTrue(summary[4] & flag)

    def test_real_gobject_dynamic_class_gate(self):
        summary, _ = self.run_case('pre-dynamic')
        self.assertEqual(summary, [1, 0, 0, 1, 8])
        summary, _ = self.run_case('late-dynamic')
        self.assertEqual(summary, [1, 1, 0, 0, 8])

    def test_clock_unmatched_and_missing_owner_fail_closed(self):
        for mode, flag in (('clock', 64), ('unmatched', 256), ('missing-owner', 512)):
            with self.subTest(mode=mode):
                summary, _ = self.run_case(mode)
                self.assertEqual(summary[:4], [1, 1, 0, 1])
                self.assertEqual(summary[4], flag)

    def test_hot_path_has_no_type_lookup_log_allocation_or_sizing_api(self):
        source = (HERE / 'leaf.inc').read_text()
        hot_path = source.split('static void\nleaf_increment', 1)[1].split('/* Slots:', 1)[0]
        self.assertNotRegex(hot_path.replace('g_type_is_a', ''), r'\b(g_type_|g_object_|g_print|printf|malloc|g_new|g_strdup|g_str_|gtk_widget_get_preferred|gtk_cell_renderer_get_preferred)')
        self.assertNotIn('get_preferred_height =', source)
        self.assertNotIn('g_type_class_ref (', source)


if __name__ == '__main__':
    unittest.main()
