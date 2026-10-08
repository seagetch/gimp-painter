#!/usr/bin/env python3
"""Bounded, source-isolated regression probe for original WBS 04.013 edges.

Copy real generators, enum headers, and Meson target declarations. All generator
writes are confined to output/baseline and output/fixed. Static fixture consumers
compile real generated core-enums.c/internal-procs.c plus C/C++ header consumers;
small stubs avoid requiring the entire application, and no runtime claim is made.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(cmd, log, cwd=None):
    p = subprocess.run(cmd, cwd=cwd, text=True, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT)
    log.write_text('$ ' + ' '.join(map(str, cmd)) + '\n' + p.stdout)
    if p.returncode:
        raise RuntimeError(f'{p.returncode}: {log}\n{p.stdout[-5000:]}')
    return p.stdout


def put(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


def copy(source, fixture, relative):
    target = fixture / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / relative, target)


def original(source, relative):
    return subprocess.check_output(['git', 'show', f'818ed559:{relative}'],
                                   cwd=source, text=True)


def prepare(source, root, fixed):
    fixture = root / 'source'
    fixture.mkdir(parents=True)
    shutil.copytree(source / 'pdb', fixture / 'pdb')
    put(fixture / 'pdb/meson.build', (source / 'pdb/meson.build').read_text()
        if fixed else original(source, 'pdb/meson.build'))
    for relative in ['tools/gimp-mkenums', 'tools/meson-mkenums.py',
                     'app/core/core-enums.h', 'app/core/core-enums.c',
                     'app/operations/operations-enums.h', 'app/paint/paint-enums.h',
                     'libgimpbase/gimpbaseenums.h', 'libgimpconfig/gimpconfigenums.h',
                     'libgimp/gimpenums.h', 'app/pdb/internal-procs.c',
                     'app/pdb/internal-procs.h']:
        copy(source, fixture, relative)
    core_source = (source / 'app/core/meson.build').read_text() if fixed else original(source, 'app/core/meson.build')
    core = core_source.split('\n)\n', 1)[0] + '\n)\n'
    core += """
core_lib = static_library('core-consumers', ['core-enums.c', 'consumer.c', 'consumer.cpp', stamp_core_enums],
  include_directories: include_directories('../..'), dependencies: gio)
"""
    put(fixture / 'app/core/meson.build', core)
    for suffix in ['c', 'cpp']:
        put(fixture / f'app/core/consumer.{suffix}', f'''#include <glib-object.h>
#include "core-enums.h"
#ifdef __cplusplus
extern "C"
#endif
int core_consumer_{suffix}(void) {{ return GIMP_CONVERT_DITHER_NONE; }}
''')
        put(fixture / f'libgimp/consumer.{suffix}', f'''#include <glib-object.h>
#include "pdb/stamp-enumcode.h"
#include "gimpenums.h"
#ifdef __cplusplus
extern "C"
#endif
int lib_consumer_{suffix}(void) {{ return GIMP_CONVERT_DITHER_NONE; }}
''')
    put(fixture / 'libgimp/meson.build', """
lib_lib = static_library('lib-consumers', ['consumer.c', 'consumer.cpp', stamp_enumcode],
  include_directories: include_directories('..'), dependencies: gio)
""")
    put(fixture / 'app/pdb/meson.build', """
static_library('pdb-consumer', ['internal-procs.c', pdbgen],
  include_directories: include_directories('../..', '../../pdb'), dependencies: gio)
""")
    put(fixture / 'app/pdb/pdb-types.h', 'typedef struct _GimpPDB GimpPDB;\n')
    put(fixture / 'app/pdb/gimppdb.h', '#define GIMP_IS_PDB(p) ((p) != NULL)\n')
    put(fixture / 'config.h', '/* isolated compilation fixture */\n')
    put(fixture / 'gimp-intl.h', '#define NC_(ctx, msg) (msg)\n#define N_(msg) (msg)\n')
    put(fixture / 'libgimpbase/gimpbase.h', '''#include <glib-object.h>
typedef struct { gint value; const gchar *value_desc; const gchar *value_help; } GimpEnumDesc;
typedef struct { guint value; const gchar *value_desc; const gchar *value_help; } GimpFlagsDesc;
void gimp_type_set_translation_context(GType, const gchar *);
void gimp_enum_set_value_descriptions(GType, const GimpEnumDesc *);
void gimp_flags_set_value_descriptions(GType, const GimpFlagsDesc *);
''')
    put(fixture / 'meson.build', """project('original-enum-generator-edge-probe', 'c', 'cpp', default_options: ['warning_level=1'])
python = find_program('python3')
perl = find_program('perl')
mkenums_wrap = find_program('tools/meson-mkenums.py')
gimp_mkenums_source = files('tools/gimp-mkenums')
gio = dependency('gio-2.0')
subdir('pdb')
subdir('app/core')
subdir('libgimp')
subdir('app/pdb')
executable('value-probe', 'value-main.c', link_with: [core_lib, lib_lib])
""")
    put(fixture / 'value-main.c', '''#include <stdlib.h>
#include <stdio.h>
int core_consumer_c(void);
int core_consumer_cpp(void);
int lib_consumer_c(void);
int lib_consumer_cpp(void);
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    const int expected = atoi(argv[1]);
    const int a = core_consumer_c(), b = core_consumer_cpp();
    const int c = lib_consumer_c(), d = lib_consumer_cpp();
    printf("core C=%d, core C++=%d, generated lib C=%d, generated lib C++=%d; expected=%d\\n", a,b,c,d,expected);
    return a != expected || b != expected || c != expected || d != expected;
}
''')
    (root / 'logs').mkdir()
    run(['meson', 'setup', str(root / 'build'), str(fixture)], root / 'logs/setup.log')
    # The actual scripts write temporary files under these build directories.
    (root / 'build/app/pdb').mkdir(exist_ok=True, parents=True)
    return fixture


def state(root):
    paths = {
        'core_stamp': root / 'build/app/core/stamp-core-enums.h',
        'enumgen_stamp': root / 'build/pdb/stamp-enumgen.h',
        'enumcode_stamp': root / 'build/pdb/stamp-enumcode.h',
        'pdbgen_stamp': root / 'build/pdb/stamp-pdbgen.h',
        'core_generated': root / 'source/app/core/core-enums.c',
        'enums_generated': root / 'source/pdb/enums.pl',
        'lib_generated': root / 'source/libgimp/gimpenums.h',
        'pdb_generated': root / 'source/app/pdb/internal-procs.c',
        'pdb_group_app_generated': root / 'source/app/pdb/gimp-cmds.c',
        'pdb_group_lib_generated': root / 'source/libgimp/gimp_pdb.c',
    }
    for path in (root / 'build').rglob('*.o'):
        paths[str(path.relative_to(root / 'build'))] = path
    return {name: {'mtime_ns': path.stat().st_mtime_ns, 'sha256': digest(path)}
            for name, path in paths.items()}


def mutate(path, before, after):
    text = path.read_text()
    assert before in text, (path, before)
    path.write_text(text.replace(before, after, 1))


def scenario(root, name, change):
    before = state(root)
    change()
    log = run(['ninja', '-C', str(root / 'build'), '-d', 'explain'],
              root / f'logs/{name}.log')
    after = state(root)
    artifacts = root / 'artifacts' / name
    artifacts.mkdir(parents=True)
    for path in ['app/core/core-enums.c', 'pdb/enums.pl', 'libgimp/gimpenums.h',
                 'app/pdb/internal-procs.c', 'app/pdb/gimp-cmds.c', 'libgimp/gimp_pdb.c']:
        copy(root / 'source', artifacts, path)
    (artifacts / 'state.json').write_text(json.dumps({'before': before, 'after': after}, indent=2) + '\n')
    changes = {k: {'rebuilt': after[k]['mtime_ns'] != v['mtime_ns'],
                   'content_changed': after[k]['sha256'] != v['sha256']}
               for k, v in before.items()}
    noop = run(['ninja', '-C', str(root / 'build')], root / f'logs/{name}-noop.log')
    assert 'no work to do' in noop, noop
    result = {'name': name, 'changes': changes, 'immediate_noop': True}
    print(root.name, name, [k for k, v in changes.items() if v['rebuilt']], flush=True)
    return result


def validate_results(results, fixed):
    core_object = 'app/core/libcore-consumers.a.p/core-enums.c.o'
    pdb_object = 'app/pdb/libpdb-consumer.a.p/internal-procs.c.o'
    lib_objects = ['libgimp/liblib-consumers.a.p/consumer.c.o',
                   'libgimp/liblib-consumers.a.p/consumer.cpp.o']
    assert results[1]['changes'][core_object]['rebuilt'] == fixed
    assert results[2]['changes'][core_object]['rebuilt']
    assert results[2]['changes']['core_generated']['content_changed']
    for index in [0, 3, 4]:
        for name in [pdb_object] + lib_objects:
            assert results[index]['changes'][name]['rebuilt']
    assert results[4]['changes']['enums_generated']['content_changed']
    for name in lib_objects:
        assert results[5]['changes'][name]['rebuilt']
    assert results[5]['changes']['lib_generated']['content_changed']
    assert results[6]['changes'][pdb_object]['rebuilt']
    assert results[6]['changes']['pdb_generated']['content_changed']
    assert all(row['immediate_noop'] for row in results)


def probe(source, root, fixed):
    fixture = prepare(source, root, fixed)
    run(['ninja', '-C', str(root / 'build')], root / 'logs/initial.log')
    run([str(root / 'build/value-probe'), '0'], root / 'logs/initial-values.log')
    run(['ninja', '-C', str(root / 'build'), '-t', 'deps'], root / 'logs/compiler-deps.log')
    results = []
    def header_edit():
        mutate(fixture / 'app/core/core-enums.h', '  GIMP_CONVERT_DITHER_NONE,',
               '  GIMP_CONVERT_DITHER_NONE = 70,')
        mutate(fixture / 'app/core/core-enums.h', 'desc="None"', 'desc="Isolated enum edge probe"')
    results.append(scenario(root, 'core_header', header_edit))
    run([str(root / 'build/value-probe'), '70'], root / 'logs/header-values.log')
    results.append(scenario(root, 'mkenums_perl', lambda: mutate(
        fixture / 'tools/gimp-mkenums', 'Generated data (by gimp-mkenums)',
        'Generated data (by gimp-mkenums isolated-edge-probe)')))
    results.append(scenario(root, 'mkenums_wrapper', lambda: mutate(
        fixture / 'tools/meson-mkenums.py', "'--fprod', '\\n/* enumerations",
        "'--fprod', '\\n/* isolated wrapper probe */\\n/* enumerations")))
    # util.pl is loaded by enumgen; marker affects only its enums.pl output.
    # With the broken graph, downstream generators run but enums.pl is stale.
    util_insert = '''    my $file = shift;
    if ($file =~ m{/enums\\.pl}) {
        open my $EDGE, '>>', $file or die $!;
        print $EDGE "\\n# isolated util dependency probe\\n";
        close $EDGE;
    }
'''
    results.append(scenario(root, 'enumgen_util', lambda: mutate(
        fixture / 'pdb/util.pl', '    my $file = shift;\n', util_insert)))
    results.append(scenario(root, 'enumgen_perl', lambda: mutate(
        fixture / 'pdb/enumgen.pl', ':# autogenerated by enumgen.pl',
        ':# autogenerated by enumgen.pl isolated-edge-probe')))
    results.append(scenario(root, 'enumcode_perl', lambda: mutate(
        fixture / 'pdb/enumcode.pl', 'This file is autogenerated by enumcode.pl',
        'This file is autogenerated by enumcode.pl isolated-edge-probe')))
    results.append(scenario(root, 'pdbgen_module', lambda: mutate(
        fixture / 'pdb/app.pl', 'This file is auto-generated by pdbgen.pl.',
        'This file is auto-generated by pdbgen.pl isolated-edge-probe.')))
    results.append(scenario(root, 'pdb_group_source', lambda: mutate(
        fixture / 'pdb/groups/gimp.pdb', 'Returns the host GIMP version.',
        'Returns the host GIMP version (isolated dependency probe).')))
    # Both languages observe edited values through source and generated headers.
    run([str(root / 'build/value-probe'), '70'], root / 'logs/final-values.log')
    assert results[0]['changes']['core_generated']['content_changed']
    for name in ['enums_generated', 'lib_generated']:
        assert results[0]['changes'][name]['content_changed']
    assert results[1]['changes']['core_generated']['content_changed'] == fixed
    assert results[3]['changes']['enums_generated']['content_changed'] == fixed
    for index, marker, file in [(1, 'isolated-edge-probe', 'app/core/core-enums.c'),
                               (3, 'isolated util dependency probe', 'pdb/enums.pl')]:
        artifact = root / 'artifacts' / results[index]['name'] / file
        assert (marker in artifact.read_text()) == fixed
    for name in ['app/core/libcore-consumers.a.p/consumer.c.o',
                 'app/core/libcore-consumers.a.p/consumer.cpp.o',
                 'libgimp/liblib-consumers.a.p/consumer.c.o',
                 'libgimp/liblib-consumers.a.p/consumer.cpp.o']:
        assert results[0]['changes'][name]['rebuilt']
        assert results[0]['changes'][name]['content_changed']
    for name in ['pdb_group_app_generated', 'pdb_group_lib_generated']:
        assert results[7]['changes'][name]['content_changed']
    assert results[7]['changes']['pdbgen_stamp']['rebuilt']
    assert results[7]['changes']['app/pdb/libpdb-consumer.a.p/internal-procs.c.o']['rebuilt']
    validate_results(results, fixed)
    return results


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    assert not output.is_relative_to(source), 'probe output must be outside active checkout'
    output.mkdir(parents=True, exist_ok=True)
    results = {}
    for name, fixed in [('baseline', False), ('fixed', True)]:
        root = output / name
        assert not root.exists(), f'use a fresh output directory: {root}'
        results[name] = probe(source, root, fixed)
        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    assert not results['baseline'][1]['changes']['core_stamp']['rebuilt']
    assert results['fixed'][1]['changes']['core_stamp']['rebuilt']
    assert not results['baseline'][3]['changes']['enumgen_stamp']['rebuilt']
    assert results['fixed'][3]['changes']['enumgen_stamp']['rebuilt']
    print('PASS: both absent edges reproduced; both minimal fixes verified')


if __name__ == '__main__':
    main()
