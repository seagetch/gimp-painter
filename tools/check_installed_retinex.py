#!/usr/bin/env python3
"""Observe an installed Retinex route against sealed genuine-old live bytes.

Run under the shared build/test lock, after generating the native fixture with
filter-quit-fixture --retinex EVIDENCE_ROOT OUTPUT_DIRECTORY. --relocate
copies the supplied runtime to a fresh path containing spaces and Japanese
characters. The source bundle and earlier acceptance evidence stay untouched.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


observer = module('retinex_owned_processes', ROOT/'tools/check_filter_active_quit.py')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


BATCH = r'''
import hashlib
import json
import os
from pathlib import Path
import struct
import time
from gi.repository import Gegl, Gio, Gimp, GLib

config = json.loads(os.environ['GIMP_PAINTER_RETINEX_SMOKE'])
output = Path(config['output'])
events = output/'events.jsonl'
width, height = 53, 41

def emit(kind, **details):
    with events.open('a') as stream:
        stream.write(json.dumps(dict(event=kind, monotonic=time.monotonic(), **details))+'\n')

def wait_for_observer(name):
    deadline = time.monotonic()+30
    while not (output/name).exists():
        if time.monotonic() > deadline:
            raise RuntimeError('Retinex observer did not acknowledge '+name)
        time.sleep(0.01)

def record(path):
    data = path.read_bytes()
    assert data[:9] == b'gimp xcf ' and data[9:10] == b'v' and data[13:14] == b'\0'
    assert 11 <= int(data[10:13]) <= 23, 'Unexpected smoke XCF version'
    def integer(at, count=4):
        assert 0 <= at <= len(data)-count, 'Truncated XCF integer'
        return int.from_bytes(data[at:at+count], 'big')
    def string(at):
        count = integer(at)
        at += 4
        assert 0 < count <= 1048576 and at+count <= len(data)
        assert data[at+count-1] == 0
        return data[at:at+count-1], at+count
    def properties(at):
        values = []
        for _ in range(1024):
            tag, count = integer(at), integer(at+4)
            at += 8
            assert at+count <= len(data), 'Truncated XCF property'
            if tag == 0:
                assert count == 0
                return values, at
            values.append((tag, at, count))
            at += count
        raise AssertionError('Too many XCF properties')
    _, at = properties(30)
    capsules = []
    for _ in range(16):
        offset = integer(at, 8)
        at += 8
        if offset == 0:
            break
        name, layer_at = string(offset+12)
        if name != b'retinex Filter':
            continue
        props, _ = properties(layer_at)
        for tag, cursor, count in props:
            if tag != 21:
                continue
            end = cursor+count
            while cursor < end:
                name, cursor = string(cursor)
                count = integer(cursor+4)
                cursor += 8
                assert cursor+count <= end, 'Truncated XCF parasite'
                if name == b'gimp-painter-item':
                    capsules.append(data[cursor:cursor+count])
                cursor += count
    else:
        raise AssertionError('Too many XCF layers')
    assert len(capsules) == 1, 'Expected exactly one Retinex Filter capsule'
    assert capsules[0][:12] == b'GPXCF\0\0\0\1\0\0\0'
    value = GLib.Variant.new_from_bytes(GLib.VariantType.new('a{sv}'),
                                       GLib.Bytes.new(capsules[0][12:]), False)
    assert value.is_normal_form(), 'Malformed Filter capsule'
    value = value.unpack()
    assert value['version'] == 1 and value['kind'] == 'filter'
    return value

def definition(value):
    return {key:value[key] for key in
            ('procedure', 'has-definition', 'definition', 'has-arguments', 'arguments')}

def pixels(layer):
    return bytes(layer.get_buffer().get(Gegl.Rectangle.new(0, 0, width, height),
                                       1.0, "R'G'B'A u8", Gegl.AbyssPolicy.NONE))

def layers(image):
    assert image.get_width() == width and image.get_height() == height
    values = {layer.get_name():layer for layer in image.get_layers()}
    assert set(values) == {'retinex source', 'retinex Filter'}
    return values['retinex source'], values['retinex Filter']

def save(image, name):
    path = output/name
    assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, image, Gio.File.new_for_path(str(path)), None)
    return record(path)

def complete(image, effect, name, previous_generation, expected, initial_definition):
    deadline = time.monotonic()+90
    while True:
        state = save(image, name)
        if state['saved-state'] == 0 and state['cache-complete'] and \
                state['generation'] > previous_generation and \
                state['cache-generation'] == state['generation']:
            break
        assert state['saved-state'] != 6, 'Installed Retinex reached terminal failure'
        if time.monotonic() > deadline:
            raise RuntimeError('Installed Retinex did not complete the requested generation')
        time.sleep(0.05)
    assert pixels(effect) == expected, 'Retinex full raster differs from genuine-old live output'
    assert definition(state) == initial_definition, 'Execution changed typed Filter definition'
    return state

source_bytes = Path(config['source']).read_bytes()
settled = Path(config['settled']).read_bytes()
rerun = Path(config['rerun']).read_bytes()
assert len(source_bytes) == len(settled) == len(rerun) == width*height*4
assert settled != bytes((13, 29, 47, 255))*(width*height)
image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(config['fixture']))
assert image is not None
source, effect = layers(image)
assert pixels(source) == source_bytes, 'Native fixture lower source differs from old capture'
assert pixels(effect) == bytes((13, 29, 47, 255))*(width*height), 'Initial cache changed before explicit update'
initial = save(image, 'initial-cache.xcf')
assert initial['saved-state'] == 0 and initial['cache-complete']
assert initial['generation'] == initial['cache-generation']
assert bytes(initial['procedure']) == b'plug-in-retinex\0'
assert initial['has-arguments'] and initial['arguments'] == [
    ('gint', False, 1), ('gint', False, 0), ('gint', False, 0),
    ('gint', False, config['scale']), ('gint', False, config['nscales']),
    ('gint', False, config['mode']),
    ('gdouble', False, struct.unpack('<Q', struct.pack('<d', config['cvar']))[0])], initial['arguments']
initial_definition = definition(initial)
emit('READY', image=image.get_id(), generation=initial['generation'])
wait_for_observer('start-first')
assert source.update(0, 0, width, height)
emit('INVALIDATED', phase='first', image=image.get_id())
completed = complete(image, effect, 'saved.xcf', initial['generation'], settled, initial_definition)
emit('EXACT_OUTPUT', sha256=hashlib.sha256(settled).hexdigest(), generation=completed['generation'])
assert image.delete()
image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(str(output/'saved.xcf')))
assert image is not None
source, effect = layers(image)
assert pixels(source) == source_bytes and pixels(effect) == settled
reopened = save(image, 'reopened-definition.xcf')
assert definition(reopened) == initial_definition, 'Reopen changed typed Filter definition'
assert reopened['saved-state'] == 0 and reopened['cache-complete']
assert reopened['generation'] == reopened['cache-generation']
emit('REOPENED_READY', image=image.get_id(), generation=reopened['generation'])
wait_for_observer('start-rerun')
assert source.update(0, 0, width, height)
emit('INVALIDATED', phase='reopened-rerun', image=image.get_id())
repeated = complete(image, effect, 'rerun.xcf', reopened['generation'], rerun, initial_definition)
assert image.delete()
image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(str(output/'rerun.xcf')))
assert image is not None
_, effect = layers(image)
assert pixels(effect) == rerun, 'Second Save/reopen changed genuine-old rerun pixels'
restored = save(image, 'rerun-reopened-definition.xcf')
assert definition(restored) == initial_definition, 'Second reopen changed typed Filter definition'
assert restored['saved-state'] == 0 and restored['cache-complete']
assert restored['generation'] == restored['cache-generation']
assert image.delete()
result = dict(status='passed', variant=config['variant'], scene=config['scene'],
    expected_sha256=hashlib.sha256(settled).hexdigest(), rerun_sha256=hashlib.sha256(rerun).hexdigest(),
    definition_preserved=True, reopened_definition_preserved=True, argument_count=7,
    initial_generation=initial['generation'], first_generation=completed['generation'],
    reopened_generation=reopened['generation'], rerun_generation=repeated['generation'],
    scope='Two explicit lower updates, full genuine-old live Retinex bytes, two Save/reopen cycles and seven typed arguments, including binary64 cvar; no GUI or whole-port claim')
(output/'batch-result.json').write_text(json.dumps(result, indent=2)+'\n')
emit('SAVED_REOPENED', sha256=result['expected_sha256'], rerun_sha256=result['rerun_sha256'])
print('INSTALLED_RETINEX_EXACT_SAVE_REOPEN_OK', flush=True)
'''


def installed_executables(bundle):
    result = {}
    for name, pattern in (('helper', 'usr/**/gimp-painter-filter-worker'),
                          ('plugin', 'usr/**/plug-ins/contrast-retinex/contrast-retinex')):
        matches = [path for path in bundle.glob(pattern) if path.is_file()]
        if len(matches) != 1:
            raise RuntimeError('Expected exactly one installed '+name+' executable')
        path = matches[0].resolve()
        if not path.is_relative_to(bundle) or not os.access(path, os.X_OK):
            raise RuntimeError('Installed executable escapes runtime or is not executable: '+str(path))
        result[name] = path
    return result


def generate_fixture(build, evidence, output, variant):
    """The native producer saves a complete cache without running any job."""
    output.mkdir()
    generator = build/'app/tests/filter-quit-fixture'
    before = sha(generator)
    tests = json.loads((build/'meson-info/intro-tests.json').read_text())
    configuration = next(test['env'] for test in tests if test['name'] == 'script-fu-startup')
    env = os.environ.copy()
    for key, value in configuration.items():
        if key in ('LD_LIBRARY_PATH', 'GI_TYPELIB_PATH'):
            env[key] = value+os.pathsep+env.get(key, '')
        else:
            env[key] = value
    with tempfile.TemporaryDirectory(prefix='retinex-fixture-profile-') as temporary:
        env.update(GIMP3_DIRECTORY=temporary, GIMP3_DATADIR=temporary,
                   GIMP3_CACHEDIR=temporary, GIMP3_TEMPDIR=temporary,
                   GIMP_TESTING_PLUGINDIRS=temporary,
                   GIMP_TESTING_INTERPRETER_DIRS=temporary,
                   GIMP_TESTING_ENVIRON_DIRS=temporary)
        command = [str(generator), '--retinex', str(evidence), str(output)]
        with (output/'console.log').open('w') as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                                    text=True, timeout=90, check=False)
    contents = (output/'console.log').read_text(errors='replace')
    if result.returncode != 0 or 'RETINEX_FIXTURE_READY variant='+str(variant) not in contents:
        raise RuntimeError('Native Retinex fixture generation failed; inspect '+str(output/'console.log'))
    for marker in observer.SANITIZER_DIAGNOSTICS + ('Gimp-Core-CRITICAL',):
        if marker in contents:
            raise RuntimeError('Native Retinex fixture diagnostic: '+marker)
    if before != sha(generator):
        raise RuntimeError('Native fixture generator changed while running')
    fixture = output/('retinex-'+str(variant)+'.xcf')
    evidence_record = dict(command=command, exit_code=result.returncode,
        generator_sha256=before, source_sha256=sha(ROOT/'app/tests/test-filter-quit-fixture.cpp'),
        fixture_sha256=sha(fixture), no_filter_jobs=True)
    (output/'report.json').write_text(json.dumps(evidence_record, indent=2)+'\n')
    return fixture, evidence_record


def run(command, environment, executables, fixture, evidence, output, variant):
    """Also usable with a prefix console and its explicitly supplied environment."""
    if variant not in (0, 1):
        raise ValueError('This smoke has genuine-old fixtures only for variants 0 and 1')
    output.mkdir(parents=True, exist_ok=False)
    scene = 'retinex-g0-s0-v'+str(variant)
    raw = {name:evidence/'live'/(scene+'-'+name+'.raw') for name in ('source', 'settled', 'rerun')}
    for path in raw.values():
        if path.stat().st_size != 53*41*4:
            raise RuntimeError('Unexpected genuine-old Retinex raw extent: '+str(path))
    parameters = dict(scale=16, nscales=3, mode=0, cvar=1.2) if variant == 0 else \
                 dict(scale=256, nscales=8, mode=2, cvar=0.123456789)
    config = dict(output=str(output), fixture=str(fixture), variant=variant, scene=scene,
                  **parameters, **{name:str(path) for name,path in raw.items()})
    env = {**environment, 'GIMP_PAINTER_RETINEX_SMOKE':json.dumps(config)}
    arguments = [*command, '--new-instance', '--no-interface', '--no-data', '--no-fonts', '--no-splash',
                 '--batch-interpreter=python-fu-eval', '-b', '-', '--quit']
    expected = {name:str(Path(path).resolve()) for name,path in executables.items()}
    executable_hashes = {name:sha(path) for name,path in expected.items()}
    (output/'observer.executed.py').write_bytes(Path(__file__).read_bytes())
    (output/'batch.executed.py').write_text(BATCH)
    events = output/'events.jsonl'
    observed, profiles, errors, acknowledgements = {}, set(), [], set()
    helpers_after_update, plugins_after_update = set(), set()
    began = time.monotonic()
    with (output/'console.log').open('w') as log:
        process = subprocess.Popen([str(value) for value in arguments], env=env, stdin=subprocess.PIPE,
                                   stdout=log, stderr=subprocess.STDOUT, text=True,
                                   start_new_session=True, cwd=output)
        try:
            process.stdin.write(BATCH)
            process.stdin.close()
            while process.poll() is None:
                current = observer.descendants(process.pid)
                observed.update({(item['pid'], item['start_ticks']):item for item in current.values()})
                helpers = [item for item in current.values() if item['exe'] == expected['helper'] and item['state'] != 'Z']
                for item in helpers:
                    if len(item['argv']) == 3:
                        profiles.add(item['argv'][2])
                seen = observer.events(events)
                invalidated = any(item['event'] == 'INVALIDATED' for item in seen)
                if invalidated:
                    helper_ids = {item['pid'] for item in helpers}
                    helpers_after_update.update((item['pid'], item['start_ticks']) for item in helpers)
                    plugins_after_update.update((item['pid'], item['start_ticks']) for item in current.values()
                        if item['exe'] == expected['plugin'] and item['parent'] in helper_ids and
                           item['state'] != 'Z' and '-run' in item['argv'])
                for event, gate in (('READY', 'start-first'), ('REOPENED_READY', 'start-rerun')):
                    if event not in acknowledgements and any(item['event'] == event for item in seen):
                        if helpers:
                            errors.append('A helper was active before explicit lower update: '+event)
                        acknowledgements.add(event)
                        (output/gate).write_text('Initial saved cache and no live helper observed\n')
                if time.monotonic()-began > 240:
                    errors.append('Installed Retinex smoke exceeded 240 seconds')
                    os.killpg(process.pid, signal.SIGKILL)
                    break
                time.sleep(0.005)
            code = process.wait(timeout=5)
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
    contents = (output/'console.log').read_text(errors='replace')
    if code != 0 or 'INSTALLED_RETINEX_EXACT_SAVE_REOPEN_OK' not in contents:
        errors.append('Console did not confirm Retinex exact output and Save/reopen')
    for marker in observer.SANITIZER_DIAGNOSTICS + ('batch command experienced', 'Traceback (most recent call last)', 'Filter cleanup exceeded', 'Gimp-Core-CRITICAL'):
        if marker in contents:
            errors.append('Runtime diagnostic: '+marker)
    helpers = [item for item in observed.values() if item['exe'] == expected['helper']]
    helper_ids = {item['pid'] for item in helpers}
    plugins = [item for item in observed.values() if item['exe'] == expected['plugin'] and item['parent'] in helper_ids]
    if acknowledgements != {'READY', 'REOPENED_READY'}:
        errors.append('Missing both initial and reopened readiness handshakes')
    if len(helpers_after_update) < 2 or len(plugins_after_update) < 2:
        errors.append('Missing two actual installed helper/native contrast-retinex -run observations after invalidation')
    if any(len(item['argv']) != 3 or item['argv'][1] != '--filter-worker-v4' for item in helpers):
        errors.append('Installed helper did not use the GPF4 worker entry point')
    all_helper_ids = {item['pid'] for item in observed.values() if Path(item['exe']).name == 'gimp-painter-filter-worker'}
    wrong = [item for item in observed.values()
             if (Path(item['exe']).name == 'gimp-painter-filter-worker' and item['exe'] != expected['helper']) or
                (Path(item['exe']).name in ('tile-small', 'blinds', 'contrast-retinex') and
                 item['parent'] in all_helper_ids and item['exe'] != expected['plugin'])]
    if wrong:
        errors.append('A Filter executable resolved outside the intended Retinex runtime route')
    survivors = [item for item in observed.values() if observer.still_same(item)]
    leftovers = [path for path in sorted(profiles) if Path(path).exists()]
    if survivors or leftovers:
        errors.append('Observed process or private worker profile survived console exit')
    if errors:
        for item in survivors:
            if observer.still_same(item):
                try:
                    os.killpg(item['group'], signal.SIGKILL)
                except ProcessLookupError:
                    pass
    result = output/'batch-result.json'
    batch = json.loads(result.read_text()) if result.exists() else None
    if not batch or batch.get('status') != 'passed':
        errors.append('Missing successful batch result')
    elif (batch.get('expected_sha256') != sha(raw['settled']) or
          batch.get('rerun_sha256') != sha(raw['rerun']) or
          batch.get('definition_preserved') is not True or
          batch.get('reopened_definition_preserved') is not True or batch.get('argument_count') != 7):
        errors.append('Missing genuine-old output or typed-definition preservation evidence')
    if executable_hashes != {name:sha(path) for name,path in expected.items()}:
        errors.append('Installed executable bytes changed during smoke')
    report = dict(status='failed' if errors else 'passed', errors=errors,
        variant=variant, scene=scene, exit_code=code, seconds=time.monotonic()-began,
        command=[str(value) for value in arguments], expected_executables=expected,
        executable_sha256=executable_hashes, observed_helpers=helpers, observed_plugins=plugins,
        wrong_installation=wrong, survivors=survivors, leftover_profiles=leftovers,
        invalidated_helper_identities=sorted(helpers_after_update),
        invalidated_plugin_run_identities=sorted(plugins_after_update),
        events=observer.events(events), batch_result=batch, fixture_sha256=sha(fixture),
        genuine_old_raw_sha256={name:sha(path) for name,path in raw.items()},
        observer_source_sha256=sha(Path(__file__)),
        process_observer_source_sha256=sha(ROOT/'tools/check_filter_active_quit.py'),
        batch_source_sha256=sha(output/'batch.executed.py'), console_log_sha256=sha(output/'console.log'))
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    fixture_group = parser.add_mutually_exclusive_group(required=True)
    fixture_group.add_argument('--fixture', type=Path)
    fixture_group.add_argument('--build', type=Path, help='Generate a native fixture from this completed build')
    parser.add_argument('--variant', type=int, choices=(0, 1), default=1)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--relocate', action='store_true')
    args = parser.parse_args()
    bundle, output = args.bundle.resolve(), args.output.resolve()
    if output.exists():
        parser.error('Output must be new to preserve previous evidence')
    output.mkdir(parents=True)
    verifier = module('retinex_sealed_evidence', ROOT/'tools/check_retinex_evidence.py')
    manifest = verifier.verify(extract=output/'evidence')
    evidence = output/'evidence/retinex-evidence'
    if args.build:
        fixture, generated = generate_fixture(args.build.resolve(), evidence, output/'fixtures', args.variant)
    else:
        fixture, generated = args.fixture.resolve(), None
    if args.relocate:
        relocated = output/'relocated path 日本語'/bundle.name
        shutil.copytree(bundle, relocated, symlinks=True)
        bundle = relocated
    profile = output/'profile'
    for folder in ('home', 'config', 'cache', 'data'):
        (profile/folder).mkdir(parents=True)
    environment = {'PATH':'/usr/bin:/bin', 'LANG':'C.UTF-8', 'HOME':str(profile/'home'),
                   'XDG_CONFIG_HOME':str(profile/'config'), 'XDG_CACHE_HOME':str(profile/'cache'),
                   'XDG_DATA_HOME':str(profile/'data')}
    report = run([bundle/'AppRun', '--console'], environment, installed_executables(bundle),
                 fixture, evidence, output/'smoke', args.variant)
    report['native_fixture_generator'] = generated
    report['evidence_archive_sha256'] = sha(verifier.DEFAULT_ARCHIVE)
    report['evidence_manifest'] = manifest
    report['relocation'] = 'fresh path with spaces and Japanese characters' if args.relocate else 'supplied installed bundle'
    report['environment'] = 'explicit minimum whitelist; no inherited developer dependency variables'
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(status=report['status'], errors=report['errors'], report=str(output/'report.json'))))
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    sys.exit(main())
