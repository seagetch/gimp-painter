#!/usr/bin/env python3
"""Bounded four-route acceptance of one installed Linux runtime prototype.

Owns the build/test lock and never builds, installs, recaptures old oracles, or
changes source/build siblings. Supply freshly prepared native XCF fixtures:
--small-tiles-fixtures contains small-tiles-{0,3}.xcf, --retinex-fixtures contains
retinex-{0,1}.xcf, and --convolution-inputs has the existing installed checker's
manifest.json. Fixture production requires a separate native build/display run.

The two corpus drivers are non-installed test binaries from --build. Each runs
once, with the exact relocated helper and its executable-relative plug-ins.
/proc executable/library observations are sampled evidence, not a syscall audit.
"""
from __future__ import annotations

import argparse
import contextlib
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
TRIPLET = 'x86_64-linux-gnu'
PLUGINS = ('blinds', 'tile-small', 'contrast-retinex', 'convolution-matrix')
BUILD_FILES = ('app/gimp-3.0', 'app/gimp-console-3.0',
               'app/gimp-painter-filter-worker', 'app/painter-filter-procedure',
               'app/painter-filter-convolution', 'app/tests/filter-quit-fixture',
               'config.h', 'build.ninja',
               *(f'plug-ins/common/{name}' for name in PLUGINS))
CORPUS_FILES = ('legacy-filter-context.tar.gz', 'legacy-filter-context.tar.manifest.json',
                'legacy-filter-context-expansion.tar.gz',
                'legacy-filter-context-expansion.tar.manifest.json',
                'small-tiles-evidence.tar.gz', 'small-tiles-evidence.tar.manifest.json',
                'retinex-evidence.tar.gz', 'retinex-evidence.tar.manifest.json',
                'legacy-convolution.tar.gz', 'legacy-convolution.tar.manifest.json',
                'legacy-blinds-package-smoke.json', 'legacy-blinds-package-smoke.tar.gz')
PROCEDURE_MARKERS = (
    '320 actual legacy Blinds oracles byte-exact',
    '196 actual legacy SmallTiles oracles byte-exact (RGB/RGBA/Gray/Gray-alpha, factors0..6)',
    '174 actual legacy Retinex oracles byte-exact (87 native RGB,87 native RGBA)',
    '6 analytic selected Retinex ROI compositions matched old whole-fixture outputs',
    '2 analytic native Retinex raw-shadow ROI cases and 4 RGB carrier rejection cases passed',
    '12 native Blinds identity cases passed')
CONVOLUTION_MARKERS = (
    '296 genuine old Convolution final merges, 296 raw ROI comparisons, '
    '8 old geometry rejections, 41 native analytic requests passed',)


def module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT/relative)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


package = module('acceptance_package', 'tools/package-linux-runtime.py')
observer = module('acceptance_observer', 'tools/check_filter_active_quit.py')


def sha(path):
    return package.sha(Path(path))


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False)+'\n')


def paths_seal(paths):
    return {str(path): package.path_fingerprint(path) for path in sorted(set(paths))}


def inputs_seal(args):
    files = [args.build/name for name in BUILD_FILES]
    files += [ROOT/'migration/fixtures'/name for name in CORPUS_FILES]
    files += [ROOT/'migration/tests/filter-active-quit-pdb/fixtures/quit-blinds-1.xcf']
    files += [ROOT/'migration/tests/filter-convolution'/name for name in
              ('installed-inputs.tar.gz', 'installed-inputs.tar.manifest.json')]
    for directory in (ROOT/'migration/fixtures/legacy-blinds', args.small_tiles_fixtures,
                      args.retinex_fixtures, args.convolution_inputs):
        files.extend(p for p in directory.rglob('*') if p.is_file() or p.is_symlink())
    return dict(source_commit=package.command(['git', 'rev-parse', 'HEAD'], cwd=ROOT),
                source=package.source_inventory(ROOT), gimp_data=package.data_state(ROOT/'gimp-data'),
                inputs=paths_seal(files))


def verify_manifest(bundle):
    manifest = json.loads((bundle/'file-manifest.json').read_text())
    actual = package.inventory(bundle)
    unexpected = sorted(set(actual)-set(manifest)-{'file-manifest.json'})
    changed = sorted(name for name, value in manifest.items() if actual.get(name) != value)
    escaping = sorted(str(p.relative_to(bundle)) for p in bundle.rglob('*')
                      if p.is_symlink() and not p.resolve().is_relative_to(bundle))
    return dict(passed=not (unexpected or changed or escaping),
                verified_files=len(manifest), changed_or_missing=changed,
                unexpected_files=unexpected, escaping_symlinks=escaping,
                file_manifest_sha256=sha(bundle/'file-manifest.json'))


def verify_old_blinds():
    root = ROOT/'migration/fixtures/legacy-blinds'
    capture = root/'capture-report.json'
    if sha(capture) != '1f0eeb4d82d8d5d5b55d25717eda9a3d1355c96773f2d279707d384b9771f6a8':
        raise RuntimeError('Genuine-old Blinds capture provenance changed')
    report = json.loads(capture.read_text())
    if (report['status'] != 'passed' or report['errors'] or report['exit_code'] or
            report['source_commit'] != 'afa43fae3e920210146abed514f136fd49f671b5' or
            report['case_count'] != 320 or report['captured_count'] != 320 or
            len(report['cases']) != 320 or sha(root/'fixtures.tsv') != report['fixtures_sha256']):
        raise RuntimeError('Incomplete genuine-old Blinds capture')
    for item in report['inputs']:
        if sha(root/item['loaded']) != item['loaded_sha256']:
            raise RuntimeError('Genuine-old Blinds loaded input changed')
    for item in report['cases']:
        if sha(root/item['output']) != item['output_sha256']:
            raise RuntimeError('Genuine-old Blinds output changed')
    return dict(capture_report_sha256=sha(capture), case_count=320,
                fixtures_sha256=report['fixtures_sha256'], source_commit=report['source_commit'])


def verify_convolution_inputs(directory):
    archive = ROOT/'migration/tests/filter-convolution/installed-inputs.tar.gz'
    manifest_path = archive.with_suffix('.manifest.json')
    manifest = json.loads(manifest_path.read_text())
    expected = 'a7988e113b7c4d9ca0b42feb6c91d5d4a56a94001cc5e30f488eb7c40149ae5a'
    if (sha(archive) != expected or manifest['sha256'] != expected or
            sha(manifest_path) != 'd0b571587a63f5b40773e57b0ad2aa91e80ec27f9f2d6216f18bda5fcf373e8d'):
        raise RuntimeError('Historic synthetic Convolution fixture archive changed')
    for name, item in manifest['members'].items():
        path = (directory/name).resolve()
        if (not path.is_relative_to(directory) or not path.is_file()
                or path.stat().st_size != item['size'] or sha(path) != item['sha256']):
            raise RuntimeError('Extracted Convolution fixture differs from sealed export: '+name)
    return dict(archive_sha256=expected, manifest_sha256=sha(manifest_path),
                provenance='Immutable prior synthetic native export; not a genuine-old oracle or current-output recapture')


def minimal_environment(profile):
    for name in ('home', 'config', 'cache', 'data', 'tmp'):
        (profile/name).mkdir(parents=True, exist_ok=False)
    return dict(PATH='/usr/bin:/bin', LANG='C.UTF-8', HOME=str(profile/'home'),
                XDG_CONFIG_HOME=str(profile/'config'), XDG_CACHE_HOME=str(profile/'cache'),
                XDG_DATA_HOME=str(profile/'data'), TMPDIR=str(profile/'tmp'),
                PYTHONDONTWRITEBYTECODE='1')


def corpus_environment(bundle, profile):
    env = minimal_environment(profile)
    lib, deps = bundle/'usr/lib'/TRIPLET, bundle/'deps/usr/lib'/TRIPLET
    # Match AppRun's runtime search roots without invoking its final exec or
    # inheriting the caller's build/dependency environment.
    env.update(LD_LIBRARY_PATH=os.pathsep.join(map(str, (lib, deps, deps/'blas', deps/'lapack'))),
               BABL_PATH=str(deps/'babl-0.1'), GEGL_PATH=str(deps/'gegl-0.4'),
               GI_TYPELIB_PATH=os.pathsep.join(map(str, (lib/'girepository-1.0', deps/'girepository-1.0'))),
               GIO_MODULE_DIR=str(deps/'gio/modules'),
               GSETTINGS_SCHEMA_DIR=str(bundle/'deps/usr/share/glib-2.0/schemas'),
               XDG_DATA_DIRS=f'{bundle}/usr/share:{bundle}/deps/usr/share:/usr/share',
               GTK_EXE_PREFIX=str(bundle/'deps/usr'), GTK_DATA_PREFIX=str(bundle/'deps/usr'),
               GTK_PATH=str(deps/'gtk-3.0'), GIMP_PAINTER_BUNDLE=str(bundle),
               GIMP3_DATADIR=str(bundle/'usr/share/gimp/3.0'),
               GIMP3_LOCALEDIR=str(bundle/'usr/share/locale'),
               GIMP3_SYSCONFDIR=str(bundle/'usr/etc/gimp/3.0'),
               GIMP3_PLUGINDIR=str(lib/'gimp/3.0'))
    return env


class Observation:
    """Observe only test-owned descendants; never use PID alone for cleanup."""
    def __init__(self, bundle):
        self.bundle = bundle
        self.expected = {'gimp-painter-filter-worker': str(bundle/'usr/libexec/gimp-painter-filter-worker')}
        self.expected.update({name: str(bundle/'usr/lib'/TRIPLET/'gimp/3.0/plug-ins'/name/name)
                              for name in PLUGINS})
        self.processes, self.profiles, self.libraries = {}, set(), {}
        self.map_errors = set()

    def capture(self, current):
        self.processes.update({(p['pid'], p['start_ticks']): p for p in current.values()})
        for item in current.values():
            name = Path(item['exe']).name
            if name == 'gimp-painter-filter-worker' and len(item['argv']) == 3:
                self.profiles.add(item['argv'][2])
            if name not in self.expected and name not in ('gimp-3.0', 'gimp-console-3.0'):
                continue
            if not observer.still_same(item):
                continue
            try:
                rows = (Path('/proc')/str(item['pid'])/'maps').read_text().splitlines()
            except (FileNotFoundError, ProcessLookupError):
                continue
            except PermissionError as error:
                self.map_errors.add(str(error))
                continue
            if not observer.still_same(item):
                continue
            for row in rows:
                columns = row.split(None, 5)
                if (len(columns) != 6 or not columns[5].startswith('/') or
                        not re.search(r'\.so(?:\.\d+)*$', Path(columns[5]).name)):
                    continue
                path = columns[5]
                self.libraries.setdefault(item['exe'], set()).add(path)
        return current

    def descendants(self, pid):
        return self.capture(observer.descendants(pid))

    def cleanup(self):
        survivors = [p for p in self.processes.values() if observer.still_same(p)]
        for item in survivors:
            # Revalidate group identity as well as start time immediately before
            # signalling; do not kill a reused group or our own process group.
            current = observer.record(item['pid'])
            if (current and current['start_ticks'] == item['start_ticks'] and
                    current['group'] == item['group'] and current['group'] != os.getpgrp()):
                try:
                    os.killpg(current['group'], signal.SIGKILL)
                except ProcessLookupError:
                    pass
        return survivors

    def report(self):
        wrong = [p for p in self.processes.values() if Path(p['exe']).name in self.expected
                 and p['exe'] != self.expected[Path(p['exe']).name]]
        helpers = [p for p in self.processes.values() if Path(p['exe']).name == 'gimp-painter-filter-worker']
        bad_protocol = [p for p in helpers if len(p['argv']) != 3 or p['argv'][1] != '--filter-worker-v6']
        libraries = {exe: sorted(values) for exe, values in sorted(self.libraries.items())}
        external = sorted({path for values in libraries.values() for path in values
                           if not Path(path).is_relative_to(self.bundle)
                           and Path(path).name not in package.BASE_ABI})
        survivors = [p for p in self.processes.values() if observer.still_same(p)]
        leftovers = sorted(path for path in self.profiles if Path(path).exists())
        return dict(wrong_installation=wrong, bad_helper_protocol=bad_protocol,
                    observed_helpers=helpers,
                    observed_plugins=[p for p in self.processes.values() if Path(p['exe']).name in PLUGINS],
                    survivors=survivors, leftover_profiles=leftovers,
                    sampled_loaded_libraries=libraries, unexpected_external_libraries=external,
                    library_sampling_errors=sorted(self.map_errors),
                    sampling_scope='Test-owned /proc exe and maps samples; short-lived processes or mappings may be missed; no resource syscall audit')


@contextlib.contextmanager
def observe_checker(checker, observation):
    original = checker.observer.descendants
    checker.observer.descendants = lambda pid: observation.capture(original(pid))
    try:
        yield
    finally:
        checker.observer.descendants = original


def run_process(command, env, output, observation, timeout, markers=(), required_plugins=()):
    output.mkdir(parents=True, exist_ok=False)
    errors, began = [], time.monotonic()
    with (output/'stdout.log').open('w') as stdout, (output/'stderr.log').open('w') as stderr:
        process = subprocess.Popen(list(map(str, command)), env=env, cwd=output,
                                   stdout=stdout, stderr=stderr, start_new_session=True)
        try:
            while process.poll() is None:
                observation.descendants(process.pid)
                if time.monotonic()-began > timeout:
                    errors.append(f'Run exceeded {timeout} seconds')
                    observation.cleanup()
                    if process.poll() is None:
                        os.killpg(process.pid, signal.SIGKILL)
                    break
                time.sleep(0.01)
            code = process.wait(timeout=10)
        finally:
            if process.poll() is None:
                observation.cleanup()
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=10)
    stdout = (output/'stdout.log').read_text(errors='replace')
    stderr = (output/'stderr.log').read_text(errors='replace')
    if code:
        errors.append(f'Exit status {code}')
    for marker in markers:
        if stdout.splitlines().count(marker) != 1:
            errors.append('Expected exactly one corpus result: '+marker)
    for diagnostic in observer.SANITIZER_DIAGNOSTICS + ('Traceback (most recent call last)',):
        if diagnostic in stdout or diagnostic in stderr:
            errors.append('Runtime diagnostic: '+diagnostic)
    process_report = observation.report()
    for field in ('wrong_installation', 'bad_helper_protocol', 'survivors', 'leftover_profiles',
                  'unexpected_external_libraries'):
        if process_report[field]:
            errors.append('Unexpected process observation: '+field)
    actual_plugins = {Path(p['exe']).name for p in process_report['observed_plugins'] if '-run' in p['argv']}
    if not set(required_plugins) <= actual_plugins:
        errors.append('Did not observe every required relocated plug-in -run: '+repr(sorted(set(required_plugins)-actual_plugins)))
    if errors:
        observation.cleanup()
    result = dict(status='failed' if errors else 'passed', errors=errors, exit_code=code,
                  seconds=time.monotonic()-began, command=list(map(str, command)),
                  stdout_sha256=sha(output/'stdout.log'), stderr_sha256=sha(output/'stderr.log'),
                  expected_result_markers=list(markers), **process_report)
    write_json(output/'report.json', result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('build', 'bundle', 'output', 'small-tiles-fixtures', 'retinex-fixtures', 'convolution-inputs'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--lock', type=Path, default=Path('/workspace/shared/gimp-painter-build.lock'))
    args = parser.parse_args()
    for name in ('build', 'bundle', 'output', 'small_tiles_fixtures', 'retinex_fixtures', 'convolution_inputs'):
        setattr(args, name, getattr(args, name).resolve())
    if args.output.exists():
        parser.error('Output must be new; acceptance never overwrites prior evidence')
    for name in BUILD_FILES:
        if not (args.build/name).is_file():
            parser.error('Missing completed build input: '+name)
    for directory, names in ((args.small_tiles_fixtures, ('small-tiles-0.xcf', 'small-tiles-3.xcf')),
                             (args.retinex_fixtures, ('retinex-0.xcf', 'retinex-1.xcf')),
                             (args.convolution_inputs, ('manifest.json',))):
        if any(not (directory/name).is_file() for name in names):
            parser.error('Missing prepared fixture files in '+str(directory))
    with args.lock.open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        return execute(args)


def execute(args):
    args.output.mkdir(parents=True)
    report = dict(format=1, status='failed', errors=[], runs=[],
                  scope='Phase E bounded Linux four-route installed runtime prototype; not candidate, full aggregate or release',
                  corpus_invocations=0,
                  limitations=['Corpus drivers are non-installed current-build harnesses; helper and plug-ins are the chosen relocated runtime',
                               'Executable/library sampling is not a complete file/resource syscall audit',
                               'No GUI, Wayland, physical tablet, Windows/macOS or distribution-source/license acceptance'])
    before = inputs_seal(args)
    write_json(args.output/'input-seal-before.json', before)
    original_manifest = verify_manifest(args.bundle)
    original_inventory = package.inventory(args.bundle)
    relocated = args.output/'baseline/relocated path 日本語'/args.bundle.name
    observations = []
    try:
        if not original_manifest['passed']:
            raise RuntimeError('Source runtime file manifest is not intact')
        manifest = json.loads((args.bundle/'build-manifest.json').read_text())
        if manifest['status'] != 'prototype-not-a-release':
            raise RuntimeError('This bounded acceptance runner requires a prototype runtime')
        if manifest['source_files_sha256'] != before['source']['files_sha256']:
            raise RuntimeError('Runtime source inventory differs from the current checkout')
        if (manifest['gimp_data_commit'] != before['gimp_data']['commit'] or
                manifest['gimp_data_files_sha256'] != before['gimp_data']['inventory']['files_sha256']):
            raise RuntimeError('Runtime pinned data identity differs from the current checkout')
        for name in ('app/gimp-3.0', 'app/gimp-console-3.0'):
            if manifest['build_inputs'][name]['sha256'] != sha(args.build/name):
                raise RuntimeError('Runtime build identity differs: '+name)
        dependencies = {row['soname'] for rows in manifest['elf_dependency_graph'].values() for row in rows}
        legacy = sorted(name for name in dependencies if name.startswith((
            'libgtk-x11-2.0', 'libgdk-x11-2.0', 'libgimp-2.0', 'libgimpbase-2.0', 'libgegl-0.2', 'libgegl-0.3')))
        required = {'libgimp-3.0.so.0', 'libgtk-3.so.0', 'libgegl-0.4.so.0'}
        if legacy or not required <= dependencies:
            raise RuntimeError('Package must contain GIMP3/GTK3/GEGL0.4 and no legacy ABI')
        report['packaged_abi'] = dict(required=sorted(required), legacy_dependencies=legacy)
        report['old_blinds_provenance'] = verify_old_blinds()
        report['convolution_fixture_provenance'] = verify_convolution_inputs(args.convolution_inputs)
        report.update(source_commit=before['source_commit'],
                      build_manifest_sha256=sha(args.bundle/'build-manifest.json'),
                      source_runtime_manifest=original_manifest, relocated_bundle=str(relocated))
        baseline_observation = Observation(relocated)
        observations.append(baseline_observation)
        baseline = run_process([sys.executable, '-B', ROOT/'tools/test-linux-runtime.py',
                                args.bundle, '--output', args.output/'baseline'],
                               minimal_environment(args.output/'baseline-caller-profile'),
                               args.output/'baseline-run', baseline_observation, 1200)
        report['runs'].append(dict(name='baseline-runtime-and-blinds', **baseline))
        baseline_results = args.output/'baseline/results.json'
        if baseline['status'] != 'passed' or not baseline_results.is_file() or not json.loads(baseline_results.read_text())['all_passed']:
            raise RuntimeError('Existing installed runtime acceptance failed')
        report['baseline_results_sha256'] = sha(baseline_results)
        for route, fixture_root, choices, prefix in (
                ('small_tiles', args.small_tiles_fixtures, (0, 3), 'small-tiles'),
                ('retinex', args.retinex_fixtures, (0, 1), 'retinex')):
            checker = module('acceptance_'+route, 'tools/check_installed_'+route+'.py')
            verifier = module('acceptance_'+route+'_evidence', 'tools/check_'+route+'_evidence.py')
            evidence_output = args.output/(prefix+'-evidence')
            verifier.verify(extract=evidence_output)
            evidence = evidence_output/(prefix+'-evidence')
            for choice in choices:
                name = prefix+'-'+str(choice)
                observation = Observation(relocated)
                observations.append(observation)
                env = minimal_environment(args.output/(name+'-profile'))
                with observe_checker(checker, observation):
                    result = checker.run([relocated/'AppRun', '--console'], env,
                                         checker.installed_executables(relocated),
                                         fixture_root/(name+'.xcf'), evidence, args.output/name, choice)
                report['runs'].append(dict(name=name, status=result['status'], report=name+'/report.json',
                                           report_sha256=sha(args.output/name/'report.json')))
                if result['status'] != 'passed':
                    raise RuntimeError('Installed route failed: '+name)
        convolution = module('acceptance_convolution', 'tools/check_installed_convolution.py')
        cases = json.loads((args.convolution_inputs/'manifest.json').read_text())['cases']
        if (len(cases) != 2 or {c['format'] for c in cases} != {"R'G'B'A u8", "R'G'B'A double"}
                or len({c['id'] for c in cases}) != 2
                or any(not c['id'] or Path(c['id']).name != c['id'] or c['id'] in ('.', '..') for c in cases)):
            raise RuntimeError('Expected two distinct U8/Double Convolution synthetic fixture cases')
        for case in cases:
            case = {**case, 'bytes_per_pixel': {"R'G'B'A u8": 4, "R'G'B'A double": 32}[case['format']]}
            name = 'convolution-'+case['id']
            observation = Observation(relocated)
            observations.append(observation)
            with observe_checker(convolution, observation):
                result = convolution.run_case(relocated, args.convolution_inputs, case, args.output/name)
            report['runs'].append(dict(name=name, status=result['status'], report=name+'/report.json',
                                       report_sha256=sha(args.output/name/'report.json')))
            if result['status'] != 'passed':
                raise RuntimeError('Installed route failed: '+name)
        # One final replay of each whole driver, against this exact runtime.
        for name, plugins, markers, extra in (
                ('painter-filter-procedure', PLUGINS[:3], PROCEDURE_MARKERS,
                 [ROOT/'migration/fixtures/legacy-blinds/fixtures.tsv']),
                ('painter-filter-convolution', PLUGINS[3:], CONVOLUTION_MARKERS, [])):
            observation = Observation(relocated)
            observations.append(observation)
            report['corpus_invocations'] += 1
            result = run_process([sys.executable, '-B', ROOT/'migration/tests/run_filter_owner_context_fixture_test.py',
                                  '--', args.build/'app'/name, relocated/'usr/libexec/gimp-painter-filter-worker', *extra],
                                 corpus_environment(relocated, args.output/(name+'-profile')),
                                 args.output/name, observation, 1200, markers, plugins)
            report['runs'].append(dict(name=name, **result))
            if result['status'] != 'passed':
                raise RuntimeError('Relocated genuine-old corpus failed: '+name)
        report['old_pixel_comparisons'] = dict(blinds=320, small_tiles=196, retinex=174,
                                              convolution_final_merges=296, total_final_outputs=986,
                                              convolution_raw_roi=296, convolution_old_rejections=8)
    except Exception as error:
        report['errors'].append(type(error).__name__+': '+str(error))
    finally:
        process_reports = [value.report() for value in observations]
        report['process_observations'] = process_reports
        for value, observed in zip(observations, process_reports):
            for field in ('wrong_installation', 'bad_helper_protocol', 'survivors', 'leftover_profiles',
                          'unexpected_external_libraries'):
                if observed[field]:
                    report['errors'].append('Process observation failed: '+field)
            if observed['survivors']:
                value.cleanup()
        try:
            after = inputs_seal(args)
            write_json(args.output/'input-seal-after.json', after)
            report['sealed_inputs_unchanged'] = before == after
            report['input_seal_before_sha256'] = sha(args.output/'input-seal-before.json')
            report['input_seal_after_sha256'] = sha(args.output/'input-seal-after.json')
            report['source_runtime_unchanged'] = original_inventory == package.inventory(args.bundle)
            report['relocated_runtime_unchanged'] = relocated.exists() and original_inventory == package.inventory(relocated)
            report['final_source_runtime_manifest'] = verify_manifest(args.bundle)
            report['final_relocated_runtime_manifest'] = verify_manifest(relocated) if relocated.exists() else {'passed': False}
            if (not report['sealed_inputs_unchanged'] or not report['source_runtime_unchanged'] or
                    not report['relocated_runtime_unchanged']):
                report['errors'].append('Sealed source, fixture, build or original runtime changed')
            if not report['final_source_runtime_manifest']['passed'] or not report['final_relocated_runtime_manifest']['passed']:
                report['errors'].append('Final runtime manifest check failed')
        except Exception as error:
            report['errors'].append('Final input/runtime verification failed: '+type(error).__name__+': '+str(error))
        if report['corpus_invocations'] != 2 or len(report['runs']) != 9:
            report['errors'].append('Incomplete required runtime/corpus coverage')
        report['status'] = 'failed' if report['errors'] else 'passed'
        write_json(args.output/'report.json', report)
    print(json.dumps(dict(status=report['status'], errors=report['errors'], report=str(args.output/'report.json'))))
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
