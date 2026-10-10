#!/usr/bin/env python3
"""Original 07.014: native C/C++ bridge runtime, layout and export acceptance.

This is a native test, not a cross-compiler substitute. It does not build GIMP
or modify the existing sanitizer runner/reports. Failures retain their evidence.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
import test_painter_foundation as foundation

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / 'app/painter'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def native_arch(name):
    return {'amd64': 'x86_64', 'x86_64': 'x86_64',
            'arm64': 'arm64', 'aarch64': 'arm64'}.get(name.lower(), name.lower())


def binary_identity(path):
    data = path.read_bytes()
    if data[:4] == b'\x7fELF':
        require(data[4:6] == b'\x02\x01', 'Expected little-endian ELF64')
        return 'ELF', {62: 'x86_64', 183: 'arm64'}.get(struct.unpack_from('<H', data, 18)[0])
    if data[:2] == b'MZ':
        offset = struct.unpack_from('<I', data, 0x3c)[0]
        require(data[offset:offset + 4] == b'PE\0\0', 'Missing PE signature')
        return 'PE', {0x8664: 'x86_64', 0xaa64: 'arm64'}.get(struct.unpack_from('<H', data, offset + 4)[0])
    if data[:4] == b'\xcf\xfa\xed\xfe':
        return 'Mach-O', {0x01000007: 'x86_64', 0x0100000c: 'arm64'}.get(struct.unpack_from('<I', data, 4)[0])
    raise RuntimeError('Unknown native executable format')


def run_command(report, argv, input_text=None, timeout=180):
    argv = list(map(str, argv))
    entry = {'argv': argv, 'exit_code': None, 'output': ''}
    report['commands'].append(entry)
    try:
        result = subprocess.run(argv, input=input_text, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        output = error.stdout or ''
        entry.update(timeout_seconds=timeout,
                     output=output.decode(errors='replace') if isinstance(output, bytes) else output,
                     error='TimeoutExpired')
        raise RuntimeError('Command timed out: ' + shlex.join(argv)) from error
    except OSError as error:
        entry['error'] = str(error)
        raise RuntimeError('Command could not start: ' + shlex.join(argv)) from error
    entry.update(exit_code=result.returncode, output=result.stdout)
    require(result.returncode == 0, 'Command failed: ' + shlex.join(argv) + '\n' + result.stdout)
    return result.stdout


def foundation_results(output):
    require(not re.search(r'^not ok\b|^ok [^\n]*#\s*(?:SKIP|TODO)\b', output, re.M | re.I),
            'Failed, skipped or TODO foundation case')
    cases = re.findall(r'^ok (\d+) (/painter/[^\s#]+)$', output, re.M)
    tests = [name for _, name in cases]
    plan = re.findall(r'^1\.\.(\d+)$', output, re.M)
    require(len(plan) == 1 and int(plan[0]) == len(tests) == len(set(tests)) and len(tests) >= 69
            and [int(number) for number, _ in cases] == list(range(1, len(tests) + 1)),
            'Incomplete foundation execution or skipped tests')
    return tests


def archive_objects(members, system):
    # Apple/BSD ar exposes its generated symbol index in `ar t`; this is not
    # a compiled object. Accept at most one known index on Mach-O only.
    indexes = {'__.SYMDEF', '__.SYMDEF SORTED', '__.SYMDEF_64', '__.SYMDEF_64 SORTED'}
    metadata = [name for name in members if name in indexes]
    require(not metadata or (system == 'darwin' and len(metadata) == 1),
            'Unexpected archive symbol-index members')
    return [name for name in members if name not in indexes]


def pe_exports(raw):
    marker = '[Ordinal/Name Pointer] Table'
    require(marker in raw, 'Missing PE export-name table')
    table = raw.split(marker, 1)[1].split('\n\n', 1)[0]
    # Binutils versions either print [index] name or include ordinal-base and
    # hint columns. Restrict parsing to the actual name table in both formats.
    return re.findall(r'^\s*\[\s*\d+\]\s+(?:\+base\[\s*\d+\]\s+[0-9a-fA-F]+\s+)?(\S+)\s*$', table, re.M)


def macho_exports(raw):
    # dyld_info prints flags after ordinary names. Re-exports have a labelled
    # prefix rather than an address and must also be inspected for private ABI.
    return re.findall(r'^\s*(?:0x[0-9a-fA-F]+|\[re-export\])\s+(\S+)', raw, re.M)


def run(args, report):
    system = platform.system().lower()
    arch = native_arch(platform.machine())
    require((system, arch) == (args.system, args.arch), 'Wrong native host target')
    require(struct.calcsize('P') == 8, 'Expected native 64-bit Python')
    if system == 'windows':
        require(os.environ.get('MSYSTEM') == 'UCRT64', 'Windows gate requires native MSYS2 UCRT64 toolchain')
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    report.update(host={'system': system, 'architecture': arch, 'python': platform.python_version()},
                  ci={key: os.environ.get(key) for key in
                      ['GITHUB_SHA', 'GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT', 'GITHUB_JOB']})

    def execute(argv, input_text=None):
        return run_command(report, argv, input_text)

    cc = shlex.split(os.environ.get('CC', 'cc'))
    cxx = shlex.split(os.environ.get('CXX', 'c++'))
    ar = shlex.split(os.environ.get('AR', 'ar'))
    pkg = shlex.split(os.environ.get('PKG_CONFIG', 'pkg-config'))
    nm = shlex.split(os.environ.get('NM', 'nm'))
    cxx_version = execute(cxx + ['--version'])
    report['toolchain'] = {'cc': execute(cc + ['--version']).splitlines()[0],
                           'cxx': cxx_version.splitlines()[0],
                           'cc_target': execute(cc + ['-dumpmachine']).strip(),
                           'cxx_target': execute(cxx + ['-dumpmachine']).strip(),
                           'glib': execute(pkg + ['--modversion', 'gobject-2.0']).strip()}
    cflags = shlex.split(execute(pkg + ['--cflags', 'gobject-2.0']))
    libs = shlex.split(execute(pkg + ['--libs', 'gobject-2.0']))
    flags = ['-Wall', '-Wextra', '-Werror', '-g', '-fvisibility=hidden']
    cppflags = ['-std=c++14', '-fexceptions', '-fno-rtti', '-fvisibility-inlines-hidden']
    if 'clang' in cxx_version.lower():
        # Existing ownership tests deliberately self-copy/move. Keep diagnostics
        # visible without treating those explicit test operations as errors.
        cppflags += ['-Wno-error=self-assign-overloaded', '-Wno-error=self-move']
    includes = ['-I' + str(MODULE), '-I' + str(MODULE / 'tests')]
    sources = foundation.C_SOURCES + foundation.CPP_SOURCES + foundation.TEST_CPP
    tracked = [MODULE / name for name in sources + foundation.C_HEADERS + foundation.CPP_HEADERS +
               ['gimp-painter-visibility.h', 'tests/test-c-api.h', 'tests/test-fixture.h',
                'tests/test-fixture-traits.hpp', 'tests/test-hierarchy.h', 'tests/test-registry.hpp',
                'tests/platform-abi.h', 'tests/platform-abi.c', 'tests/platform-abi.cpp']]
    tracked += [Path(__file__).resolve(), Path(foundation.__file__).resolve()]
    report['source_sha256'] = {str(p.relative_to(ROOT)): digest(p) for p in sorted(set(tracked))}

    def compile_source(name, suffix='', extra=()):
        source = MODULE / name
        output = build / (source.name + suffix + '.o')
        is_c = source.suffix == '.c'
        execute((cc if is_c else cxx) + flags + (['-std=c11'] if is_c else cppflags) +
                list(extra) + cflags + includes + ['-c', source, '-o', output])
        return output

    objects = {name: compile_source(name) for name in sources}
    archive = build / 'libapppainter.a'
    # ar rcs updates rather than replaces a pre-existing archive. A reused
    # build directory must never retain objects outside the sealed source set.
    with tempfile.TemporaryDirectory(prefix='archive-', dir=build) as directory:
        fresh = Path(directory) / archive.name
        execute(ar + ['rcs', fresh] + [objects[name] for name in foundation.CPP_SOURCES])
        members = execute(ar + ['t', fresh]).splitlines()
        object_members = archive_objects(members, system)
        require(object_members == [objects[name].name for name in foundation.CPP_SOURCES],
                'Archive contains unrecorded or missing objects')
        fresh.replace(archive)
    report['archive_members'] = members
    report['archive_object_members'] = object_members
    extension = '.exe' if system == 'windows' else ''

    def link(name, objects, export=False):
        path = build / (name + extension)
        export_flags = (['-Wl,--export-dynamic'] if system == 'linux' else
                        ['-Wl,-export_dynamic'] if system == 'darwin' else []) if export else []
        execute(cxx + flags + list(objects) + [archive] + libs + ['-pthread'] + export_flags + ['-o', path])
        identity = binary_identity(path)
        require(identity == ({'linux': 'ELF', 'windows': 'PE', 'darwin': 'Mach-O'}[system], arch),
                'Wrong executable format/architecture')
        report['binaries'][path.name] = {'format': identity[0], 'architecture': identity[1], 'sha256': digest(path)}
        return path

    executable = link('painter-foundation', [obj for name, obj in objects.items() if name not in foundation.CPP_SOURCES])
    output = execute([executable])
    report['foundation_tests'] = foundation_results(output)
    c_obj = compile_source('tests/platform-abi.c', extra=['-fvisibility=default'])
    cpp_obj = compile_source('tests/platform-abi.cpp', extra=['-fvisibility=default'])
    abi = link('painter-platform-abi', [c_obj, cpp_obj], export=True)
    output = execute([abi])
    require('PLATFORM_ABI_PASS pointer=8 ' in output, 'Missing native layout/callback result')
    report['abi_result'] = output.strip()

    def normalize(name):
        return name[1:] if system == 'darwin' and name.startswith('_') else name

    def exports(path):
        if system == 'linux':
            raw = execute(nm + ['-D', '--defined-only', '--format=posix', path])
            names = [line.split()[0] for line in raw.splitlines() if line.strip()]
        elif system == 'windows':
            raw = execute(['objdump', '-p', path])
            names = pe_exports(raw)
        else:
            raw = execute(['xcrun', 'dyld_info', '-exports', path])
            names = macho_exports(raw)
        names = sorted(set(normalize(n) for n in names))
        require('painter_platform_export_control' in names, 'Export inspection missed positive control')
        demangled = execute(['c++filt'], '\n'.join(names) + '\n').splitlines()
        require(len(names) == len(demangled), 'Incomplete export demangling')
        private = [n for n in demangled if 'GimpPainter::' in n]
        return {'names': names, 'private_cpp_exports': private}

    report['exports'] = exports(abi)
    require(not report['exports']['private_cpp_exports'], 'Private bridge C++ symbols exported')
    # Explicit negative control proves the classifier sees an actual exported
    # private-name symbol in this platform's binary, not just a nonempty table.
    negative_obj = compile_source('tests/platform-abi.cpp', '-negative',
                                  ['-fvisibility=default', '-DPAINTER_PLATFORM_NEGATIVE_EXPORT'])
    negative = link('painter-platform-leak-control', [c_obj, negative_obj], export=True)
    detected = exports(negative)['private_cpp_exports']
    require(any('GimpPainter::platform_export_leak_control()' == name for name in detected),
            'Export leak control was not detected')
    report['negative_export_control'] = detected
    symbols = execute(nm + (['-U', '-j'] if system == 'darwin' else ['--defined-only', '--format=posix']) + [abi])
    names = {normalize(line.strip() if system == 'darwin' else line.split()[0])
             for line in symbols.splitlines() if line.strip()}
    required = ['gimp_painter_binding_close', 'gimp_painter_error_quark',
                'painter_platform_cpp_layout', 'painter_platform_roundtrip']
    require(set(required) <= names, 'Missing unmangled linked C boundary symbols')
    report['unmangled_c_symbols'] = required
    if system == 'linux':
        runtime = execute(['readelf', '-d', abi])
    elif system == 'windows':
        runtime = execute(['objdump', '-p', abi])
    else:
        runtime = execute(['xcrun', 'dyld_info', '-platform', '-linked_dylibs', abi])
    require(('libstdc++' in runtime if system != 'darwin' else 'libc++' in runtime), 'C++ runtime dependency missing')
    require('gobject-2.0' in runtime, 'GObject runtime dependency missing')
    report['runtime_dependencies'] = runtime
    report['archive_sha256'] = digest(archive)
    require(all(digest(ROOT / name) == value for name, value in report['source_sha256'].items()),
            'Source changed during platform verification')
    report['status'] = 'PASS'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system', required=True, choices=['linux', 'windows', 'darwin'])
    parser.add_argument('--arch', required=True, choices=['x86_64', 'arm64'])
    parser.add_argument('--build-dir', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    report = {'task': '07.014', 'status': 'FAIL', 'scope': 'Native common bridge only; no full-GIMP/platform release claim',
              'commands': [], 'binaries': {}}
    try:
        run(args, report)
    except Exception as error:
        report['failure'] = str(error)
    finally:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: report.get(key) for key in ['status', 'host', 'failure', 'abi_result']}))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
