#!/usr/bin/env python3
"""Focused ASan/UBSan overlays for file-load and Script-Fu startup.

Only the listed production/test sources are recompiled with instrumentation.
The existing build's other objects and third-party libraries remain ordinary.
No normal build object, executable or library is replaced. LeakSanitizer is off.
Run after a successful current normal build with its dependency environment.
"""
import argparse
import datetime
import fcntl
import gzip
import io
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FLAGS = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-O1', '-g']
SOURCES = ['app/pdb/file-cmds.c', 'app/text/gimpfontfactory.c',
           'app/tests/test-file-load-pipeline.c',
           'plug-ins/script-fu/libscriptfu/script-fu-lib.c',
           'plug-ins/script-fu/libscriptfu/scheme-wrapper.c']
FINGERPRINTS = SOURCES + ['tools/in-build-gimp.py',
                          'migration/tests/test_script_fu_startup.py']
CRASH = re.compile(r'fatal error:|Segmentation fault|AddressSanitizer:|'
                   r'UndefinedBehaviorSanitizer|runtime error:|'
                   r'CRITICAL|Traceback', re.I)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--lock', type=Path, default=Path('/workspace/shared/gimp-painter-build.lock'))
    args = parser.parse_args()
    build = args.build.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    report = {'scope': 'focused changed PDB/libscriptfu and native test objects',
              'flags': FLAGS, 'leak_sanitizer': False,
              'uninstrumented_dependencies': True, 'build': str(build),
              'started_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'commands': [], 'results': []}

    def run(command, *, env=None, name, timeout=180):
        report['commands'].append(command)
        result = subprocess.run(command, cwd=build, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=timeout)
        log = output / (name + '.log')
        log.write_text(result.stdout)
        if result.returncode:
            raise RuntimeError(f'{name} exited {result.returncode}; see {log}')
        return result.stdout

    with args.lock.open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        report['source_before'] = {name: digest(ROOT / name) for name in FINGERPRINTS}
        compilation = json.loads((build / 'compile_commands.json').read_text())
        replacements = {}
        compilation_jobs = []
        inputs = {ROOT / name for name in FINGERPRINTS}
        inputs.update(ROOT / name for name in (
            'pdb/groups/file.pdb', 'meson.build', 'app/tests/meson.build',
            'app/tests/tests.c', 'app/tests/gimp-app-test-utils.c',
            'migration/tests/run_native_batch_sanitizers.py'))
        for source in SOURCES:
            entry = next(e for e in compilation
                         if (Path(e['directory']) / e['file']).resolve() == ROOT / source)
            command = shlex.split(entry['command'])
            original = command[command.index('-o') + 1]
            destination = output / (Path(source).name + '.o')
            filtered = []
            index = 0
            while index < len(command):
                if command[index] in ('-o', '-MF', '-MQ', '-MT'):
                    index += 2
                elif command[index] in ('-MD', '-MMD'):
                    index += 1
                else:
                    filtered.append(command[index])
                    index += 1
            dependencies = output / (Path(source).name + '.d')
            run([*filtered, *FLAGS, '-M', '-MT', 'instrumented', '-MF', str(dependencies)],
                name='dependencies-' + Path(source).name)
            dependency_text = dependencies.read_text().replace('\\\n', ' ')
            for filename in shlex.split(dependency_text.split(':', 1)[1]):
                inputs.add((build / filename).resolve())
            compilation_jobs.append((filtered, destination, source))
            replacements[original] = str(destination)

        # Include exactly the source scripts used by the isolated build wrapper.
        installed = json.loads((build / 'meson-info/intro-installed.json').read_text())
        script_root = ROOT / 'plug-ins/script-fu/scripts'
        for source, target in installed.items():
            source, target = Path(source), Path(target)
            if ((source.parent == script_root and target.parent.name == 'scripts') or
                (source.parent == script_root / 'init' and target.parent.name == 'scriptfu-init')):
                inputs.add(source.resolve())
        input_before = {str(path): digest(path) for path in sorted(inputs)}
        archived = {}
        external = {}
        with (output / 'sources.tar.gz').open('wb') as stream:
            with gzip.GzipFile(filename='', mode='wb', fileobj=stream, mtime=0) as compressed:
                with tarfile.open(fileobj=compressed, mode='w') as archive:
                    for path in sorted(inputs):
                        if path.is_relative_to(build):
                            name = 'generated/' + path.relative_to(build).as_posix()
                        elif path.is_relative_to(ROOT):
                            name = 'source/' + path.relative_to(ROOT).as_posix()
                        else:
                            external[str(path)] = input_before[str(path)]
                            continue
                        data = path.read_bytes()
                        entry = tarfile.TarInfo(name)
                        entry.size = len(data)
                        entry.mode = path.stat().st_mode & 0o777
                        archive.addfile(entry, io.BytesIO(data))
                        archived[name] = hashlib.sha256(data).hexdigest()
        report['archived_source_inputs'] = archived
        report['external_header_sha256'] = external
        for filtered, destination, source in compilation_jobs:
            run([*filtered, *FLAGS, '-o', str(destination)],
                name='compile-' + Path(source).name)

        targets = json.loads((build / 'meson-info/intro-targets.json').read_text())
        library_target = next(t for t in targets if t['name'] == 'gimp-scriptfu-3.0')
        library_path = Path(library_target['filename'][0]).relative_to(build)
        for target, name in [('app/tests/file-load-pipeline', 'file-load-pipeline'),
                             ('app/gimp-console-3.0', 'gimp-console-3.0'),
                             (str(library_path), library_path.name)]:
            listing = subprocess.check_output(['ninja', '-t', 'commands', target],
                                              cwd=build, text=True)
            command = shlex.split(listing.splitlines()[-1])
            command[command.index('-o') + 1] = str(output / name)
            command = [replacements.get(argument, argument) for argument in command]
            if name in ('file-load-pipeline', 'gimp-console-3.0'):
                # The replacement registers the same procedure symbols before
                # the ordinary static archive, so its old member is not pulled.
                command.insert(3, replacements['app/pdb/libappinternalprocs.a.p/file-cmds.c.o'])
                command.insert(3, replacements['app/text/libapptext.a.p/gimpfontfactory.c.o'])
            run([*command, *FLAGS], name='link-' + name)
        for name in ('libgimp-scriptfu-3.0.so', 'libgimp-scriptfu-3.0.so.0'):
            destination = output / name
            if destination.is_symlink():
                destination.unlink()
            destination.symlink_to(library_path.name)

        registry = json.loads((build / 'meson-info/intro-tests.json').read_text())
        startup = next(t for t in registry if t['name'] == 'script-fu-startup')
        native = next(t for t in registry if t['name'] == 'file-load-pipeline')
        env = dict(os.environ, **startup['env'])
        for key in ('LD_LIBRARY_PATH', 'GI_TYPELIB_PATH', 'PATH'):
            if key in startup['env'] and os.environ.get(key):
                env[key] += os.pathsep + os.environ[key]
        env['GIMP_SELF_IN_BUILD'] = str(output / 'gimp-console-3.0')
        env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',
                   GSETTINGS_BACKEND='memory')
        # Preload ASan into the ordinary host executable before the instrumented
        # libscriptfu is loaded in plug-in processes.
        env['LD_PRELOAD'] = subprocess.check_output(
            ['cc', '-print-file-name=libasan.so'], text=True).strip()
        env['LD_LIBRARY_PATH'] = str(output) + os.pathsep + env.get('LD_LIBRARY_PATH', '')
        trace_env = dict(env, LD_TRACE_LOADED_OBJECTS='1')
        resolution = run([str(build / 'plug-ins/script-fu/script-fu')],
                         env=trace_env, name='script-fu-library-resolution')
        if str(output / 'libgimp-scriptfu-3.0.so.0') not in resolution:
            raise RuntimeError('Script-Fu did not resolve the instrumented library')
        report['instrumented_scriptfu_resolution_verified'] = True
        with tempfile.TemporaryDirectory(prefix='sanitizer-profile-', dir=output) as profile:
            for directory in ('config', 'cache', 'data', 'tmp'):
                Path(profile, directory).mkdir()
            env.update(XDG_CONFIG_HOME=profile + '/config', XDG_CACHE_HOME=profile + '/cache',
                       XDG_DATA_HOME=profile + '/data', TMPDIR=profile + '/tmp',
                       GIMP3_DIRECTORY=profile + '/gimp')
            native_env = dict(env, **{key: value for key, value in native['env'].items()
                                      if key not in ('LD_LIBRARY_PATH', 'GI_TYPELIB_PATH', 'PATH')})
            for name, command, environment, expected in (
                ('native', [str(output / 'file-load-pipeline')], native_env, 6),
                ('script-fu', [sys.executable, '-B', str(ROOT / 'migration/tests/test_script_fu_startup.py')], env, 7),
            ):
                text = run(command, env=environment, name=name, timeout=480)
                if CRASH.search(text):
                    raise RuntimeError(f'{name} contains runtime failure diagnostics')
                if name == 'native' and len(re.findall(r'(?m)^ok \d+ ', text)) != expected:
                    raise RuntimeError('Native sanitizer case count mismatch')
                if name == 'script-fu' and 'Ran 7 tests' not in text:
                    raise RuntimeError('Script-Fu sanitizer case count mismatch')
                report['results'].append({'name': name, 'passed': True, 'cases': expected})
        input_after = {str(path): digest(path) for path in sorted(inputs)}
        if input_before != input_after:
            raise RuntimeError('Compilation/test input closure changed during sanitizer run')
        report['complete_input_closure_unchanged'] = True
        report['source_after'] = {name: digest(ROOT / name) for name in FINGERPRINTS}
        if report['source_before'] != report['source_after']:
            raise RuntimeError('Instrumented sources changed during sanitizer run')
        report['finished_at'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        report['outputs'] = {p.name: digest(p) for p in output.iterdir()
                             if p.is_file() and not p.is_symlink()}
        report['all_passed'] = True
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print('Focused native 6/6 and Script-Fu 7/7 ASan/UBSan passed; LSan disabled')


if __name__ == '__main__':
    main()
