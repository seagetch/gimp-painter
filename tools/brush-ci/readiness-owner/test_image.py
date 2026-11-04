"""Real-GObject lifetimes and synthetic GTK signatures; no GUI/ptrace/babl."""
import importlib.util
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('image_records', HERE / 'image_records.py')
image_records = importlib.util.module_from_spec(spec)
spec.loader.exec_module(image_records)


class ImageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        path = Path(cls.directory.name)
        source = path / 'image-proof.c'
        source.write_text('\n'.join((HERE / name).read_text() for name in (
            'synthetic-image-prefix.c', 'image.inc', 'synthetic-image-suffix.c')))
        cls.executable = path / 'image-proof'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        str(source), '-o', str(cls.executable),
                        '-l:libgobject-2.0.so.0', '-l:libglib-2.0.so.0'], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_case(self, mode, flags=0):
        result = subprocess.run([str(self.executable), mode], check=True,
                                text=True, capture_output=True, timeout=10)
        self.assertEqual(result.stderr, '')
        rows = result.stdout.splitlines()
        self.assertEqual(rows[-1], 'IMAGE_DIAG complete')
        records = image_records.ImageRecords()
        for row in rows:
            records.consume(row)
        result = records.result()
        self.assertEqual(result['status'], 'captured' if flags in (0, 2048) else 'inconclusive')
        if result['status'] == 'captured':
            self.assertEqual(result['identity_coverage_complete'], flags == 0 and mode not in ('reuse', 'unknown', 'unknown-nested'))
            self.assertFalse(result['acceptance_eligible'])
        parsed = []
        for row in rows[:-1]:
            self.assertRegex(row, r'^IMAGE_DIAG (summary|window|image|meta|metric)( -?[0-9]+)+$')
            words = row.split()
            parsed.append((words[1], list(map(int, words[2:]))))
        summary = parsed[0][1]
        self.assertEqual(len(summary), 9)
        self.assertEqual(summary[:3], [1, int(mode != 'missing-type'), flags])
        self.assertEqual(len(rows), 5 + summary[3] + 6 * summary[5])
        position = 1
        windows, identities, metadata, metrics = {}, {}, {}, {}
        for ordinal in range(1, summary[3] + 1):
            kind, values = parsed[position]
            self.assertEqual(kind, 'window')
            self.assertEqual(len(values), 2)
            self.assertEqual(values[0], ordinal)
            windows[ordinal] = values[1]
            position += 1
        for ordinal in range(1, summary[5] + 1):
            kind, values = parsed[position]
            self.assertEqual(kind, 'image')
            self.assertEqual(len(values), 5)
            self.assertEqual(values[0], ordinal)
            identities[ordinal] = values
            position += 1
            for phase in range(3):
                kind, values = parsed[position]
                self.assertEqual(kind, 'meta')
                self.assertEqual(len(values), 14)
                self.assertEqual(values[:2], [ordinal, phase])
                metadata[ordinal, phase] = values[2:]
                position += 1
            for metric_kind in (1, 2):
                kind, values = parsed[position]
                self.assertEqual(kind, 'metric')
                self.assertEqual(len(values), 8)
                self.assertEqual(values[:2], [ordinal, metric_kind])
                metrics[ordinal, metric_kind] = values[2:]
                position += 1
        for phase in (1, 2):
            kind, values = parsed[position]
            self.assertEqual(kind, 'meta')
            self.assertEqual(values[:2], [0, phase])
            metadata[0, phase] = values[2:]
            position += 1
        kind, values = parsed[position]
        self.assertEqual(kind, 'metric')
        self.assertEqual(values[:2], [0, 0])
        metrics[0, 0] = values[2:]
        for value in metadata.values():
            self.assertEqual(len(value), 12)
            if not value[0]:
                self.assertEqual(value, [0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0])
        if mode != 'counter':  # deliberate corruption exercises counter failure
            self.assertEqual(summary[6], sum(m[0] for m in metrics.values()))
            self.assertEqual(summary[7], sum(m[1] for m in metrics.values()))
            self.assertEqual(summary[8], sum(m[2] for m in metrics.values()))
        return summary, windows, identities, metadata, metrics

    def test_nested_first_repeat_internal_children_and_deduplication(self):
        summary, _, images, meta, metrics = self.run_case('normal')
        self.assertEqual(summary[3:], [1, 5, 3, 4, 4, 0])
        first = next(i for i in images if metrics[i, 2][0] == 2)
        nested = next(i for i in images if metrics[i, 1][3] == 1)
        uncalled = next(i for i in images if not metrics[i, 1][0])
        self.assertEqual(metrics[first, 1][:4], [1, 1, 0, 0])
        self.assertEqual(metrics[first, 2][:4], [2, 2, 0, 1])
        self.assertEqual(metrics[nested, 1][:4], [1, 1, 0, 1])
        self.assertEqual(meta[uncalled, 1][0], 0)
        self.assertGreater(metrics[first, 1][4], metrics[first, 2][5])
        self.assertEqual(meta[nested, 0][-2:], [1, 1])

    def test_original_destruction_keeps_token_safe_without_postcall_getters(self):
        for mode, alive in (('destroy', 0), ('destroy-retained', 1)):
            with self.subTest(mode=mode):
                _, _, images, _, metrics = self.run_case(mode)
                destroyed = next(i for i in images if images[i][3])
                self.assertEqual(images[destroyed][2:4], [alive, 1])
                self.assertEqual(metrics[destroyed, 1][:2], [1, 1])

    def test_real_weak_finalization_prevents_synthetic_address_reuse_matching(self):
        _, _, images, _, metrics = self.run_case('reuse')
        cleared = next(i for i in images if not images[i][2])
        self.assertEqual(images[cleared][3], 0)  # finalization != destroy signal
        self.assertEqual(metrics[cleared, 1][0], 0)
        self.assertEqual(metrics[0, 0][:2], [2, 2])

    def test_unknown_calls_are_not_unique_new_image_count(self):
        summary, _, images, meta, metrics = self.run_case('unknown')
        self.assertEqual(summary[5], 3)
        self.assertTrue(all(metrics[i, 1][0] == 0 for i in images))
        self.assertEqual(metrics[0, 0][0], 4)
        self.assertEqual(meta[0, 1][-1], 1)
        self.assertEqual(meta[0, 2][-1], 18)

    def test_unknown_nested_under_known_has_inclusive_overlap(self):
        _, _, images, meta, metrics = self.run_case('unknown-nested')
        known = next(i for i in images if metrics[i, 1][0])
        self.assertEqual(metrics[known, 1][:4], [1, 1, 0, 0])
        self.assertEqual(metrics[0, 0][:4], [1, 1, 0, 1])
        self.assertGreater(metrics[known, 1][4], metrics[0, 0][4])
        self.assertEqual(meta[0, 1][-2:], [1, 1])

    def test_metadata_snapshot_first_last_and_large_valid_values(self):
        _, _, images, meta, metrics = self.run_case('metadata')
        changed = next(i for i in images if metrics[i, 1][0])
        self.assertEqual(images[changed][4], 3)
        self.assertEqual(meta[changed, 0][2], 4)
        self.assertEqual(meta[changed, 1][2], 32768)
        self.assertEqual(meta[changed, 2], [1, 1, -1, 2147483647,
                                         0, 0, 0, 0, 4294967295, 2147483647, 2, 1])
        self.run_case('storages')

    def test_snapshot_window_finalization_is_explicit(self):
        _, windows, _, meta, metrics = self.run_case('window-finalized')
        self.assertEqual(windows, {1: 0})
        called = next(i for i, kind in metrics if i and kind == 1 and metrics[i, kind][0])
        self.assertEqual(meta[called, 0][-1], 1)
        self.assertEqual(meta[called, 1][-1], 18)

    def test_snapshot_traversal_bounds(self):
        for mode, flags in (('windows', 2), ('toplevels', 2), ('nodes', 4),
                            ('images', 8), ('depth', 16)):
            with self.subTest(mode=mode):
                self.run_case(mode, flags)

    def test_queued_finalization_or_reparent_is_safe_and_inconclusive(self):
        for mode in ('queue-finalized', 'queue-reparented'):
            with self.subTest(mode=mode):
                self.run_case(mode, 4096)

    def test_parent_counter_duration_clock_and_unmatched_flags(self):
        for mode, flags in (('parent-bound', 32), ('counter', 64 | 256),
                            ('duration', 64), ('clock', 128), ('unmatched', 256),
                            ('inflight', 256), ('invalid-metadata', 512)):
            with self.subTest(mode=mode):
                self.run_case(mode, flags)

    def test_missing_required_types_semantic_types_and_destroy_signal(self):
        for mode, flags in (('missing-type', 1), ('missing-semantic', 2048), ('missing-destroy', 1024)):
            with self.subTest(mode=mode):
                self.run_case(mode, flags)

    def test_hot_path_has_no_registration_allocation_names_or_size_queries(self):
        source = (HERE / 'image.inc').read_text()
        hot = source.split('static ImageMetadata\nimage_metadata_empty', 1)[1].split('/* Setup/finish only below', 1)[0]
        self.assertNotRegex(hot, r'\b(g_object_|g_signal_|g_type_from_name|g_type_class_|malloc|calloc|realloc|g_new|printf|g_print|gtk_icon_theme_|gtk_icon_size_lookup|gtk_widget_get_preferred|gtk_cell_renderer_get_preferred|g_object_get)')
        self.assertNotRegex(hot, r'\b(g_strdup|gtk_widget_get_name|gtk_widget_get_path)')
        exit = hot.split('image_observe_exit (ImageCall call)', 1)[1]
        self.assertNotRegex(exit, r'\b(gtk_|g_object_|g_signal_|G_OBJECT)')
        token = re.search(r'typedef struct \{\n  guint ordinal;.*?\} ImageCall;', source, re.S).group()
        self.assertNotIn('*', token)
        self.assertIn('gtk_container_forall (', source)
        self.assertNotIn('gtk_container_foreach (', source)
        self.assertNotIn('g_object_ref (', source)
        self.assertIn('IMAGE_SNAPSHOT_MUTATED', source)


if __name__ == '__main__':
    unittest.main()
