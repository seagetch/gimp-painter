#!/usr/bin/env python3
"""Matched source-only lifecycle proof. DRAFT: run only after review in hosted CI."""
import argparse
import hashlib
import json
import os
import platform
from pathlib import Path

import subprocess
import tarfile
from validate import check_run, check_elf, loaded_paths

ARCHIVE_SHA = 'bcbb7786c1e447703db3bc7fa34d62d0d2d117b22f04d8834c7b2d5ded456487'
PATCH_SHA = '6691ca4835d382f354232905995d50e37680cf2d6098246eccf70431adfe139c'
PATCHED_FILE_SHA = 'ba75bf92f80650022a249085d4f56cb6b7310dfedfa37b7be8750e7619565eca'
CFLAGS = '-O2 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls -fstack-protector-strong -D_FORTIFY_SOURCE=2'
LDFLAGS = '-Wl,-z,relro,-z,now'
SAN = '-fsanitize=address,undefined'
HERE = Path(__file__).resolve().parent
PROOF_FILES = ('prove.py', 'validate.py', 'lifecycle.c', 'test_validate.py',
               'job-fragment.yml', 'README.md')
CI_METADATA = ('GITHUB_SHA', 'GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT',
               'RUNNER_OS', 'RUNNER_ARCH', 'RUNNER_ENVIRONMENT', 'ImageOS', 'ImageVersion')
CPU_FIELDS = ('vendor_id', 'cpu family', 'model', 'model name', 'stepping', 'flags')
TOOL_PACKAGES = ('gcc', 'gcc-13', 'libasan8', 'libubsan1', 'libc6', 'libc6-dev',
                 'liblcms2-2', 'liblcms2-dev', 'meson', 'ninja-build',
                 'pkg-config', 'binutils', 'patch', 'python3', 'python3-minimal')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(argv, env, log, cwd=None, expected=0):
    try:
        proc = subprocess.run(list(map(str, argv)), env=env, cwd=cwd, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=900)
    except subprocess.TimeoutExpired as error:
        for suffix, value in [('stdout', error.stdout), ('stderr', error.stderr)]:
            if isinstance(value, bytes):
                value = value.decode('utf-8', errors='replace')
            Path(str(log) + '.' + suffix).write_text(value or '')
        Path(str(log) + '.exitcode').write_text('TIMEOUT\n')
        raise RuntimeError(f'command timed out: {log}') from error
    Path(str(log) + '.stdout').write_text(proc.stdout)
    Path(str(log) + '.stderr').write_text(proc.stderr)
    Path(str(log) + '.exitcode').write_text(str(proc.returncode) + '\n')
    if expected is not None and proc.returncode != expected:
        raise RuntimeError(f'{argv[0]} exit {proc.returncode}: {log}')
    return proc


def tree(path):
    return {str(p.relative_to(path)): sha(p) for p in sorted(path.rglob('*')) if p.is_file()}


def public_identity():
    # Explicit bounded allowlist: never dump environment, uname node, serials,
    # hostnames, network interfaces, account identifiers, or arbitrary proc files.
    with open('/proc/cpuinfo') as stream:
        first_cpu = stream.read(16384).split('\n\n', 1)[0]
    cpu = {}
    for line in first_cpu.splitlines():
        key, separator, value = line.partition(':')
        if separator and key.strip() in CPU_FIELDS:
            cpu[key.strip()] = value.strip()[:4096]
    return {'ci': {key: os.environ[key][:256] for key in CI_METADATA if key in os.environ},
            'kernel': {'system': platform.system(), 'release': platform.release(),
                       'machine': platform.machine()}, 'cpu': cpu}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--archive', required=True, type=Path)
    ap.add_argument('--patch', required=True, type=Path)
    ap.add_argument('--output', required=True, type=Path)
    a = ap.parse_args()
    # Deliberate accidental-local-execution guard; not a security boundary.
    if os.environ.get('GITHUB_ACTIONS') != 'true' or os.environ.get('RUNNER_ENVIRONMENT') != 'github-hosted':
        raise SystemExit('ordinary GitHub-hosted CI required; no local sanitizer runs')
    if sha(a.archive) != ARCHIVE_SHA or sha(a.patch) != PATCH_SHA:
        raise SystemExit('source/patch digest mismatch')
    a.archive, a.patch = a.archive.resolve(), a.patch.resolve()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    base = {'PATH': '/usr/bin:/bin', 'LANG': 'C', 'LC_ALL': 'C',
            'CC': '/usr/bin/gcc', 'CFLAGS': CFLAGS, 'LDFLAGS': LDFLAGS,
            'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=24:fast_unwind_on_malloc=0',
            'LSAN_OPTIONS': 'exitcode=23:report_objects=0',
            'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1:exitcode=25'}
    # No inherited LD_PRELOAD, compiler flags, sanitizer options or suppression paths.
    versions = {}
    for name in ('gcc', 'meson', 'ninja', 'readelf', 'pkg-config'):
        versions[name] = run([name, '--version'], base, out / ('version-' + name)).stdout
    packages = run(['dpkg-query', '-W', '-f=${binary:Package}\t${Version}\t${Architecture}\n',
                    *TOOL_PACKAGES], base, out / 'tool-packages').stdout
    (out / 'recipe.json').write_text(json.dumps({'environment': base, 'versions': versions,
        'archive_sha256': ARCHIVE_SHA, 'patch_sha256': PATCH_SHA,
        'proof_sha256': {name: sha(HERE / name) for name in PROOF_FILES},
        'public_runner_identity': public_identity(), 'tool_packages': packages,
        'compiler': {'path': str(Path('/usr/bin/gcc').resolve()), 'sha256': sha('/usr/bin/gcc')}}, indent=2))
    results = {}
    trees = {}
    for mode in ('original', 'patched'):
        root = out / mode
        root.mkdir()
        src = root / 'source'
        src.mkdir()
        with tarfile.open(a.archive, 'r:xz') as tf:
            # Reject links, special files, traversal, and unexpected archive root.
            for item in tf.getmembers():
                parts = Path(item.name).parts
                if not parts or parts[0] != 'babl-0.1.114' or '..' in parts or not (item.isfile() or item.isdir()):
                    raise ValueError('unsafe/unexpected source archive entry')
            tf.extractall(src, filter='data')
        src = src / 'babl-0.1.114'
        if mode == 'patched':
            run(['patch', '--batch', '--forward', '--no-backup-if-mismatch', '--fuzz=0', '-p1', '-i', a.patch], base, root / 'patch', cwd=src)
            if sha(src / 'babl/babl-format.c') != PATCHED_FILE_SHA:
                raise ValueError('unexpected patched file')
        trees[mode] = tree(src)
        (root / 'source-tree.json').write_text(json.dumps(trees[mode], indent=2))
        env = dict(base)
        for key, folder in [('HOME', 'home'), ('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache'), ('XDG_DATA_HOME', 'data'), ('XDG_STATE_HOME', 'state'), ('XDG_RUNTIME_DIR', 'runtime'), ('TMPDIR', 'tmp')]:
            directory = root / folder
            directory.mkdir(mode=0o700)
            env[key] = str(directory)
        prefix, build = root / 'prefix', root / 'build'
        run(['meson', 'setup', build, src, '--prefix', prefix, '--libdir', 'lib/x86_64-linux-gnu',
             '--buildtype', 'debugoptimized', '--wrap-mode=nodownload', '-Db_sanitize=address,undefined',
             '-Dwith-docs=false', '-Dgi-docgen=disabled', '-Denable-gir=false', '-Denable-vapi=false',
             '-Denable-sse4_1=false', '-Denable-f16c=false', '-Dwith-lcms=true'], env, root / 'configure')
        run(['meson', 'compile', '-C', build, '-j', '2'], env, root / 'compile')
        run(['meson', 'install', '-C', build, '--no-rebuild'], env, root / 'install')
        lib = prefix / 'lib/x86_64-linux-gnu'
        modules = lib / 'babl-0.1'
        env.update(LD_LIBRARY_PATH=str(lib), BABL_PATH=str(modules),
                   PKG_CONFIG_LIBDIR=str(lib / 'pkgconfig'), PKG_CONFIG_SYSROOT_DIR='')
        pkg = run(['pkg-config', '--cflags', '--libs', 'babl-0.1'], env, root / 'pkg-config').stdout
        import shlex
        probe = root / 'lifecycle'
        run(['/usr/bin/gcc', *shlex.split(CFLAGS), SAN, HERE / 'lifecycle.c', '-o', probe,
             *shlex.split(pkg), '-ldl', *shlex.split(LDFLAGS)], env, root / 'probe-compile')
        artifacts = {str(p.relative_to(prefix)): sha(p) for p in sorted(prefix.rglob('*.so*')) if p.is_file()}
        (root / 'installed-hashes.json').write_text(json.dumps(artifacts, indent=2))
        # Check every installed library/extension, not Meson's temporary build RUNPATH.
        for i, elf in enumerate([probe, *sorted({p.resolve() for p in prefix.rglob('*.so*') if p.is_file()})]):
            dynamic = run(['readelf', '-d', elf], env, root / f'elf-{i}-dynamic').stdout
            program = run(['readelf', '-l', elf], env, root / f'elf-{i}-program').stdout
            check_elf(dynamic, program)
            symbols = run(['readelf', '-Ws', elf], env, root / f'elf-{i}-symbols').stdout
            if '__asan_init' not in symbols:
                raise ValueError(f'uninstrumented owned ELF: {elf}')
        proc = run([probe], env, root / 'lifecycle-run', expected=None)
        leak = check_run(mode, proc.returncode, proc.stdout, proc.stderr)
        loaded = {}
        owned = []
        external = {}
        for raw in loaded_paths(proc.stdout):
            if raw == 'linux-vdso.so.1':
                loaded[raw] = {'sha256': None, 'kind': 'kernel-vdso'}
                continue
            path = Path(raw)
            if not path.is_absolute() or not path.is_file():
                raise ValueError(f'unresolved loaded object {raw}')
            path = path.resolve()
            digest = sha(path)
            loaded[raw] = {'realpath': str(path), 'sha256': digest}
            if path.is_relative_to(prefix):
                owned.append(str(path.relative_to(prefix)))
            else:
                if 'babl' in path.name:
                    raise ValueError('external babl library loaded')
                external[str(path)] = digest
        if len([p for p in owned if Path(p).name.startswith('libbabl-0.1.so.')]) != 1:
            raise ValueError('wrong core library count')
        if not any('/babl-0.1/' in p for p in owned):
            raise ValueError('no extensions observed')
        (root / 'loaded-hashes.json').write_text(json.dumps(loaded, indent=2))
        results[mode] = {'leak': leak, 'probe_sha256': sha(probe), 'installed_names': sorted(artifacts),
                         'loaded_owned_names': sorted(owned), 'external_closure': external}
    if trees['original'].keys() != trees['patched'].keys():
        raise ValueError('source file set changed')
    if [p for p in trees['original'] if trees['original'][p] != trees['patched'][p]] != ['babl/babl-format.c']:
        raise ValueError('source changes exceed destructor patch')
    for field in ('installed_names', 'loaded_owned_names', 'external_closure'):
        if results['original'][field] != results['patched'][field]:
            raise ValueError('unmatched ' + field)
    (out / 'result.json').write_text(json.dumps({'status': 'causal-lifecycle-red-green', 'results': results,
        'scope': 'babl 0.1.114 owned-doc lifecycle only; NOT GIMP acceptance or dependency adoption'}, indent=2))

if __name__ == '__main__':
    main()
