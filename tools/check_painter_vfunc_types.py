#!/usr/bin/env python3
"""Verify current native vfunc assignments and C/C++ signature rejection controls."""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

from check_painter_vfunc_order import ROOT, METADATA, balanced, clean, discover, function_body


def expression_kind(expression):
    expression = clean(expression).strip()
    if re.fullmatch(r'&?\s*[A-Za-z_]\w*|0', expression):
        return 'null' if expression in ('nullptr', 'NULL', '0') else 'function'
    captureless = re.match(r'\+?\s*\[\s*\]\s*\(', expression)
    if captureless:
        parameters_end = balanced(expression, captureless.end() - 1, '(', ')')
        body_start = expression.find('{', parameters_end)
        if body_start >= 0 and balanced(expression, body_start, '{', '}') == len(expression):
            return 'captureless-lambda'
    raise ValueError('vfunc assignment requires an uncast function or complete captureless lambda: ' + expression)


def assignments():
    types = discover()
    result = []
    for name, info in types.items():
        text = (ROOT / info['source']).read_text()
        for initializer in info['initializers']:
            body = function_body(text, initializer['symbol'])
            actual = []
            for match in re.finditer(r'->\s*(\w+)\s*=(?!=)\s*', body):
                field = match[1]
                if field in METADATA:
                    continue
                depth = {'(': 0, '[': 0, '{': 0}
                closing = {')': '(', ']': '[', '}': '{'}
                end = None
                for offset in range(match.end(), len(body)):
                    char = body[offset]
                    if char in depth:
                        depth[char] += 1
                    elif char in closing:
                        depth[closing[char]] -= 1
                    elif char == ';' and not any(depth.values()):
                        end = offset
                        break
                assert end is not None, (name, field)
                expression = body[match.end():end].strip()
                kind = expression_kind(expression)
                actual.append(field)
                result.append({'type': name, 'source': info['source'],
                               'initializer': initializer['symbol'], 'slot': field,
                               'expression': expression,
                               'kind': kind})
            expected = [field for field in initializer['assigned_fields'] if field not in METADATA]
            assert Counter(actual) == Counter(expected), (name, initializer['symbol'])
    return types, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', required=True, type=Path)
    parser.add_argument('--work-dir', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    types, slots = assignments()
    commands = []

    def execute(command, cwd=None, expected_failure=False):
        process = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
        commands.append({'command': command, 'cwd': str(cwd or ROOT),
                         'exit_code': process.returncode, 'stdout': process.stdout,
                         'stderr': process.stderr, 'expected_failure': expected_failure})
        if expected_failure:
            assert process.returncode != 0 and re.search(r'incompatible|invalid conversion|cannot convert', process.stderr), process.stderr
        else:
            assert process.returncode == 0, process.stderr

    database = json.loads((args.build_dir / 'compile_commands.json').read_text())
    sources = sorted({slot['source'] for slot in slots} |
                     {'app/painter/tests/test-fixture.c', 'app/painter/tests/test-hierarchy.c'})
    for source in sources:
        records = [row for row in database if (Path(row['directory']) / row['file']).resolve() == ROOT / source]
        assert records, 'No actual compile command for ' + source
        row = records[0]
        original = shlex.split(row['command'])
        assert '-fpermissive' not in original
        command = []
        index = 0
        while index < len(original):
            option = original[index]
            if option in ('-o', '-MF', '-MQ'):
                index += 2
                continue
            if option in ('-MD', '-MMD', '-c') or option.startswith('-fdiagnostics-color'):
                index += 1
                continue
            command.append(option)
            index += 1
        command += ['-fsyntax-only']
        if source.endswith('.c'):
            command += ['-Werror=incompatible-pointer-types']
        execute(command, row['directory'])

    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', 'gobject-2.0'], text=True))
    # Use real native class/interface declarations. Invalid controls correspond
    # to the legacy Binder's independently selected return and owner types.
    prototypes = {
        'constructed': 'void probe_constructed (GObject *);',
        'set': 'void probe_set (GObject *, guint, const GValue *, GParamSpec *);',
        'read': 'gint probe_read (PainterReadable *, GError **);',
    }
    controls = {
        'positive': {},
        'wrong-return': {'constructed': 'gint probe_constructed (GObject *);'},
        'wrong-owner': {'constructed': 'void probe_constructed (GParamSpec *);'},
        'wrong-arity': {'constructed': 'void probe_constructed (GObject *, guint);'},
        'wrong-constness': {'set': 'void probe_set (GObject *, guint, GValue *, GParamSpec *);'},
        'wrong-interface-return': {'read': 'gchar *probe_read (PainterReadable *, GError **);'},
    }
    for language, variable, default, standard in [('c', 'CC', 'cc', 'c11'), ('c++', 'CXX', 'c++', 'c++14')]:
        compiler = shlex.split(os.environ.get(variable, default))
        for name, changes in controls.items():
            declarations = dict(prototypes, **changes)
            text = ('#include <glib-object.h>\n#include "test-hierarchy.h"\n' +
                    '\n'.join(declarations.values()) +
                    '\nvoid probe_assign (GObjectClass *klass, PainterReadableInterface *iface)\n{\n'
                    '  klass->constructed = probe_constructed;\n'
                    '  klass->set_property = probe_set;\n'
                    '  iface->read = probe_read;\n}\n')
            path = args.work_dir / (name + ('.c' if language == 'c' else '.cpp'))
            path.write_text(text)
            command = compiler + ['-std=' + standard, '-Wall', '-Wextra', '-Werror', '-fsyntax-only'] + flags + ['-I' + str(ROOT / 'app/painter/tests'), str(path)]
            if language == 'c':
                command += ['-Werror=incompatible-pointer-types']
            execute(command, ROOT, name != 'positive')

    # The current machine does not execute the big-endian stream. Check its
    # exact source declaration against the native slot without pretending to
    # run a big-endian configuration or replacing its implementation.
    endian_source = (ROOT / 'app/xcf/painter-xcf-arguments.cpp').read_text()
    declarations = re.findall(r'\bstatic\s+gssize\s+endian_input_read\s*\([^)]*\)', endian_source)
    assert len(declarations) == 1
    declaration = re.sub(r'^static\s+', '', declarations[0])
    endian_probe = args.work_dir / 'conditional-endian-signature.cpp'
    endian_probe.write_text('#include <gio/gio.h>\n#include <type_traits>\n' + declaration + ';\n'
        'static_assert(std::is_same<decltype(&endian_input_read), '
        'decltype(GInputStreamClass::read_fn)>::value, "native stream slot mismatch");\n')
    execute(shlex.split(os.environ.get('CXX', 'c++')) +
            ['-std=c++14', '-Wall', '-Wextra', '-Werror', '-fsyntax-only'] + flags + [str(endian_probe)], ROOT)

    report = {'task': '06.014', 'status': 'PASS', 'types': len(types),
              'initializers': sum(len(info['initializers']) for info in types.values()),
              'slot_assignments': len(slots), 'assignment_kinds': dict(Counter(slot['kind'] for slot in slots)),
              'actual_sources_syntax_checked': sources,
              'production_type_sources': sorted({info['source'] for info in types.values()}),
              'no_override_sources': sorted({info['source'] for info in types.values()} -
                                            {slot['source'] for slot in slots}),
              'positive_controls': 2, 'negative_controls': 10,
              'conditional_signature_controls': 1,
              'conditional_signature_scope': 'Exact EndianInput declaration versus native GInputStreamClass.read_fn; no big-endian runtime claim',
              'assignments': slots, 'commands': commands,
              'source_sha256': {source: hashlib.sha256((ROOT / source).read_bytes()).hexdigest() for source in sources},
              'limitations': ['Syntax checks use current configured build conditions, not every platform configuration',
                              'Framework G_DEFINE registration and signal G_CALLBACK transport are outside custom vfunc assignment expressions',
                              'This source check rejects casts in slot expressions; it is not a general C++ alias/dataflow analyzer',
                              'Historical 05.011 inventory remains unchanged; feature behavior and platform gates are separate']}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in ['types', 'initializers', 'slot_assignments', 'positive_controls', 'negative_controls', 'status']}))


if __name__ == '__main__':
    main()
