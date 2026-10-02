#!/usr/bin/env python3
# SPDX-License-Identifier: ISC
"""Exercise the production Meson edge and C/C++ rebuilds in a temporary project."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--meson', default='meson')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    commands, output = [], []

    def run(*arguments, expected_success=True):
        command = shlex.split(args.meson) + list(arguments)
        commands.append(command)
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output.append(result.stdout)
        if (result.returncode == 0) != expected_success:
            raise RuntimeError(' '.join(command) + '\n' + result.stdout)
        return result.stdout

    with tempfile.TemporaryDirectory(prefix='painter-brush-meson-') as directory:
        source = Path(directory) / 'source'
        source.mkdir()
        shutil.copytree(ROOT, source / 'brush-settings', ignore=shutil.ignore_patterns('__pycache__'))
        (source / 'meson.build').write_text("project('painter-brush-generation-check', 'c', 'cpp', meson_version: '>=0.61.0', default_options: ['warning_level=3', 'werror=true'])\nsubdir('brush-settings')\n")
        build = Path(directory) / 'build'
        run('setup', str(build), str(source))
        run('compile', '-C', str(build))
        run('test', '-C', str(build), '--print-errorlogs')
        objects = sorted(build.rglob('*.o'))
        if len(objects) != 2:
            raise RuntimeError('expected the C and C++ probe objects; found ' + str(objects))
        before = {str(p.relative_to(build)): p.stat().st_mtime_ns for p in objects}
        run('compile', '-C', str(build))
        unchanged = {str(p.relative_to(build)): p.stat().st_mtime_ns for p in objects}
        if before != unchanged:
            raise RuntimeError('no-op build unexpectedly rebuilt consumers')
        manifest = source / 'brush-settings/brush-settings.json'
        data = json.loads(manifest.read_text())
        data['settings'][0]['default_value'] = '0.5'
        manifest.write_text(json.dumps(data, indent=2) + '\n')
        run('compile', '-C', str(build))
        after = {str(p.relative_to(build)): p.stat().st_mtime_ns for p in objects}
        if not all(after[name] > before[name] for name in before):
            raise RuntimeError('manifest edit failed to rebuild both C and C++ consumers')
        changed_header = (build / 'brush-settings/mypaintbrush-settings-data.h').read_text()
        if '"opaque", BRUSH_OPAQUE, "Opacity", 0, 0.0, 0.5, 2.0' not in changed_header:
            raise RuntimeError('manifest change did not reach generated metadata')
        # The compatibility comparison must now fail. This is an intentional
        # mutation only in the disposable test copy, never the actual source.
        run('test', '-C', str(build), '--print-errorlogs', 'painter-brush-metadata-c', 'painter-brush-metadata-cpp')
        run('test', '-C', str(build), '--print-errorlogs', 'painter-brush-generator', expected_success=False)
        report = {'status': 'PASS', 'scope': 'Isolated production Meson edge, C11/C++14 consumers and manifest-change rebuild',
                  'mutation_compatibility_test_failed_as_expected': True,
                  'noop_build_rebuilt_objects': False, 'manifest_edit_rebuilt_both_consumers': True,
                  'objects_before': before, 'objects_after': after,
                  'commands': commands, 'output': output,
                  'source_sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                    for path in sorted(ROOT.rglob('*')) if path.is_file() and '__pycache__' not in path.parts}}
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2) + '\n')
        print('PASS: production generation edge, pinned tests, no-op build, and both C/C++ rebuilds')


if __name__ == '__main__':
    main()
