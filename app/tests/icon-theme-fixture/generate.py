#!/usr/bin/env python3
"""Copy reviewed production icon bytes to the existing build-tree GTK fixture."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import sys
import tempfile

OWNER_FILE = '.fixture-owner.json'
OWNER_BYTES = b'{"format":1,"scope":"production-vector-icon-test-fixture"}\n'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def output_path(value):
    path = PurePosixPath(value)
    if (not value or value in ('manifest.json', OWNER_FILE) or path.is_absolute() or value != path.as_posix() or
            any(part in ('.', '..') for part in path.parts) or
            any(char in value for char in ('\\', ':', '\t', '\n', '\r'))):
        raise ValueError('unsafe fixture output path: ' + value)
    return path


def source_file(root, value):
    if not value or Path(value).is_absolute():
        raise ValueError('source must be relative to the source root')
    path = (root / value).resolve()
    try:
        path.relative_to(root)
    except ValueError:
        raise ValueError('fixture source escapes the source root: ' + value)
    if not path.is_file():
        raise ValueError('missing fixture input: ' + value)
    return path


def read_plan(root, plan):
    root = root.resolve()
    data = plan.read_bytes()
    lines = data.decode('utf-8').splitlines()
    if not lines or lines[0] != 'mode\tvector':
        raise ValueError('only the reviewed vector production recipe is supported')
    inputs, copies = {}, {}
    for line in lines[1:]:
        fields = line.split('\t')
        if len(fields) == 2 and fields[0] == 'input':
            path = source_file(root, fields[1])
            inputs[path.relative_to(root).as_posix()] = sha(path.read_bytes())
        elif len(fields) == 3 and fields[0] == 'file':
            path = source_file(root, fields[1])
            destination = output_path(fields[2]).as_posix()
            if destination in copies and copies[destination] != path:
                raise ValueError('conflicting fixture destination: ' + destination)
            copies[destination] = path
        else:
            raise ValueError('invalid fixture plan line: ' + line)
    if not copies:
        raise ValueError('empty fixture asset plan')
    entries = [{'source': path.relative_to(root).as_posix(),
                'destination': destination, 'sha256': sha(path.read_bytes()),
                'bytes': path.stat().st_size}
               for destination, path in sorted(copies.items())]
    return {'format': 1, 'scope': 'production-vector-icon-test-fixture',
            'plan_sha256': sha(data), 'recipe_input_sha256': inputs,
            'files': entries}, copies


def json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True) + '\n').encode('utf-8')


def stamp_bytes(manifest):
    return ('/* Generated build-tree test fixture readiness; no runtime API. */\n'
            '#ifndef GIMP_TEST_ICON_THEME_READY_H\n'
            '#define GIMP_TEST_ICON_THEME_READY_H\n'
            '#define GIMP_TEST_ICON_THEME_SHA256 "' + sha(json_bytes(manifest)) + '"\n'
            '#endif\n').encode('ascii')


def reject_symlink_components(path):
    for part in [path] + list(path.parents):
        if part.is_symlink():
            raise ValueError('fixture output may not traverse symlinks: ' + str(part))


def write_changed(path, data):
    reject_symlink_components(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def canonical_output(path):
    # Allow an ordinary platform parent alias such as macOS /var -> /private/var,
    # but never follow a symlink used as the owned fixture directory or stamp.
    if path.is_symlink():
        raise ValueError('fixture output may not be a symlink: ' + str(path))
    return path.parent.resolve() / path.name


def establish_owner(output):
    if output.exists():
        marker = output / OWNER_FILE
        if not marker.is_file() or marker.read_bytes() != OWNER_BYTES:
            raise ValueError('refusing an existing unowned fixture directory')
        reject_symlink_components(marker)
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix='.' + output.name + '-owner-',
                                    dir=output.parent))
    try:
        (staging / OWNER_FILE).write_bytes(OWNER_BYTES)
        if output.exists():
            raise ValueError('fixture directory appeared during ownership setup')
        # Publish ownership before copying any assets. Interrupted first copies
        # are resumable, while arbitrary pre-existing directories stay rejected.
        staging.rename(output)
    finally:
        if staging.exists():
            shutil.rmtree(staging)


def generate(root, plan, output, stamp):
    output, stamp = canonical_output(output), canonical_output(stamp)
    manifest, copies = read_plan(root, plan)
    reject_symlink_components(output)
    establish_owner(output)
    old = None
    if (output / 'manifest.json').is_file():
        try:
            old = json.loads((output / 'manifest.json').read_text())
        except json.JSONDecodeError:
            # An interrupted manifest write is incomplete, not proof of a
            # complete fixture. The ownership marker permits reconstruction;
            # final verification still rejects unknown files or wrong bytes.
            old = None
        if old:
            if old.get('format') != 1 or old.get('scope') != manifest['scope']:
                raise ValueError('unrecognized existing fixture manifest')
            for entry in old['files']:
                output_path(entry['destination'])
    for entry in manifest['files']:
        path = copies[entry['destination']]
        data = path.read_bytes()
        if sha(data) != entry['sha256']:
            raise ValueError('source changed while copying: ' + str(path))
        write_changed(output / entry['destination'], data)
    if old:
        for entry in old['files']:
            if entry['destination'] not in copies:
                path = output / entry['destination']
                reject_symlink_components(path)
                if path.is_file():
                    path.unlink()
    write_changed(output / 'manifest.json', json_bytes(manifest))
    write_changed(stamp, stamp_bytes(manifest))
    return verify(root, plan, output, stamp)


def verify(root, plan, output, stamp):
    output, stamp = canonical_output(output), canonical_output(stamp)
    manifest, _ = read_plan(root, plan)
    reject_symlink_components(output)
    marker = output / OWNER_FILE
    reject_symlink_components(marker)
    if marker.read_bytes() != OWNER_BYTES:
        raise ValueError('fixture ownership marker does not match')
    if (output / 'manifest.json').read_bytes() != json_bytes(manifest):
        raise ValueError('fixture manifest does not match current production inputs')
    if stamp.read_bytes() != stamp_bytes(manifest):
        raise ValueError('fixture readiness header does not match the manifest')
    for entry in manifest['files']:
        path = output / entry['destination']
        reject_symlink_components(path)
        if not path.is_file() or sha(path.read_bytes()) != entry['sha256']:
            raise ValueError('copied fixture asset differs: ' + entry['destination'])
    known = {entry['destination'] for entry in manifest['files']} | {'manifest.json', OWNER_FILE}
    actual = {path.relative_to(output).as_posix() for path in output.rglob('*') if path.is_file()}
    if actual != known:
        raise ValueError('unexpected fixture files: ' + str(sorted(actual - known)))
    return {'status': 'verified', 'files': len(manifest['files']),
            'ownership_marker_sha256': sha(OWNER_BYTES),
            'manifest_sha256': sha(json_bytes(manifest)),
            'stamp_sha256': sha(stamp.read_bytes()), 'plan_sha256': manifest['plan_sha256']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--plan', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--stamp', type=Path, required=True)
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    function = verify if args.verify else generate
    try:
        result = function(args.source_root, args.plan, args.output, args.stamp)
    except (ValueError, OSError, KeyError, TypeError) as error:
        print(str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == '__main__':
    sys.exit(main())
