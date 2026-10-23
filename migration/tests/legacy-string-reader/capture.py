#!/usr/bin/env python3
"""Execute unchanged pinned legacy string writer/reader through observer wrappers.

Requires tools/build_legacy_reference.sh to have completed first. The installed
reference is never modified. This is a parser observation, not full-file Open.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

PIN = 'afa43fae3e920210146abed514f136fd49f671b5'
HERE = Path(__file__).resolve().parent
FILES = ['app/xcf/xcf-save.c', 'app/xcf/xcf-load.c', 'app/tests/test-xcf.c']


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    source, build, output = (p.resolve() for p in (args.source, args.build, args.output))
    if output.exists():
        parser.error('use a new output directory; never overwrite capture evidence')
    assert subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip() == PIN
    original = {}
    for name in FILES:
        data = subprocess.check_output(['git', '-C', str(source), 'show', PIN + ':' + name])
        assert (source / name).read_bytes() == data, 'capture requires uninstrumented file: ' + name
        original[name] = hashlib.sha256(data).hexdigest()
    reference = build / 'prefix/bin/gimp-2.8'
    reference_hash = sha(reference)
    output.mkdir(parents=True)
    (output / 'fixtures').mkdir()
    subprocess.run(['git', '-C', str(source), 'apply', '--check', str(HERE / 'capture.patch')], check=True)
    subprocess.run(['git', '-C', str(source), 'apply', str(HERE / 'capture.patch')], check=True)
    # The exported wrappers are appended: every original reader/writer byte is
    # preserved, not a copied or simplified reimplementation of the old parser.
    for name in FILES[:2]:
        before = subprocess.check_output(['git', '-C', str(source), 'show', PIN + ':' + name])
        assert (source / name).read_bytes().startswith(before)
    commands = [
        ('build-xcf', 'make -C app/xcf -j2'),
        ('build-observer', "make -C app/tests -j2 test-xcf 'LIBS=../gimp-features.o ../presets/libapppresets.a -ljson-glib-1.0 -lstdc++ -lm'"),
        ('capture', 'GIMP_PAINTER_STRING_CAPTURE=' + shlex.quote(str(output / 'fixtures')) +
         ' timeout 30 app/tests/test-xcf'),
    ]
    runs = []
    for name, command in commands:
        command = 'set -e; source ' + shlex.quote(str(build / 'env.sh')) + '; ' + command
        (output / (name + '.command')).write_text(command + '\n')
        with (output / (name + '.log')).open('w') as log:
            result = subprocess.run(['bash', '-c', command], cwd=source, stdout=log, stderr=subprocess.STDOUT)
        (output / (name + '.exit')).write_text(str(result.returncode) + '\n')
        runs.append({'name': name, 'exit': result.returncode})
        if result.returncode:
            raise RuntimeError('capture step failed; preserve ' + str(output / (name + '.log')))
    assert sha(reference) == reference_hash, 'installed pristine reference changed'
    manifest = {
        'source_commit': PIN, 'original_source_sha256': original,
        'observer_source_sha256': {name: sha(source / name) for name in FILES},
        'patch_sha256': sha(HERE / 'capture.patch'),
        'installed_pristine_binary_sha256': reference_hash,
        'observer_binary_sha256': sha(source / 'app/tests/.libs/test-xcf'),
        'runs': runs,
        'files': {str(p.relative_to(output)): sha(p) for p in sorted(output.rglob('*')) if p.is_file()},
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Original writer/reader capture complete; no full-XCF application compatibility claim.')


if __name__ == '__main__':
    main()
