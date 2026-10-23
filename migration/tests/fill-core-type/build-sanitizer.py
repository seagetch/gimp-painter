#!/usr/bin/env python3
"""Build the original08.011 focused Fill core sanitizer executable, never run it.

Source env-http.sh first. Uses existing compile/link commands without invoking a
normal build or manifest regeneration. Every private object is rebuilt afresh;
all production archive aliases are replaced, including link_whole aliases.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import fcntl
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

BASE = Path('/workspace/scratch/5b5281e79681')
ROOT = BASE / 'gtk-binding-recovery-20261010/worktree'
BUILD = BASE / 'gimp-native-restore-20261010/build-http'
RECOVERY = Path(__file__).resolve().parent
PRIVATE = RECOVERY / 'sanitizer-private'
NINJA = BASE / 'gimp-native-restore-20261010/http.ninja'
LOCK = Path('/workspace/shared/gimp-painter-build.lock')
REPORT = RECOVERY / 'sanitizer-build.json'
TARGET = 'app/tests/gimp-fill-brush'
FLAGS = ['-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-O1']
CXX = {'.cpp', '.cc', '.cxx', '.c++', '.cp', '.C'}
INSTRUMENTED = {
    'app/painter/binding-store.cpp',
    'app/painter/gimp-painter-binding.cpp',
    'app/painter/gimp-painter-error.cpp',
    'app/paint/gimpfillbrush.cpp',
    'app/paint/painter-bounded-fill/search.cpp',
    'app/paint/gimppaintcore.c',
    'app/paint/gimpbrushcore.c',
    'app/paint/gimppaintcore-loops.cc',
    'app/paint/gimpbrushcore-loops.cc',
    'app/paint/gimppaintcore-stroke.c',
    'app/paint/gimppaintcoreundo.c',
    'app/paint/gimppaintoptions.c',
    'app/paint/gimppainterpaintgate.cpp',
    'app/paint/gimppainterpaper-paste.cpp',
    'app/paint/gimppainterpaper.cpp',
    'app/paint/painter-mypaint-surface/gimp-painter-options.cpp',
    'app/paint/painter-mypaint-surface/gimp-painter-session.cpp',
    'app/core/gimpimage.c',
    'app/core/gimpitem.c',
    'app/core/gimpdrawable.c',
    'app/core/gimpviewable.c',
    'app/core/gimpcontext.c',
    'app/core/gimpimage-undo.c',
    'app/core/gimpimage-undo-push.c',
    'app/core/gimpundo.c',
    'app/core/gimpundostack.c',
    'app/core/gimpitemundo.c',
    'app/core/gimpdrawableundo.c',
    'app/core/gimpdrawable-stroke.c',
    'app/operations/layer-modes-legacy/gimpoperationpainterlegacy.c',
    'app/operations/layer-modes/gimp-layer-modes.c',
    'app/tests/test-gimp-fill-brush.cpp',
}
CASES = ['registered-type-parent-contract', 'parent-start-failure-retry',
         'standalone-parent-lifetime', 'parent-start-notification-lifetime']


def sha(path):
    path = Path(path)
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def require(value, message):
    if not value:
        raise RuntimeError(message)


def arguments(entry):
    return entry.get('arguments', []) or shlex.split(entry['command'])


def clean_compile(entry, output):
    cleaned, skip = [], False
    for arg in arguments(entry):
        if skip:
            skip = False
            continue
        if arg in ('-MF', '-MQ', '-MT'):
            skip = True
            continue
        if arg not in ('-MD', '-MMD'):
            cleaned.append(arg)
    cleaned[cleaned.index('-o') + 1] = str(output)
    return cleaned


def header_seals():
    # Deliberately broader than the direct include lists: seal every checkout
    # header/template/include fragment, plus every generated build header.
    result = {}
    for base in (ROOT, BUILD):
        for path in sorted(base.rglob('*')):
            if path.suffix not in ('.h', '.hpp', '.hxx', '.inc', '.inl', '.tpp'):
                continue
            if '.git' in path.relative_to(base).parts or not path.is_file():
                continue
            result[str(path.resolve())] = sha(path)
    return result


def save(report):
    REPORT.write_text(json.dumps(report, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    require(1 <= args.jobs <= 4, '--jobs must be between 1 and 4')
    with LOCK.open('a') as lock:
        fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
        build(args.jobs)


def build(jobs):
    PRIVATE.mkdir(exist_ok=True)
    entries = json.loads((BUILD / 'compile_commands.json').read_text())
    rtti_sources = set()
    for entry in entries:
        source = (Path(entry['directory']) / entry['file']).resolve()
        if source.suffix not in CXX or not source.is_relative_to(ROOT):
            continue
        relative = source.relative_to(ROOT)
        if relative.parts[0] == 'app' and 'tests' not in relative.parts and '-fno-rtti' in arguments(entry):
            rtti_sources.add(relative.as_posix())
    rtti_only = rtti_sources - INSTRUMENTED
    wanted = INSTRUMENTED | rtti_only
    selected = {}
    for source in sorted(wanted):
        candidates = [entry for entry in entries
                      if (Path(entry['directory']) / entry['file']).resolve() == ROOT / source]
        require(candidates, 'No compile command for ' + source)
        selected[source] = next((entry for entry in candidates
                                if entry['output'].startswith(str(Path(source).parent) + '/')), candidates[0])

    link = shlex.split(subprocess.check_output(
        ['ninja', '-f', str(NINJA), '-t', 'commands', TARGET], cwd=BUILD, text=True).strip().splitlines()[-1])
    archive_members = {}
    normal_paths = {(BUILD / e['output']).resolve() for e in selected.values()}
    normal_paths.add(BUILD / TARGET)
    for arg in link:
        if arg.startswith('-'):
            continue
        path = (BUILD / arg).resolve()
        if path.is_relative_to(BUILD) and path.is_file():
            normal_paths.add(path)
        if arg.endswith('.a') and path.is_relative_to(BUILD) and arg not in archive_members:
            members = subprocess.check_output(['ar', 't', arg], cwd=BUILD, text=True).splitlines()
            archive_members[arg] = [str((BUILD / member).resolve()) for member in members]
            normal_paths.update(Path(member) for member in archive_members[arg])

    sources = {source: sha(ROOT / source) for source in sorted(wanted)}
    headers = header_seals()
    manifests = {str(path): sha(path) for path in (NINJA, BUILD / 'compile_commands.json', Path(__file__).resolve())}
    original_hashes = {str(path): sha(path) for path in sorted(normal_paths)}
    require(all(sources.values()), 'A source is missing')
    report = {
        'build_status': 'building', 'run_status': 'Not run. Build-only evidence; parent owns all execution.',
        'work_unit': 'original08.011 Fill brush core registered type acceptance',
        'scope': 'Focused Fill core, BindingStore/boundaries/lifetimes, native PaintCore/BrushCore, '
                 'stroke callers, paper paste/paint gate, drawable/image/context/viewable ownership '
                 'and undo chain, bounded fill and legacy compositor, and test harness instrumented. '
                 'Listed RTTI compatibility sources rebuilt with -frtti only. '
                 'Remaining host and dependencies uninstrumented; no full application/platform '
                 'or renderer acceptance and no LeakSanitizer claim.',
        'sanitizers': ['address', 'undefined', 'float-cast-overflow'], 'leak_detection': False,
        'instrumented_cpp_rtti': True, 'fresh_private_build': True, 'private_object_reuse': False,
        'instrumented_sources': sorted(INSTRUMENTED),
        'rtti_compatibility_only_sources': sorted(rtti_only),
        'sources': sorted(wanted), 'sources_sha256': sources,
        'source_and_generated_headers_sha256': headers,
        'header_seal_scope': 'All checkout and generated build .h/.hpp/.hxx/.inc/.inl/.tpp files before/after build',
        'manifest_sha256': manifests, 'lock': str(LOCK), 'lock_held_for_entire_build': True,
        'ninja_file': str(NINJA), 'normal_target': TARGET,
        'new_type_cases': CASES, 'jobs': jobs,
        'commands': [], 'objects': {}, 'private_archives': {},
        'executables': [], 'executables_sha256': {},
        'production_artifacts_sha256_before': original_hashes,
    }
    save(report)
    compiles, replacements = [], {}
    try:
        for source, entry in selected.items():
            obj = PRIVATE / (source.replace('/', '_') + '.o')
            command = clean_compile(entry, obj)
            if source in INSTRUMENTED:
                command += FLAGS
            if Path(source).suffix in CXX:
                command += ['-frtti']
            depfile = obj.with_suffix('.d')
            command += ['-MD', '-MF', str(depfile), '-MT', str(obj)]
            replacements[str((BUILD / entry['output']).resolve())] = str(obj)
            compiles.append((source, command))
            report['commands'].append(command)
            report['objects'][source] = {'path': str(obj), 'action': 'fresh_compile',
                'instrumented': source in INSTRUMENTED, 'compile_command': command,
                'normal_object': str((BUILD / entry['output']).resolve()), 'dependency_file': str(depfile)}
        save(report)

        def compile_one(pair):
            source, command = pair
            kind = 'ASan/UBSan' if source in INSTRUMENTED else 'RTTI only'
            print('Compiling ' + source + ' (' + kind + ')', flush=True)
            log = PRIVATE / (source.replace('/', '_') + '.compile.log')
            with log.open('w') as stream:
                result = subprocess.run(command, cwd=BUILD, stdout=stream, stderr=subprocess.STDOUT)
            if result.returncode:
                print(log.read_text(), flush=True)
                raise RuntimeError('Compile failed for ' + source + '; see ' + str(log))
            return source

        with ThreadPoolExecutor(max_workers=jobs) as pool:
            for source in pool.map(compile_one, compiles):
                report['objects'][source]['sha256'] = sha(report['objects'][source]['path'])
        save(report)
        archives = {}
        for archive, members in sorted(archive_members.items()):
            if not any(member in replacements for member in members):
                continue
            private = PRIVATE / archive.replace('/', '_')
            private.unlink(missing_ok=True)
            command = ['ar', 'crsT', str(private)] + [replacements.get(member, member) for member in members]
            report['commands'].append(command)
            subprocess.run(command, cwd=BUILD, check=True)
            archives[archive] = str(private)
            report['private_archives'][archive] = {'path': str(private), 'sha256': sha(private),
                'replaced_members': {member: replacements[member] for member in members if member in replacements}}
        exe = PRIVATE / 'gimp-fill-brush-asan'
        link[link.index('-o') + 1] = str(exe)
        link = [archives.get(arg, replacements.get(str((BUILD / arg).resolve()), arg))
                if not arg.startswith('-') else arg for arg in link]
        link[1:1] = FLAGS
        report['commands'].append(link)
        save(report)
        print('Linking ' + str(exe), flush=True)
        subprocess.run(link, cwd=BUILD, check=True)
        report['executables'] = [str(exe)]
        report['executables_sha256'] = {str(exe): sha(exe)}
        dynamic = subprocess.check_output(['readelf', '-d', str(exe)], text=True)
        (RECOVERY / 'sanitizer-binary-dynamic.txt').write_text(dynamic)
        require('libasan.so' in dynamic and 'libubsan.so' in dynamic, 'Executable lacks sanitizer runtimes')
        report['runtime_dependencies_verified'] = ['libasan.so', 'libubsan.so']
        report['build_status'] = 'passed'
    except Exception as error:
        report['build_status'] = 'failed'
        report['build_error'] = str(error)
        raise
    finally:
        report['changed_sources_during_build'] = [source for source, digest in sources.items()
                                                   if sha(ROOT / source) != digest]
        headers_after = header_seals()
        report['changed_headers_during_build'] = sorted(path for path in headers.keys() | headers_after.keys()
                                                       if headers.get(path) != headers_after.get(path))
        report['changed_manifests_during_build'] = sorted(path for path, digest in manifests.items() if sha(path) != digest)
        after = {path: sha(path) for path in original_hashes}
        report['production_artifacts_sha256_after'] = after
        report['production_artifacts_changed'] = sorted(path for path in original_hashes if after[path] != original_hashes[path])
        report['production_artifacts_unchanged'] = bool(original_hashes) and not report['production_artifacts_changed']
        report['normal_artifact_count'] = len(original_hashes)
        for source, info in report['objects'].items():
            info['sha256'] = sha(info['path'])
            info['dependency_file_sha256'] = sha(info['dependency_file'])
        if (report['changed_sources_during_build'] or report['changed_headers_during_build']
                or report['changed_manifests_during_build'] or report['production_artifacts_changed']):
            report['build_status'] = 'failed'
            report['build_error'] = 'Source/header/manifest or normal production artifact changed during private build'
        save(report)
    require(report['build_status'] == 'passed', report.get('build_error', 'Build failed'))
    print(json.dumps({'build_status': report['build_status'],
                      'instrumented_source_count': len(INSTRUMENTED),
                      'rtti_only_source_count': len(rtti_only),
                      'production_artifacts_unchanged': report['production_artifacts_unchanged'],
                      'normal_artifact_count': len(original_hashes),
                      'executables_sha256': report['executables_sha256'],
                      'report': str(REPORT)}, indent=2), flush=True)


if __name__ == '__main__':
    main()
