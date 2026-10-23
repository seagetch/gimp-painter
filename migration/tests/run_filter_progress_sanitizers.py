#!/usr/bin/env python3
"""Private focused Filter progress instrumentation; never replace normal outputs.

The shared build/test lock is mandatory. Listed units are instrumented; the
registered production -fno-rtti closure is rebuilt with RTTI only. Dependencies
and the rest of GIMP remain ordinary; LeakSanitizer is explicitly disabled.
"""
import argparse
from datetime import datetime, timezone
import fcntl
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tarfile
from painter_sanitizer_scope import bridge_rtti_sources

ROOT = Path(__file__).resolve().parents[2]
FLAGS = ['-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-O1', '-g']
FOCUSED = {
    'app/painter/binding-store.cpp', 'app/painter/gimp-painter-binding.cpp', 'app/painter/gimp-painter-error.cpp',
    'app/painter/filter-scheduler.cpp', 'app/painter/filter-context.cpp', 'app/painter/filter-spool.cpp', 'app/painter/filter-raster.cpp',
    'app/painter/filter-process.cpp', 'app/painter/filter-wire.cpp', 'app/painter/filter-lifetime.cpp',
    'app/painter/filter-edge.cpp', 'app/painter/filter-gauss.cpp', 'app/painter/filter-raster-kernels.cpp', 'app/painter/filter-native-kernels.cpp',
    'app/core/gimpfilterlayer.cpp', 'app/core/gimpfiltercontext.cpp', 'app/core/gimpfilterprocedure.cpp', 'app/core/gimpfilterpaths.cpp',
    'app/core/gimpprogress.c', 'app/pdb/progress-cmds.c', 'app/dialogs/painter-layer-dialog.cpp',
    'app/painter-filter-worker.cpp', 'plug-ins/common/blinds.c', 'plug-ins/common/tile-small.c',
    'plug-ins/common/contrast-retinex.c', 'plug-ins/common/convolution-matrix.c',
    'app/tests/test-gimp-filter-layer.c', 'app/tests/test-painter-layer-ui.c',
    'app/painter/tests/test-filter-process.cpp', 'app/painter/tests/test-filter-wire.cpp',
    'app/painter/tests/test-filter-kernel-progress.cpp', 'app/painter/tests/test-filter-scheduler.cpp',
    'app/painter/tests/test-filter-owner-gates.cpp',
}
LIVE = ['progress_native_abi', 'progress_start_reentry', 'progress_real_workers',
        'progress_worker_cancel_replace_close', 'progress_independent_owners', 'explicit_cancel_preserves_completed_cache',
        'explicit_cancel_survives_callback_reentry']
UI = ['filter_progress_widgets', 'filter_progress_running_cancel', 'filter_progress_render_reentry',
      'filter_status_last_owner_reentry', 'dialog_binding_close', 'isolated_editor_lifetimes', 'isolated_editor_reentry']
TARGETS = {'app/tests/gimp-filter-layer': 'gimp-filter-layer', 'app/tests/painter-layer-ui': 'painter-layer-ui',
           'app/gimp-painter-filter-worker': 'gimp-painter-filter-worker',
           'plug-ins/common/blinds': 'blinds', 'plug-ins/common/tile-small': 'tile-small',
           'plug-ins/common/contrast-retinex': 'contrast-retinex', 'plug-ins/common/convolution-matrix': 'convolution-matrix',
           **{'app/painter/painter-' + name: 'painter-' + name for name in
              ['filter-wire', 'filter-process', 'filter-kernel-progress', 'filter-scheduler', 'filter-owner-gates']}}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build-installed-filter')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--run', action='store_true')
    parser.add_argument('--lock', type=Path, default=Path('/workspace/shared/gimp-painter-build.lock'))
    args = parser.parse_args()
    build, out = args.build.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    report = dict(started_utc=datetime.now(timezone.utc).isoformat(), status='incomplete',
                  instrumented_sources=sorted(FOCUSED), flags=FLAGS, leak_sanitizer=False,
                  dependency_instrumentation=False, commands=[], results=[])

    def publish():
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')

    def run(command, name, env=None, timeout=300):
        report['commands'].append(command)
        result = subprocess.run(command, cwd=build, env=env, capture_output=True, text=True, timeout=timeout)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        if result.returncode:
            publish()
            raise RuntimeError(name + ' failed: ' + str(result.returncode) + '; see ' + str(out / (name + '.log')))
        return result

    with args.lock.open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        entries = json.loads((build / 'compile_commands.json').read_text())
        compilation = {}
        for entry in entries:
            source = (Path(entry['directory']) / entry['file']).resolve()
            if source.is_relative_to(ROOT):
                name = source.relative_to(ROOT).as_posix()
                if name == 'app/app.c' and not entry['output'].startswith('app/gimp-console-3.0.p/'):
                    continue
                compilation[name] = entry
        rtti_only = bridge_rtti_sources(ROOT, build) - FOCUSED
        report['rtti_compatibility_only_sources'] = sorted(rtti_only)
        selected = FOCUSED | rtti_only
        missing = selected - compilation.keys()
        if missing:
            raise RuntimeError('Unregistered source: ' + ', '.join(sorted(missing)))
        inputs = {ROOT / source for source in selected}
        inputs.update([Path(__file__).resolve(), ROOT / 'migration/tests/painter_sanitizer_scope.py'])
        jobs, replacements = [], {}
        for source in sorted(selected):
            entry = compilation[source]
            command = shlex.split(entry['command'])
            filtered, i = [], 0
            while i < len(command):
                arg = command[i]
                if arg in ('-o', '-MF', '-MQ', '-MT'):
                    i += 2
                    continue
                if arg in ('-MD', '-MMD'):
                    i += 1
                    continue
                filtered.append(arg)
                i += 1
            if source == 'app/core/gimpfilterpaths.cpp':
                filtered.append('-DGIMP_PAINTER_FILTER_PATHS_OVERLAY_DIR="' + str(out) + '"')
            flags = ([] if source in rtti_only else FLAGS) + (['-frtti'] if source.endswith(('.cpp', '.cc')) else [])
            obj = out / (entry['output'].replace('/', '_') + '.o')
            dep = obj.with_suffix('.d')
            run([*filtered, *flags, '-M', '-MT', 'filter-input', '-MF', str(dep)], 'deps-' + obj.name)
            text = dep.read_text().replace('\\\n', ' ')
            inputs.update((build / value).resolve() for value in shlex.split(text.split(':', 1)[1]))
            jobs.append((source, filtered + flags + ['-o', str(obj)], obj))
            replacements[entry['output']] = str(obj)
        before = {str(path): sha(path) for path in sorted(inputs)}
        report['input_sha256_before'] = before
        report['source_sha256'] = {path.relative_to(ROOT).as_posix(): before[str(path)] for path in sorted(inputs)
                                    if path.is_relative_to(ROOT) and not path.is_relative_to(build)}
        with (out / 'sources.tar.gz').open('wb') as stream:
            with gzip.GzipFile(filename='', mode='wb', fileobj=stream, mtime=0) as compressed:
                with tarfile.open(fileobj=compressed, mode='w') as archive:
                    for path in sorted(inputs):
                        if path.is_relative_to(build):
                            name = 'generated/' + path.relative_to(build).as_posix()
                        elif path.is_relative_to(ROOT):
                            name = 'source/' + path.relative_to(ROOT).as_posix()
                        else:
                            continue
                        data = path.read_bytes()
                        info = tarfile.TarInfo(name)
                        info.size = len(data)
                        archive.addfile(info, io.BytesIO(data))
        for source, command, obj in jobs:
            key = hashlib.sha256(json.dumps([command, before], sort_keys=True).encode()).hexdigest()
            keyfile = obj.with_suffix('.key')
            if not obj.exists() or not keyfile.exists() or keyfile.read_text() != key:
                print('compile', source, flush=True)
                run(command, 'compile-' + obj.name)
                keyfile.write_text(key)
        absolute = {str((build / old).resolve()): new for old, new in replacements.items()}
        archives, binaries = {}, {}
        for target, name in TARGETS.items():
            command = shlex.split(subprocess.check_output(['ninja', '-t', 'commands', target], cwd=build, text=True).strip().splitlines()[-1])
            command[command.index('-o') + 1] = str(out / name)
            for archive in sorted({arg for arg in command if arg.endswith('.a')}):
                if archive not in archives:
                    members = [str((build / member).resolve()) for member in subprocess.check_output(['ar', 't', archive], cwd=build, text=True).splitlines()]
                    if not any(member in absolute for member in members):
                        archives[archive] = None
                        continue
                    destination = out / archive.replace('/', '_')
                    destination.unlink(missing_ok=True)
                    run(['ar', 'crsT', str(destination), *[absolute.get(member, member) for member in members]], 'archive-' + destination.name)
                    archives[archive] = str(destination)
                if archives[archive]:
                    command = [archives[archive] if arg == archive else arg for arg in command]
            command = [replacements.get(arg, arg) for arg in command]
            run([*command, *FLAGS], 'link-' + name)
            binaries[name] = sha(out / name)
        report['executable_sha256'] = binaries
        report['compile_time_path_overrides'] = {'filter_worker': str(out / 'gimp-painter-filter-worker'), 'bundled_blinds': str(out / 'blinds'), 'bundled_small_tiles': str(out / 'tile-small'), 'bundled_retinex': str(out / 'contrast-retinex'), 'bundled_convolution': str(out / 'convolution-matrix')}
        report['changed_during_build'] = [name for name, expected in before.items() if sha(name) != expected]
        if report['changed_during_build']:
            publish()
            raise RuntimeError('Inputs changed during sanitizer build')
        if args.run:
            env = os.environ.copy()
            env['LD_LIBRARY_PATH'] = os.pathsep.join(str(p) for p in build.glob('libgimp*') if p.is_dir()) + os.pathsep + env.get('LD_LIBRARY_PATH', '')
            env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(ROOT), GIMP_TESTING_ABS_TOP_BUILDDIR=str(build),
                       GIMP_TESTING_PLUGINDIRS=str(build / 'plug-ins/common'), GSETTINGS_BACKEND='memory',
                       ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',
                       UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            for name in ('painter-filter-wire', 'painter-filter-process', 'painter-filter-kernel-progress', 'painter-filter-scheduler', 'painter-filter-owner-gates'):
                result = run([str(out / name)], name, env)
                report['results'].append(dict(name=name, exit_code=result.returncode))
                print(name, 'PASS', flush=True)
            for binary, prefix, cases in [('gimp-filter-layer', '/gimp-filter-layer/', LIVE), ('painter-layer-ui', '/painter-layer-ui/', UI)]:
                command = [str(out / binary)]
                for name in cases:
                    command += ['-p', prefix + name]
                result = run(command, binary, env, timeout=600)
                actual = set(re.findall(r'^ok \d+ ' + re.escape(prefix) + r'(\S+)$', result.stdout, re.M))
                if actual != set(cases):
                    raise RuntimeError('Selected native cases did not all execute: ' + repr(actual))
                report['results'].append(dict(name=binary, cases=sorted(actual), exit_code=result.returncode))
                print(binary, len(actual), 'PASS', flush=True)
        report['input_sha256_after'] = {name: sha(name) for name in before}
        report['changed_during_run'] = [name for name in before if before[name] != report['input_sha256_after'][name]]
        report['status'] = 'PASS' if not report['changed_during_run'] else 'FAIL'
        report['scope'] = f'{len(FOCUSED)} instrumented sources and {len(rtti_only)} RTTI-only compatibility sources; progress owner/editor, independent native kernels, helper transport and bundled four procedures instrumented; remaining GIMP/dependencies ordinary; LSan off'
        report['finished_utc'] = datetime.now(timezone.utc).isoformat()
        publish()
        if report['status'] != 'PASS':
            raise RuntimeError('Inputs changed during sanitizer run')
    print(out / 'report.json', flush=True)


if __name__ == '__main__':
    main()
