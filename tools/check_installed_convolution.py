#!/usr/bin/env python3
"""Verify two relocated Convolution executions and typed Save/reopen cycles.

Inputs are synthetic XCF/raw fixtures exported by the native test. Run under
the shared build/test lock against a completed prototype runtime. This is a
Linux console/process test, not a desktop or whole-release acceptance claim.
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
import time

ROOT = Path(__file__).resolve().parents[1]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


observer = module('convolution_process_observer', ROOT/'tools/check_filter_active_quit.py')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


BATCH = r'''
import hashlib, json, os, struct, time
from pathlib import Path
from gi.repository import Gegl, Gio, Gimp, GLib
c = json.loads(os.environ['GIMP_PAINTER_CONVOLUTION_SMOKE'])
out = Path(c['output'])
width, height = c['width'], c['height']

def emit(kind, **details):
    with (out/'events.jsonl').open('a') as f:
        f.write(json.dumps(dict(event=kind, monotonic=time.monotonic(), **details))+'\n')

def gate(name):
    end = time.monotonic()+30
    while not (out/name).exists():
        if time.monotonic() > end: raise RuntimeError('Observer handshake expired: '+name)
        time.sleep(0.01)

def capsule(path):
    data = path.read_bytes()
    assert data[:10] == b'gimp xcf v' and data[13] == 0
    assert 11 <= int(data[10:13]) <= 23
    def integer(at, size=4):
        assert 0 <= at <= len(data)-size
        return int.from_bytes(data[at:at+size], 'big')
    def string(at):
        size = integer(at); at += 4
        assert 0 < size <= 1048576 and at+size <= len(data) and data[at+size-1] == 0
        return data[at:at+size-1], at+size
    def props(at):
        result = []
        for _ in range(1024):
            tag, size = integer(at), integer(at+4); at += 8
            assert at+size <= len(data)
            if tag == 0:
                assert size == 0
                return result, at
            result.append((tag, at, size)); at += size
        raise AssertionError('Too many XCF properties')
    _, at = props(30)
    found = []
    for _ in range(16):
        layer = integer(at, 8); at += 8
        if layer == 0: break
        name, offset = string(layer+12)
        if name != c['filter_name'].encode(): continue
        values, _ = props(offset)
        for tag, cursor, size in values:
            if tag != 21: continue
            end = cursor+size
            while cursor < end:
                name, cursor = string(cursor)
                size = integer(cursor+4); cursor += 8
                assert cursor+size <= end
                if name == b'gimp-painter-item': found.append(data[cursor:cursor+size])
                cursor += size
    else: raise AssertionError('Too many XCF layers')
    assert len(found) == 1 and found[0][:12] == b'GPXCF\0\0\0\1\0\0\0'
    value = GLib.Variant.new_from_bytes(GLib.VariantType.new('a{sv}'), GLib.Bytes.new(found[0][12:]), False)
    assert value.is_normal_form()
    value = value.unpack()
    assert value['version'] == 1 and value['kind'] == 'filter'
    return value

def definition(value):
    return {k:value[k] for k in ('procedure','has-definition','definition','has-arguments','arguments')}

def pixels(layer):
    return bytes(layer.get_buffer().get(Gegl.Rectangle.new(0,0,width,height), 1.0,
                                       c['format'], Gegl.AbyssPolicy.NONE))

def layers(image):
    assert image is not None and image.get_width() == width and image.get_height() == height
    values = {v.get_name():v for v in image.get_layers()}
    assert set(values) == {c['source_name'],c['filter_name']}
    return values[c['source_name']], values[c['filter_name']]

def save(image, name):
    path = out/name
    assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, image, Gio.File.new_for_path(str(path)), None)
    return capsule(path)

def ready(state):
    return state['saved-state'] == 0 and state['cache-complete'] and state['generation'] == state['cache-generation']

def complete(image, effect, name, previous):
    deadline = time.monotonic()+90
    while True:
        state = save(image,name)
        if ready(state) and state['generation'] > previous: break
        assert state['saved-state'] != 6, 'Convolution reached FAILED'
        if time.monotonic() > deadline: raise RuntimeError('Convolution completion timed out')
        time.sleep(0.05)
    assert pixels(effect) == expected, 'Installed completed pixels differ from sealed expectation'
    assert definition(state) == original, 'Typed definition changed during execution'
    return state

source_bytes = Path(c['source']).read_bytes()
initial_bytes = Path(c['initial']).read_bytes()
expected = Path(c['expected']).read_bytes()
assert len(source_bytes) == len(initial_bytes) == len(expected) == width*height*c['bytes_per_pixel']
image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,Gio.File.new_for_path(c['fixture']))
source, effect = layers(image)
assert pixels(source) == source_bytes and pixels(effect) == initial_bytes
initial = save(image,'initial.xcf')
assert ready(initial) and bytes(initial['procedure']) == b'plug-in-convmatrix\0'
assert initial['has-arguments'] and len(initial['arguments']) == 11
assert initial['arguments'][4][0] == 'GimpDoubleArray' and initial['arguments'][9][0] == 'GimpInt32Array'
assert initial['arguments'][3] == ('gint',False,25) and initial['arguments'][8] == ('gint',False,5)
assert initial['arguments'][4][2][0] and len(initial['arguments'][4][2][1]) == 25*8
assert initial['arguments'][9][2][0] and len(initial['arguments'][9][2][1]) == 5*4
assert bytes(initial['arguments'][4][2][1]) == struct.pack('<25d',*c['matrix'])
assert bytes(initial['arguments'][9][2][1]) == struct.pack('<5i',*c['channels'])
original = definition(initial)
emit('READY', generation=initial['generation']); gate('start-first')
assert source.update(0,0,width,height)
emit('INVALIDATED', phase='first')
first = complete(image,effect,'first.xcf',initial['generation'])
assert image.delete()
image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,Gio.File.new_for_path(str(out/'first.xcf')))
source, effect = layers(image)
assert pixels(source) == source_bytes and pixels(effect) == expected
reopened = save(image,'reopened.xcf')
assert ready(reopened) and definition(reopened) == original
emit('REOPENED_READY',generation=reopened['generation']); gate('start-rerun')
assert source.update(0,0,width,height)
emit('INVALIDATED',phase='reopened-rerun')
second = complete(image,effect,'second.xcf',reopened['generation'])
assert image.delete()
image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,Gio.File.new_for_path(str(out/'second.xcf')))
_, effect = layers(image)
assert pixels(effect) == expected
final = save(image,'second-reopened.xcf')
assert ready(final) and definition(final) == original
assert image.delete()
result = dict(status='passed',case=c['id'],format=c['format'],argument_count=11,
              matrix_type='GimpDoubleArray',channels_type='GimpInt32Array',
              expected_sha256=hashlib.sha256(expected).hexdigest(),
              initial_generation=initial['generation'],first_generation=first['generation'],
              reopened_generation=reopened['generation'],second_generation=second['generation'],
              exact_pixels=True,typed_definition_preserved=True,save_reopen_cycles=2)
(out/'batch-result.json').write_text(json.dumps(result,indent=2)+'\n')
emit('DONE')
print('INSTALLED_CONVOLUTION_EXACT_SAVE_REOPEN_OK',flush=True)
'''


def executables(bundle):
    result = {}
    for name, pattern in [('helper','usr/**/gimp-painter-filter-worker'),
                          ('plugin','usr/**/plug-ins/convolution-matrix/convolution-matrix')]:
        paths = [p.resolve() for p in bundle.glob(pattern) if p.is_file()]
        if len(paths) != 1 or not paths[0].is_relative_to(bundle) or not os.access(paths[0],os.X_OK):
            raise RuntimeError('Missing or misplaced installed '+name)
        result[name] = str(paths[0])
    return result


def run_case(bundle, fixture_root, case, output):
    output.mkdir()
    config = {**case,'output':str(output)}
    for key in ('fixture','source','initial','expected'):
        path = (fixture_root/case[key]).resolve()
        if not path.is_relative_to(fixture_root) or not path.is_file():
            raise RuntimeError('Invalid synthetic input '+key)
        if sha(path) != case['files_sha256'][case[key]]:
            raise RuntimeError('Synthetic fixture seal changed: '+key)
        config[key] = str(path)
    expected = executables(bundle)
    executable_hashes = {key:sha(path) for key,path in expected.items()}
    profile = output/'profile'
    for name in ('home','config','cache','data'): (profile/name).mkdir(parents=True)
    env = dict(PATH='/usr/bin:/bin',LANG='C.UTF-8',HOME=str(profile/'home'),
               XDG_CONFIG_HOME=str(profile/'config'),XDG_CACHE_HOME=str(profile/'cache'),
               XDG_DATA_HOME=str(profile/'data'),GIMP_PAINTER_CONVOLUTION_SMOKE=json.dumps(config))
    command = [str(bundle/'AppRun'),'--console','--new-instance','--no-interface','--no-data',
               '--no-fonts','--no-splash','--batch-interpreter=python-fu-eval','-b','-','--quit']
    (output/'batch.executed.py').write_text(BATCH)
    observed, profiles, errors, acknowledged = {}, set(), [], set()
    phases = {name:{'helpers':set(),'plugins':set()} for name in ('first','reopened-rerun')}
    began = time.monotonic()
    with (output/'console.log').open('w') as log:
        process = subprocess.Popen(command,env=env,stdin=subprocess.PIPE,stdout=log,stderr=subprocess.STDOUT,
                                   text=True,start_new_session=True,cwd=output)
        try:
            process.stdin.write(BATCH); process.stdin.close()
            while process.poll() is None:
                current = observer.descendants(process.pid)
                observed.update({(v['pid'],v['start_ticks']):v for v in current.values()})
                helpers = [v for v in current.values() if v['exe'] == expected['helper'] and v['state'] != 'Z']
                for v in helpers:
                    if len(v['argv']) == 3: profiles.add(v['argv'][2])
                events = observer.events(output/'events.jsonl')
                active_phase = None
                for event in events:
                    if event['event'] == 'INVALIDATED': active_phase = event['phase']
                    elif event['event'] in ('READY','REOPENED_READY','DONE'): active_phase = None
                if active_phase in phases:
                    ids = {v['pid'] for v in helpers}
                    phases[active_phase]['helpers'].update((v['pid'],v['start_ticks']) for v in helpers)
                    phases[active_phase]['plugins'].update((v['pid'],v['start_ticks']) for v in current.values()
                        if v['exe'] == expected['plugin'] and v['parent'] in ids and '-run' in v['argv'])
                for event, gate in [('READY','start-first'),('REOPENED_READY','start-rerun')]:
                    if event not in acknowledged and any(v['event'] == event for v in events):
                        if helpers: errors.append('Helper active before explicit lower update: '+event)
                        acknowledged.add(event); (output/gate).write_text('Ready\n')
                if time.monotonic()-began > 240:
                    errors.append('Installed smoke exceeded240seconds')
                    os.killpg(process.pid,signal.SIGKILL); break
                time.sleep(0.005)
            code = process.wait(timeout=5)
        finally:
            if process.poll() is None:
                os.killpg(process.pid,signal.SIGKILL); process.wait()
    text = (output/'console.log').read_text(errors='replace')
    if code or 'INSTALLED_CONVOLUTION_EXACT_SAVE_REOPEN_OK' not in text:
        errors.append('Console did not confirm exact pixels and Save/reopen')
    for marker in observer.SANITIZER_DIAGNOSTICS+('Traceback (most recent call last)','Gimp-Core-CRITICAL','Filter cleanup exceeded'):
        if marker in text: errors.append('Runtime diagnostic: '+marker)
    helpers = [v for v in observed.values() if Path(v['exe']).name == 'gimp-painter-filter-worker']
    helper_ids = {v['pid'] for v in helpers}
    plugins = [v for v in observed.values() if v['parent'] in helper_ids and '-run' in v['argv']]
    if acknowledged != {'READY','REOPENED_READY'} or any(not identities for phase in phases.values() for identities in phase.values()):
        errors.append('Missing two actual installed helper/plugin executions or readiness handshakes')
    if any(v['exe'] != expected['helper'] or len(v['argv']) != 3 or v['argv'][1] != '--filter-worker-v5' for v in helpers):
        errors.append('Unexpected helper path or protocol')
    if any(v['exe'] != expected['plugin'] for v in plugins): errors.append('Wrong installed plugin path')
    survivors = [v for v in observed.values() if observer.still_same(v)]
    leftovers = [p for p in sorted(profiles) if Path(p).exists()]
    if survivors or leftovers: errors.append('Owned process or private profile survived console exit')
    for v in survivors:
        if observer.still_same(v):
            try: os.killpg(v['group'],signal.SIGKILL)
            except ProcessLookupError: pass
    if executable_hashes != {key:sha(path) for key,path in expected.items()}:
        errors.append('Installed executable changed during smoke')
    batch_path = output/'batch-result.json'
    batch = json.loads(batch_path.read_text()) if batch_path.exists() else None
    if not batch or batch.get('expected_sha256') != sha(config['expected']) or batch.get('status') != 'passed':
        errors.append('Missing sealed expected-pixel success result')
    report = dict(status='failed' if errors else 'passed',errors=errors,case=case['id'],exit_code=code,
                  seconds=time.monotonic()-began,command=command,expected_executables=expected,
                  executable_sha256=executable_hashes,observed_helpers=helpers,observed_plugins=plugins,
                  phase_process_identities={phase:{kind:sorted(ids) for kind,ids in kinds.items()} for phase,kinds in phases.items()},
                  survivors=survivors,leftover_profiles=leftovers,batch_result=batch,
                  events=observer.events(output/'events.jsonl'),
                  startup_diagnostics={'scriptfu_resource_warnings':text.count('script_fu_add_resource_arg declared resource name is invalid'),
                                       'welcome_messages':text.count('Welcome to GIMP')},
                  input_sha256={key:sha(config[key]) for key in ('fixture','source','initial','expected')},
                  observer_sha256=sha(__file__),process_observer_sha256=sha(ROOT/'tools/check_filter_active_quit.py'),
                  batch_sha256=sha(output/'batch.executed.py'),console_sha256=sha(output/'console.log'))
    (output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle',type=Path,required=True)
    parser.add_argument('--inputs',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    source_bundle, inputs, output = args.bundle.resolve(),args.inputs.resolve(),args.output.resolve()
    output.mkdir(parents=True,exist_ok=False)
    manifest = json.loads((inputs/'manifest.json').read_text())
    cases = manifest['cases']
    for case in cases:
        case['bytes_per_pixel'] = {"R'G'B'A u8":4,"R'G'B'A double":32}[case['format']]
    assert len(cases) == 2 and len({v['id'] for v in cases}) == 2
    assert {v['bytes_per_pixel'] for v in cases} == {4,32}
    assert all(v['id'] and Path(v['id']).name == v['id'] and v['id'] not in ('.','..') for v in cases)
    bundle = output/'relocated path 日本語'/source_bundle.name
    shutil.copytree(source_bundle,bundle,symlinks=True)
    package = json.loads((bundle/'build-manifest.json').read_text())
    dependencies = {row['soname'] for rows in package['elf_dependency_graph'].values() for row in rows}
    legacy = sorted(name for name in dependencies if name.startswith((
        'libgtk-x11-2.0','libgdk-x11-2.0','libgimp-2.0','libgimpbase-2.0','libgegl-0.2','libgegl-0.3')))
    assert not legacy, 'Packaged runtime unexpectedly links a legacy GIMP/GTK/GEGL ABI'
    required = {'libgimp-3.0.so.0','libgtk-3.so.0','libgegl-0.4.so.0'}
    assert required <= dependencies
    reports = [run_case(bundle,inputs,case,output/case['id']) for case in cases]
    report = dict(status='passed' if all(v['status']=='passed' for v in reports) else 'failed',
                  cases=reports,input_manifest_sha256=sha(inputs/'manifest.json'),
                  relocation='Fresh path with spaces and Japanese characters',
                  environment='Explicit minimum whitelist; no inherited dependency environment',
                  packaged_abi={'required':sorted(required),'legacy_dependencies':legacy,
                                'build_manifest_sha256':sha(bundle/'build-manifest.json')},
                  scope='Two synthetic native fixture cases; two explicit reruns and two Save/reopen cycles each; Linux console only')
    (output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(status=report['status'],report=str(output/'report.json'),
                         errors=[v['errors'] for v in reports])))
    return 0 if report['status']=='passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
