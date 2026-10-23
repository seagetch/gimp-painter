#!/usr/bin/env python3
"""Audit real native ELF exports for original WBS 04.012.

Read-only toward the supplied Meson build: this tool never builds or runs GIMP.
Capture a baseline before changing production, then compare a final snapshot.
Keep all defined dynamic symbols (including weak/unique and versioned names),
cross-check readelf with nm, and demangle only for the private ABI classifier.
The installed C SDK and loadable module entry points are independent controls.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import unittest


APPLICATIONS = ('app/gimp-3.0', 'app/gimp-console-3.0',
                'app/gimp-painter-filter-worker')
PRIVATE_NAMES = ('GimpPainter::', 'GimpPainterXcf::', 'gimp::painter::',
                 '_XcfPainterSave', 'GimpFilterLayerSnapshot',
                 'GimpFilterArgumentPatch', '_GimpFilterArgumentSpec',
                 '_GimpFilterArgumentsSnapshot',
                 'gimp_perspective_guide_undo_get_type()',
                 'gimp_deferred_save_get_type()')
MODULE_ENTRIES = {'gimp_module_query', 'gimp_module_register'}
UPSTREAM_CPP_C_ENTRIES = {'gimp_parallel_init', 'gimp_brush_mipmap_get_mask',
                        'gimp_brush_real_transform_mask',
                        'gimp_pickable_contiguous_region_by_seed'}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def command(args, input_text=None):
    return subprocess.run(args, input=input_text, text=True, check=True,
                          capture_output=True).stdout


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def defined_dynamic_symbols(output):
    """Reject imports/local/hidden rows, retaining all real dynamic exports."""
    result = []
    for line in output.splitlines():
        parts = line.split()
        if len(parts) < 8 or not re.fullmatch(r'\d+:', parts[0]):
            continue
        _, _, _, kind, binding, visibility, section, name = parts[:8]
        if (section == 'UND' or binding not in ('GLOBAL', 'WEAK', 'UNIQUE')
                or visibility not in ('DEFAULT', 'PROTECTED')):
            continue
        result.append({'name': name, 'type': kind, 'binding': binding,
                       'visibility': visibility})
    return sorted(result, key=lambda row: row['name'])


def private_symbol(demangled):
    return any(name in demangled for name in PRIVATE_NAMES)


def is_cpp(name):
    return name.startswith('_Z')


def inspect_elf(path, destination):
    if path.read_bytes()[:4] != b'\x7fELF':
        raise ValueError(f'Expected a genuine ELF output: {path}')
    readelf = command(['readelf', '--dyn-syms', '--wide', str(path)])
    nm = command(['nm', '-D', '--defined-only', '--format=posix', str(path)])
    symbols = defined_dynamic_symbols(readelf)
    names = [row['name'] for row in symbols]
    nm_names = {line.split()[0] for line in nm.splitlines() if line.strip()}
    if set(names) != nm_names:
        raise ValueError(f'readelf/nm disagree for {path}: '
                         f'{sorted(set(names) ^ nm_names)}')
    demangled = command(['c++filt'], '\n'.join(names) + '\n').splitlines()
    if len(demangled) != len(names):
        raise ValueError(f'Incomplete demangler output for {path}')
    for row, readable in zip(symbols, demangled):
        row['demangled'] = readable
    destination.mkdir(parents=True)
    (destination / 'readelf.txt').write_text(readelf)
    (destination / 'nm.txt').write_text(nm)
    write_json(destination / 'symbols.json', symbols)
    return {'sha256': digest(path), 'size': path.stat().st_size,
            'readelf_sha256': digest(destination / 'readelf.txt'),
            'nm_sha256': digest(destination / 'nm.txt'),
            'symbols_sha256': digest(destination / 'symbols.json'),
            'symbols': symbols,
            'private_cpp_exports': [row for row in symbols
                                    if is_cpp(row['name'])
                                    and private_symbol(row['demangled'])],
            'defined_dynamic_count': len(symbols),
            'cpp_dynamic_count': sum(is_cpp(name) for name in names),
            'readelf_nm_exact_agreement': True}


def target_outputs(build):
    targets = json.loads((build / 'meson-info/intro-targets.json').read_text())
    result = {name: 'application' for name in APPLICATIONS}
    for target in targets:
        for filename in target['filename']:
            path = Path(filename).resolve().relative_to(build)
            if (target['type'] == 'shared library'
                    and path.parts[0].startswith('libgimp')):
                result[str(path)] = 'sdk'
            elif target['type'] == 'shared module' and path.parts[0] == 'modules':
                result[str(path)] = 'module'
    return result


def compile_vectors(build):
    entries = json.loads((build / 'compile_commands.json').read_text())
    def argv(entry):
        return entry.get('arguments') or shlex.split(entry['command'])
    return {
        'c': {entry['output']: argv(entry) for entry in entries
              if Path(entry['file']).suffix == '.c'},
        'upstream_cc': {entry['output']: argv(entry) for entry in entries
                        if entry['file'].endswith('.cc')
                        and '/app/' in entry['file']}}


def export_names(artifact):
    return {row['name'] for row in artifact['symbols']}


def upstream_flag_changes(baseline, current):
    """Permit precisely the documented inline visibility flag, in C++ only."""
    flag = '-fvisibility-inlines-hidden'
    changes = {}
    for output in sorted(set(baseline) | set(current)):
        old, new = baseline.get(output), current.get(output)
        if old == new:
            continue
        allowed = (old is not None and new is not None
                   and flag not in old and new.count(flag) == 1
                   and [arg for arg in new if arg != flag] == old)
        changes[output] = {'allowed': allowed, 'before': old, 'after': new}
    return changes


def compare(baseline, current):
    cc_changes = upstream_flag_changes(baseline['compile_vectors']['upstream_cc'],
                                       current['compile_vectors']['upstream_cc'])
    checks = {
        'output_inventory_unchanged': baseline['output_kinds'] == current['output_kinds'],
        'all_c_compile_vectors_unchanged': baseline['compile_vectors']['c'] == current['compile_vectors']['c'],
        'upstream_cc_changes_only_hidden_inline': all(change['allowed']
                                                     for change in cc_changes.values()),
        'installed_manifest_unchanged': baseline['installed'] == current['installed'],
        'private_cpp_exports_absent': all(not artifact['private_cpp_exports']
                                          for artifact in current['artifacts'].values()),
    }
    comparisons = {}
    for name, original in baseline['artifacts'].items():
        if name not in current['artifacts']:
            continue
        final = current['artifacts'][name]
        old_names, new_names = export_names(original), export_names(final)
        removed, added = old_names - new_names, new_names - old_names
        kind = current['output_kinds'][name]
        removed_c = {symbol for symbol in removed if not is_cpp(symbol)}
        preserved = not removed_c
        checks[name + ':c_exports_preserved'] = preserved
        if kind in ('sdk', 'module'):
            checks[name + ':exact_exports_preserved'] = old_names == new_names
            checks[name + ':binary_unchanged'] = original['sha256'] == final['sha256']
        if kind == 'module':
            checks[name + ':module_entries_present'] = MODULE_ENTRIES <= new_names
        if kind == 'application':
            checks[name + ':upstream_cpp_c_entries_present'] = UPSTREAM_CPP_C_ENTRIES <= new_names
        comparisons[name] = {'removed': sorted(removed), 'added': sorted(added),
                             'removed_c': sorted(removed_c),
                             'removed_private_cpp': [row for row in original['symbols']
                                 if row['name'] in removed and is_cpp(row['name'])
                                 and private_symbol(row.get('demangled', row['name']))],
                             'removed_other_cpp': [row for row in original['symbols']
                                 if row['name'] in removed and is_cpp(row['name'])
                                 and not private_symbol(row.get('demangled', row['name']))],
                             'c_exports_preserved': preserved}
    return {'status': 'PASS' if all(checks.values()) else 'FAIL',
            'checks': checks, 'upstream_cc_flag_changes': cc_changes,
            'exports': comparisons}


def capture(args):
    build = args.build_dir.resolve()
    destination = args.output.resolve()
    if destination.is_relative_to(build) or build.is_relative_to(destination):
        raise ValueError('Evidence directory must be separate from the native build')
    destination.mkdir(parents=True, exist_ok=False)
    kinds = target_outputs(build)
    artifacts = {}
    for name in kinds:
        artifacts[name] = inspect_elf(build / name, destination / name)
    installed = json.loads((build / 'meson-info/intro-installed.json').read_text())
    private_installs = {key: value for key, value in installed.items()
                        if '/app/' in key and key.endswith(('.hpp', '.h', '.a'))}
    report = {'format': 1, 'configuration': args.configuration,
              'build_directory': str(build), 'output_kinds': kinds,
              'artifacts': artifacts, 'compile_vectors': compile_vectors(build),
              'installed': installed, 'private_app_sdk_installs': private_installs,
              'private_classifier': list(PRIVATE_NAMES),
              'compile_database_sha256': digest(build / 'compile_commands.json'),
              'build_manifest_sha256': digest(build / 'build.ninja'),
              'gimp_executed': False}
    if private_installs:
        raise ValueError(f'Unexpected private app SDK installs: {private_installs}')
    if args.baseline:
        baseline = json.loads(args.baseline.read_text())
        report['comparison'] = compare(baseline, report)
        report['status'] = report['comparison']['status']
        report['baseline_sha256'] = digest(args.baseline)
    else:
        report['status'] = 'BASELINE'
    write_json(destination / 'report.json', report)
    summary = {'status': report['status'], 'report': str(destination / 'report.json'),
               'outputs': {name: {'dynamic': artifact['defined_dynamic_count'],
                                 'cpp': artifact['cpp_dynamic_count'],
                                 'private_cpp': len(artifact['private_cpp_exports'])}
                           for name, artifact in artifacts.items()}}
    if args.baseline:
        summary['failed_checks'] = [name for name, passed in report['comparison']['checks'].items()
                                    if not passed]
    write_json(destination / 'summary.json', summary)
    print(json.dumps(summary, indent=2))
    return 1 if report['status'] == 'FAIL' else 0


class ParserTests(unittest.TestCase):
    def test_real_exports_only(self):
        rows = '''
1: 000000 0 FUNC GLOBAL DEFAULT UND missing
2: 000001 4 FUNC GLOBAL DEFAULT 10 retained
3: 000002 4 FUNC WEAK PROTECTED 10 weak
4: 000003 8 OBJECT UNIQUE DEFAULT 11 unique
5: 000004 4 FUNC LOCAL DEFAULT 10 local
6: 000005 4 FUNC GLOBAL HIDDEN 10 hidden
7: 000000 0 OBJECT GLOBAL DEFAULT ABS ABI_1
8: 000006 4 FUNC GLOBAL DEFAULT 10 versioned@@ABI_1
'''
        self.assertEqual([row['name'] for row in defined_dynamic_symbols(rows)],
                         ['ABI_1', 'retained', 'unique', 'versioned@@ABI_1', 'weak'])

    def test_private_templates_and_typeinfo(self):
        for name in ('GimpPainter::filter_worker_path[abi:cxx11]()',
                     'std::vector<GimpPainter::Source>::~vector()',
                     'typeinfo for GimpPainterXcf::StorageError',
                     'gimp::painter::xcf::Reader::read()',
                     '_XcfPainterSave::~_XcfPainterSave()',
                     'std::vector<GimpFilterArgumentPatch>::size() const',
                     'std::unique_ptr<GimpFilterLayerSnapshot>::reset()',
                     'std::vector<_GimpFilterArgumentSpec>::size() const',
                     '_GimpFilterArgumentsSnapshot::~_GimpFilterArgumentsSnapshot()',
                     'gimp_deferred_save_get_type()',
                     'gimp_perspective_guide_undo_get_type()'):
            self.assertTrue(private_symbol(name), name)

    def test_upstream_cpp_and_c_not_private(self):
        for name in ('gimp_parallel_init', 'gimp_module_register',
                     'std::vector<int>::size() const', 'TileIterator::next()'):
            self.assertFalse(private_symbol(name), name)

    @staticmethod
    def snapshot(extra=(), private=(), c_flag='-Wall'):
        names = sorted(UPSTREAM_CPP_C_ENTRIES | set(extra))
        return {'output_kinds': {APPLICATIONS[0]: 'application'},
                'compile_vectors': {'c': {'main.c.o': ['cc', c_flag]},
                                    'upstream_cc': {}},
                'installed': {},
                'artifacts': {APPLICATIONS[0]: {
                    'symbols': [{'name': name} for name in names],
                    'private_cpp_exports': list(private)}}}

    def test_private_export_removal_preserves_c(self):
        baseline = self.snapshot(('_ZN11GimpPainter4testEv',), ('GimpPainter::test()',))
        result = compare(baseline, self.snapshot())
        self.assertEqual(result['status'], 'PASS')
        self.assertEqual(result['exports'][APPLICATIONS[0]]['removed'],
                         ['_ZN11GimpPainter4testEv'])

    def test_hidden_c_regression_fails(self):
        result = compare(self.snapshot(('gimp_existing_c_entry',)), self.snapshot())
        self.assertEqual(result['status'], 'FAIL')
        self.assertFalse(result['checks'][APPLICATIONS[0] + ':c_exports_preserved'])

    def test_compile_flag_scope_regression_fails(self):
        result = compare(self.snapshot(), self.snapshot(c_flag='-fvisibility=hidden'))
        self.assertEqual(result['status'], 'FAIL')
        self.assertFalse(result['checks']['all_c_compile_vectors_unchanged'])

    def test_only_specific_cpp_inline_flag_allowed(self):
        old = {'upstream.cc.o': ['c++', '-O2', '-c', 'upstream.cc']}
        good = {'upstream.cc.o': ['c++', '-O2', '-fvisibility-inlines-hidden',
                                 '-c', 'upstream.cc']}
        bad = {'upstream.cc.o': ['c++', '-O2', '-fvisibility=hidden',
                                '-c', 'upstream.cc']}
        self.assertTrue(upstream_flag_changes(old, good)['upstream.cc.o']['allowed'])
        self.assertFalse(upstream_flag_changes(old, bad)['upstream.cc.o']['allowed'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--configuration', choices=('default', 'http'))
    parser.add_argument('--baseline', type=Path)
    args = parser.parse_args()
    if args.self_test:
        result = unittest.TextTestRunner(verbosity=2).run(
            unittest.defaultTestLoader.loadTestsFromTestCase(ParserTests))
        return 0 if result.wasSuccessful() else 1
    if not all((args.build_dir, args.output, args.configuration)):
        parser.error('--build-dir, --output and --configuration are required')
    return capture(args)


if __name__ == '__main__':
    sys.exit(main())
