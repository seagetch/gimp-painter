#!/usr/bin/env python3
# SPDX-License-Identifier: ISC
"""Self-contained pinned-source, generation and C/C++14 metadata comparisons."""
import argparse
import ast
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tests/legacy'
MANIFEST = ROOT / 'brush-settings.json'
spec = importlib.util.spec_from_file_location('brush_generator', ROOT / 'generate.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
DATA = json.loads(MANIFEST.read_text())
LEGACY = (FIXTURES / 'mypaintbrush-brushsettings.c').read_text()
TABLES = [('inputs', 'inputs_list', 'MyPaintBrushInputSettings'),
          ('settings', 'settings_list', 'MyPaintBrushSettings'),
          ('switches', 'switches_list', 'MyPaintBrushSwitchSettings'),
          ('texts', 'text_list', 'MyPaintBrushTextSettings')]
COMMANDS = []


def run(command, **kwargs):
    COMMANDS.append([str(arg) for arg in command])
    return subprocess.run(command, check=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, **kwargs).stdout


def without_comments(text):
    return re.sub(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*',
                  lambda m: m[0] if m[0].startswith('"') else '', text, flags=re.S)


def table_source(name, text=LEGACY):
    return re.search(r'(?:static )?\w+\s*\*?\s+' + name + r'\[\]\s*=\s*\{.*?\n\};', text, re.S)[0]


def fields(text):
    # Independent C-initializer tokenization, not the generator's serialization.
    result, current, quoted, escaped = [], '', False, False
    for ch in text:
        if ch == ',' and not quoted:
            result.append(current.strip())
            current = ''
            continue
        current += ch
        if escaped:
            escaped = False
        elif quoted and ch == '\\':
            escaped = True
        elif ch == '"':
            quoted = not quoted
    return result + [current.strip()]


def c_text(text):
    return ast.literal_eval(text[2:-1] if text.startswith('_(') else text)


def source_rows(name):
    # These four source tables have one flat initializer per row.
    source = without_comments(table_source(name))
    return [fields(row) for row in re.findall(r'\{([^{}]+)\}', source) if row.strip() != 'NULL']


DUMP = r'''
#include <stdio.h>
#include <string.h>
static void text_value(const char *s) {
  int length = s ? (int) strlen(s) : -1;
  fwrite(&length, sizeof(length), 1, stdout);
  if (s) fwrite(s, 1, (size_t) length, stdout);
}
static void int_value(int value) { fwrite(&value, sizeof(value), 1, stdout); }
static void float_value(float value) { fwrite(&value, sizeof(value), 1, stdout); }
int main(void) {
  int i;
  for (i=0; i<INPUT_COUNT; ++i) {
    text_value(inputs_list[i].name); int_value(inputs_list[i].index);
    float_value(inputs_list[i].hard_minimum); float_value(inputs_list[i].soft_minimum);
    float_value(inputs_list[i].normal); float_value(inputs_list[i].soft_maximum);
    float_value(inputs_list[i].hard_maximum);
    text_value(inputs_list[i].displayed_name); text_value(inputs_list[i].tooltip);
  }
  for (i=0; i<BRUSH_MAPPING_COUNT; ++i) {
    text_value(settings_list[i].internal_name); int_value(settings_list[i].index);
    text_value(settings_list[i].displayed_name); int_value(settings_list[i].constant);
    float_value(settings_list[i].minimum); float_value(settings_list[i].default_value);
    float_value(settings_list[i].maximum); text_value(settings_list[i].tooltip);
  }
  for (i=0; i<BRUSH_BOOL_COUNT; ++i) {
    text_value(switches_list[i].internal_name); int_value(switches_list[i].index);
    text_value(switches_list[i].displayed_name); int_value(switches_list[i].default_value);
  }
  for (i=0; i<BRUSH_TEXT_COUNT; ++i) {
    text_value(text_list[i].internal_name); int_value(text_list[i].index);
    text_value(text_list[i].displayed_name); text_value(text_list[i].default_value);
  }
  return 0;
}
'''


class BrushSettingsTests(unittest.TestCase):
    def test_fixture_provenance(self):
        self.assertEqual(DATA['provenance']['revision'], 'afa43fae3e920210146abed514f136fd49f671b5')
        for path, digest in DATA['provenance']['sources'].items():
            self.assertEqual(hashlib.sha256((FIXTURES / Path(path).name).read_bytes()).hexdigest(), digest, path)

    def test_enum_exact_legacy_bytes(self):
        generated = generator.generate(MANIFEST)
        self.assertEqual(generated[generator.ENUM_HEADER],
                         (FIXTURES / generator.ENUM_HEADER).read_bytes())
        self.assertEqual(len(DATA['constants']), 103)

    def test_numeric_constants_and_index_spaces(self):
        generator.validate(DATA)
        values = {entry['name']: entry['value'] for entry in DATA['constants']}
        self.assertEqual([values[n] for n in ['INPUT_COUNT', 'BRUSH_MAPPING_COUNT',
                         'BRUSH_BOOL_BASE', 'BRUSH_BOOL_COUNT', 'BRUSH_TEXT_BASE',
                         'BRUSH_TEXT_COUNT', 'BRUSH_SETTINGS_COUNT', 'STATE_COUNT']],
                         [9, 45, 45, 5, 50, 2, 52, 30])
        self.assertEqual([values[n] for n in ['BRUSH_STROKE_OPACITY', 'BRUSH_TEXTURE_GRAIN',
                                            'BRUSH_TEXTURE_CONTRAST']], [42, 43, 44])

    def test_all_legacy_tables_semantic_fields(self):
        schemas = [
            ['name', 'symbol', 'hard_minimum', 'soft_minimum', 'normal', 'soft_maximum', 'hard_maximum', 'displayed_name', 'tooltip'],
            ['name', 'symbol', 'displayed_name', 'constant', 'minimum', 'default_value', 'maximum', 'tooltip'],
            ['name', 'symbol', 'displayed_name', 'default_value'],
            ['name', 'symbol', 'displayed_name', 'default_value'],
        ]
        for (category, array, _), schema in zip(TABLES, schemas):
            rows = source_rows(array)
            self.assertEqual(len(rows), len(DATA[category]))
            for row, entry in zip(rows, DATA[category]):
                self.assertEqual(len(row), len(schema))
                for key, raw in zip(schema, row):
                    if key in ('name', 'displayed_name', 'tooltip'):
                        value = c_text(raw)
                    elif key == 'constant' or category == 'switches' and key == 'default_value':
                        value = raw.lower() == 'true'
                    elif category == 'texts' and key == 'default_value':
                        value = None if raw == 'NULL' else c_text(raw)
                    else:
                        value = raw
                    self.assertEqual(entry[key], value, category + ':' + entry['name'] + ':' + key)

    def test_state_order_constructor_and_no_invented_range(self):
        source = without_comments(table_source('states_list'))
        names = [c_text(s) for s in re.findall(r'"[a-z0-9_]+"', source)]
        self.assertEqual(names, [entry['name'] for entry in DATA['states']])
        engine = (FIXTURES / 'mypaintbrush-brush.hpp').read_text()
        self.assertRegex(engine, r'for \(int i=0; i<STATE_COUNT; i\+\+\)\s*\{\s*states\[i\] = 0;')
        self.assertRegex(engine, r'void reset\(\)\s*\{\s*reset_requested = true;')
        for entry in DATA['states']:
            self.assertEqual(entry['constructor_default'], '0')
            self.assertIsNone(entry['minimum'])
            self.assertIsNone(entry['maximum'])

    def test_legacy_groups_and_migrations_preserved(self):
        for row, entry in zip(source_rows('group_list'), DATA['legacy_groups']):
            name, symbol, label, array = row
            self.assertEqual([entry['name'], entry['symbol'], entry['displayed_name']],
                             [c_text(name), symbol, c_text(label)])
            members = re.search(r'\b' + array + r'\[\]\s*=\s*\{(.*?)\};', LEGACY, re.S)[1]
            self.assertEqual(entry['members'], [c_text(s) for s in re.findall(r'"[a-z0-9_]+"', members)])
            self.assertEqual(entry['null_terminated'], 'NULL' in members)
        self.assertEqual(len(DATA['legacy_groups']), 10)
        self.assertEqual(len(DATA['legacy_migrations']), 6)
        for row, entry in zip(source_rows('migration'), DATA['legacy_migrations']):
            self.assertEqual(entry, dict(old_name=c_text(row[0]), new_name=c_text(row[1]), transform=row[2]))
        bodies = dict(re.findall(r'static gfloat (transform_\w+) \(gfloat y\) \{\s*(.*?)\s*\}', LEGACY))
        self.assertEqual(DATA['legacy_transform_bodies'], bodies)
        groups = {e['name']: e for e in DATA['legacy_groups']}
        self.assertFalse(groups['tracking']['null_terminated'])
        self.assertEqual(groups['stroke']['symbol'], 'BRUSH_SETTING_GROUP_TRACKING')
        self.assertIn('colorize', groups['color']['members'])
        self.assertNotIn('colorize', [e['name'] for e in DATA['settings']])
        disabled = DATA['compatibility']['disabled_inputs'][0]
        self.assertIn('//' + disabled['legacy_initializer'], LEGACY)
        self.assertEqual(disabled['source'], 'app/core/mypaintbrush-brushsettings.c:39')
        self.assertEqual(DATA['compatibility']['hidden_settings'], ['color_h', 'color_s', 'color_v'])

    def test_deterministic_cli_and_read_only_check(self):
        before = MANIFEST.read_bytes()
        with tempfile.TemporaryDirectory() as directory:
            first, second = Path(directory) / 'a', Path(directory) / 'b'
            command = [sys.executable, '-B', str(ROOT / 'generate.py'), '--manifest', str(MANIFEST)]
            for path in [first, second]:
                run(command + ['--output-dir', str(path)], cwd=directory,
                    env=dict(os.environ, LC_ALL='C', PYTHONHASHSEED=str(len(path.name))))
            for name in [generator.ENUM_HEADER, generator.DATA_HEADER]:
                self.assertEqual((first / name).read_bytes(), (second / name).read_bytes())
            times = [(first / name).stat().st_mtime_ns for name in generator.generate(MANIFEST)]
            run(command + ['--output-dir', str(first)])
            self.assertEqual(times, [(first / name).stat().st_mtime_ns for name in generator.generate(MANIFEST)])
            run(command + ['--output-dir', str(first), '--check'])
            (first / generator.DATA_HEADER).write_text('stale')
            with self.assertRaises(subprocess.CalledProcessError):
                run(command + ['--output-dir', str(first), '--check'])
            self.assertEqual((first / generator.DATA_HEADER).read_text(), 'stale')
        self.assertEqual(MANIFEST.read_bytes(), before)

    def test_manifest_validation_rejects_corruption(self):
        mutations = [
            lambda d: d['settings'].pop(),
            lambda d: d['settings'][0].update(index=1),
            lambda d: d['constants'][0].update(value=99),
            lambda d: d['constants'][0].update(expression='__import__("os")'),
            lambda d: d['inputs'][0].update(normal='0.0; bad()'),
            lambda d: d['switches'][0].update(default_value=1),
            lambda d: d['texts'][0].update(name='opaque'),
            lambda d: d['states'][0].update(minimum='0'),
            lambda d: d['settings'][0].update(storage_type='double'),
        ]
        for change in mutations:
            data = copy.deepcopy(DATA)
            change(data)
            with self.assertRaises((ValueError, KeyError)):
                generator.validate(data)

    def test_generated_license_and_sentinel(self):
        output = generator.generate(MANIFEST)[generator.DATA_HEADER].decode()
        self.assertIn((ROOT / 'COPYING').read_text().strip(), output)
        self.assertIn('FLT_MAX', output)
        self.assertNotIn('INPUT_MOTION_STRENGTH', output)
        self.assertIn('unknown settings', DATA['compatibility']['unknown_values'])

    def test_c_and_cpp_field_bytes_match_old_c(self):
        if ARGS.skip_compile:
            self.skipTest('cross-build: native executable tests are registered separately')
        old_header = (FIXTURES / 'mypaintbrush-brushsettings.h').read_text()
        # Extract actual legacy struct declarations; GLib scalar spelling is
        # represented here by its C ABI primitives. No old runtime API is used.
        prefix = '#include <stddef.h>\n#include <float.h>\n#include "mypaintbrush-enum-settings.h"\n'
        prefix += 'typedef char gchar; typedef int gint; typedef float gfloat; typedef int gboolean;\n'
        prefix += '#define None FLT_MAX\n#define False 0\n#define True 1\n#define FALSE 0\n#define TRUE 1\n#define _(x) x\n'
        for _, _, ctype in TABLES:
            prefix += re.search(r'struct _' + ctype + r' \{.*?typedef struct _' + ctype + ' ' + ctype + ';', old_header, re.S)[0] + '\n'
        old = prefix + '\n'.join(table_source(name) for _, name, _ in TABLES) + DUMP
        new = '#include "mypaintbrush-settings-data.h"\n'
        new += '#include "mypaintbrush-settings-data.h"\n'  # header guard
        for category, array, _ in TABLES:
            new += '#define ' + array + ' painter_mypaint_' + category + '\n'
        new += DUMP
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            for name, content in generator.generate(MANIFEST).items():
                (build / name).write_bytes(content)
            results = []
            for name, source, compiler, flags in [
                ('legacy', old, ARGS.cc, ['-std=c11', '-Wno-missing-field-initializers']),
                ('generated-c', new, ARGS.cc, ['-std=c11']),
                ('generated-cpp', new, ARGS.cxx, ['-std=c++14']),
            ]:
                source_file = build / (name + ('.cpp' if name.endswith('cpp') else '.c'))
                source_file.write_text(source)
                executable = build / name
                run(compiler + ['-Wall', '-Wextra', '-Werror'] + flags +
                    ['-I' + str(build), str(source_file), '-o', str(executable)])
                results.append(run([str(executable)]))
            self.assertEqual(results[0], results[1], 'compiled C metadata bytes differ')
            self.assertEqual(results[0], results[2], 'compiled C++14 metadata bytes differ')
            # Prove this comparison detects a changed float, rather than merely
            # comparing two generated copies of the same manifest.
            mutated = copy.deepcopy(DATA)
            mutated['settings'][0]['default_value'] = '0.5'
            (build / generator.DATA_HEADER).write_text(generator.data_header(mutated))
            run(ARGS.cc + ['-std=c11', '-I' + str(build),
                str(build / 'generated-c.c'), '-o', str(build / 'mutated')])
            self.assertNotEqual(results[0], run([str(build / 'mutated')]))


def main():
    global ARGS
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'c++'))
    parser.add_argument('--cc-arg', action='append')
    parser.add_argument('--cxx-arg', action='append')
    parser.add_argument('--skip-compile', action='store_true')
    parser.add_argument('--report', type=Path)
    ARGS = parser.parse_args()
    ARGS.cc = ARGS.cc_arg or shlex.split(ARGS.cc)
    ARGS.cxx = ARGS.cxx_arg or shlex.split(ARGS.cxx)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(BrushSettingsTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if ARGS.report:
        ARGS.report.parent.mkdir(parents=True, exist_ok=True)
        report = {'status': 'PASS' if result.wasSuccessful() else 'FAIL',
                  'scope': 'Pinned metadata and generated C/C++14 data only; no brush rendering or reader/writer compatibility claim',
                  'tests_run': result.testsRun, 'skipped': len(result.skipped), 'errors': len(result.errors), 'failures': len(result.failures),
                  'counts': {key: len(DATA[key]) for key in ['constants', 'inputs', 'settings', 'switches', 'texts', 'states']},
                  'pinned_revision': DATA['provenance']['revision'], 'commands': COMMANDS,
                  'generated_sha256': {key: hashlib.sha256(value).hexdigest() for key, value in generator.generate(MANIFEST).items()}}
        ARGS.report.write_text(json.dumps(report, indent=2) + '\n')
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(main())
