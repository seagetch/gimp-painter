#!/usr/bin/env python3
"""Compile actual path selectors and execute copies in synthetic install layouts.

This proves Linux executable location and fail-closed lookup, not a GIMP package
or native plug-in run. No copied fixture executable launches another process.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ('app/core/gimpfilterpaths.cpp', 'app/core/gimpfilterpaths.hpp',
           'app/painter/filter-procedure.hpp', 'app/painter/filter-context.hpp',
           'migration/tests/test_filter_executable_paths.py')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    sources = out/'sources'
    for name in SOURCES:
        dest = sources/name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT/name, dest)
    before = {name:sha(ROOT/name) for name in SOURCES}
    (sources/'config.h').write_text('/* Standalone Linux selector fixture. */\n')
    (sources/'probe.cpp').write_text(r'''
#include "core/gimpfilterpaths.hpp"
#include <future>
#include <iostream>
#include <thread>
int main (int argc, char **argv) {
  try {
    if (argc != 2) return 2;
    std::promise<std::string> promise;
    auto result = promise.get_future ();
    std::thread thread ([&] {
      try {
        if (std::string (argv[1]) == "worker")
          promise.set_value (GimpPainter::filter_worker_path ());
        else
          promise.set_value (GimpPainter::filter_plugin_path (
            static_cast<GimpPainter::FilterProcedure> (std::stoi (argv[1]))));
      } catch (...) { promise.set_exception (std::current_exception ()); }
    });
    thread.join ();
    std::cout << result.get () << '\n'; return 0;
  } catch (const std::exception& error) { std::cerr << error.what () << '\n'; return 3; }
}
''')
    report = dict(status='incomplete', scope=__doc__, sanitizer=args.sanitize,
                  leak_sanitizer=False, source_sha256=before, cases=[])
    try:
        with tempfile.TemporaryDirectory(prefix='filter paths 日本語-') as temp:
            temp = Path(temp)
            build, installed, overlay = temp/'original build', temp/'moved runtime/usr', temp/'instrumented overlay'
            app, tests = build/'app', build/'app/tests'
            bin_dir, helper_dir = installed/'bin', installed/'libexec'
            plugin_dir = installed/'lib/x86_64-linux-gnu/gimp/3.0/plug-ins/blinds'
            tiles_dir = plugin_dir.parent/'tile-small'
            retinex_dir = plugin_dir.parent/'contrast-retinex'
            for directory in (tests, bin_dir, helper_dir, plugin_dir, tiles_dir, retinex_dir, overlay, build/'plug-ins/common'):
                directory.mkdir(parents=True, exist_ok=True)
            macros = {'GIMP_PAINTER_FILTER_BUILD_ROOT':str(build),
                      'GIMP_PAINTER_FILTER_BIN_TO_WORKER':'../libexec/gimp-painter-filter-worker',
                      'GIMP_PAINTER_FILTER_WORKER_TO_PLUGINS':'../lib/x86_64-linux-gnu/gimp/3.0/plug-ins',
                      'GIMP_PAINTER_FILTER_EXECUTABLE_SUFFIX':'',
                      'GIMP_PAINTER_FILTER_PATHS_OVERLAY_DIR':str(overlay)}
            compiler = shlex.split(os.environ.get('CXX', 'c++'))
            command = compiler + ['-std=c++14', '-Wall', '-Wextra', '-Werror', '-pthread',
                       '-I'+str(sources), '-I'+str(sources/'app')]
            command += ['-D'+name+'='+json.dumps(value, ensure_ascii=False) for name,value in macros.items()]
            if args.sanitize:
                command += ['-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-g', '-O1']
            else:
                command += ['-O2']
            command += [str(sources/'probe.cpp'), str(sources/'app/core/gimpfilterpaths.cpp'), '-o', str(out/'probe')]
            command += shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'glib-2.0'], text=True))
            report['compile_command'] = command
            compiled = subprocess.run(command, text=True, capture_output=True)
            (out/'compile.log').write_text(compiled.stdout+compiled.stderr)
            if compiled.returncode:
                raise RuntimeError('Selector compilation failed; see compile.log')
            report['probe_sha256'] = sha(out/'probe')
            def copy(target):
                shutil.copyfile(out/'probe', target)
                target.chmod(0o755)
                return target
            original = copy(app/'probe')
            build_worker = copy(app/'gimp-painter-filter-worker')
            build_plugin = copy(build/'plug-ins/common/blinds')
            build_tiles = copy(build/'plug-ins/common/tile-small')
            build_retinex = copy(build/'plug-ins/common/contrast-retinex')
            installed_host = copy(bin_dir/'gimp-console-3.0')
            installed_worker = copy(helper_dir/'gimp-painter-filter-worker')
            installed_plugin = copy(plugin_dir/'blinds')
            installed_tiles = copy(tiles_dir/'tile-small')
            installed_retinex = copy(retinex_dir/'contrast-retinex')
            overlay_host = copy(overlay/'gimp-filter-layer')
            overlay_worker = copy(overlay/'gimp-painter-filter-worker')
            overlay_plugin = copy(overlay/'blinds')
            overlay_tiles = copy(overlay/'tile-small')
            overlay_retinex = copy(overlay/'contrast-retinex')
            env = {**os.environ, 'ASAN_OPTIONS':'detect_leaks=0:abort_on_error=1',
                   'UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1',
                   'GIMP3_PLUGINDIR':str(build/'deliberate unrelated environment path')}
            def case(name, exe, role, expected=None):
                result = subprocess.run([str(exe), role], env=env, capture_output=True, text=True, timeout=15)
                good = (result.returncode == 0 and result.stdout.strip() == str(expected)) if expected else result.returncode == 3
                good &= not any(marker in result.stderr for marker in ('AddressSanitizer', 'runtime error:'))
                report['cases'].append(dict(name=name, passed=good, exit_code=result.returncode,
                                           stdout=result.stdout, stderr=result.stderr))
                if not good: raise RuntimeError('Selector case failed: '+name)
            case('build main', original, 'worker', build_worker)
            case('build test executable', copy(tests/'probe'), 'worker', build_worker)
            case('build helper plugin', build_worker, '1', build_plugin)
            case('build Small Tiles literal plugin', build_worker, '2', build_tiles)
            case('build Retinex literal plugin', build_worker, '3', build_retinex)
            case('relocated main ignores accessible build', installed_host, 'worker', installed_worker)
            case('relocated helper ignores accessible build and environment', installed_worker, '1', installed_plugin)
            case('relocated Small Tiles ignores build and environment', installed_worker, '2', installed_tiles)
            case('relocated Retinex ignores build and environment', installed_worker, '3', installed_retinex)
            link = temp/'launcher alias'
            link.symlink_to(installed_host)
            case('symlink launcher resolves actual executable', link, 'worker', installed_worker)
            installed_worker.unlink()
            case('missing relocated helper never falls back', installed_host, 'worker')
            copy(installed_worker).chmod(0o644)
            case('nonexecutable relocated helper never falls back', installed_host, 'worker')
            installed_worker.chmod(0o755)
            installed_plugin.unlink()
            case('missing relocated plugin never falls back', installed_worker, '1')
            installed_tiles.chmod(0o644)
            case('nonexecutable Small Tiles never falls back', installed_worker, '2')
            installed_tiles.unlink()
            case('missing Small Tiles never falls back', installed_worker, '2')
            installed_retinex.chmod(0o644)
            case('nonexecutable Retinex never falls back', installed_worker, '3')
            installed_retinex.unlink()
            case('missing Retinex never falls back', installed_worker, '3')
            case('unknown procedure fails before path lookup', installed_worker, '99')
            case('compile-time instrumentation overlay main', overlay_host, 'worker', overlay_worker)
            case('compile-time instrumentation overlay helper', overlay_worker, '1', overlay_plugin)
            case('compile-time Small Tiles overlay helper', overlay_worker, '2', overlay_tiles)
            case('compile-time Retinex overlay helper', overlay_worker, '3', overlay_retinex)
        report['source_sha256_after'] = {name:sha(ROOT/name) for name in SOURCES}
        report['changed_sources'] = [name for name in SOURCES if report['source_sha256_after'][name] != before[name]]
        if report['changed_sources']: raise RuntimeError('Source changed during selector verification')
        report['status'] = 'passed'
    finally:
        (out/'report.json').write_text(json.dumps(report, indent=2, ensure_ascii=False)+'\n')
    print(json.dumps(dict(status=report['status'], cases=len(report['cases']), sanitizer=args.sanitize)))


if __name__ == '__main__':
    main()
