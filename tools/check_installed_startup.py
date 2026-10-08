#!/usr/bin/env python3
"""Exercise a staged GIMP startup inside an externally isolated filesystem.

The caller must mount only the installed runtime and copied test inputs, with
the original source, build, prefix and development dependency paths absent.
This probe checks their absence again and does not use a developer environment.
It covers normal data restore, real GUI windows when requested, installed
modules, serialized host tool registrations, resource containers and shutdown.
It is an install acceptance test, not a complete release or feature suite.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import shutil
import subprocess
import time
import uuid
import xml.etree.ElementTree as ET


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def installed_modules(bundle):
    """The staging receipt fixes the expected set before the runtime is tested."""
    prefix = 'usr/lib/x86_64-linux-gnu/gimp/3.0/modules/'
    manifest = json.loads((bundle/'file-manifest.json').read_text())
    expected = {name: row for name, row in manifest.items()
                if name.startswith(prefix) and name.endswith('.so')}
    actual = {str(path.relative_to(bundle)) for path in (bundle/prefix).glob('*.so')}
    if not expected or actual != set(expected):
        raise RuntimeError('Installed module set differs from staging receipt')
    for name, row in expected.items():
        if sha(bundle/name) != row['sha256']:
            raise RuntimeError('Installed module differs from staging receipt: '+name)
    return sorted(str(bundle/name) for name in expected)


def gui_observation_valid(observation, run_id, image_name):
    return (observation.get('run_id') == run_id and
            any(image_name in row.get('title', '') and 'GIMP' in row['title'] and
                row.get('visible') is True and row.get('width', 0) > 0 and
                row.get('height', 0) > 0 for row in observation.get('windows', [])))


BATCH = '''import json, os, time
from pathlib import Path
target = Path(os.environ['GIMP_STARTUP_OUTPUT'])
gui = os.environ['GIMP_STARTUP_GUI'] == '1'
assert Gimp.get_pdb().lookup_procedure('plug-in-blinds') is not None
assert Gimp.get_pdb().lookup_procedure('plug-in-painter-small-tiles') is not None
assert Gimp.get_pdb().lookup_procedure('plug-in-painter-retinex') is not None
assert Gimp.get_pdb().lookup_procedure('plug-in-painter-convmatrix') is not None
resources = {kind: len(getter('')) for kind, getter in (
    ('brushes', Gimp.brushes_get_list), ('patterns', Gimp.patterns_get_list),
    ('gradients', Gimp.gradients_get_list), ('palettes', Gimp.palettes_get_list))}
assert all(resources.values()), resources
image = Gimp.Image.new(64, 64, Gimp.ImageBaseType.RGB)
layer = Gimp.Layer.new(image, 'installed startup', 64, 64,
                      Gimp.ImageType.RGBA_IMAGE, 100.0, Gimp.LayerMode.NORMAL)
assert image.insert_layer(layer, None, 0)
assert Gimp.context_set_foreground(Gegl.Color.new('rgb(1.0,0.0,0.0)'))
assert layer.edit_fill(Gimp.FillType.FOREGROUND)
destination = Gio.File.new_for_path(str(target/os.environ['GIMP_STARTUP_IMAGE']))
assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, image, destination, None)
image.delete()
reopened = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, destination)
assert reopened and len(reopened.get_layers()) == 1
assert reopened.get_layers()[0].get_name() == 'installed startup'
display = Gimp.Display.new(reopened) if gui else None
if gui:
    assert display is not None
    Gimp.displays_flush()
(target/'batch-ready.json').write_text(json.dumps(dict(resources=resources, gui=gui,
    run_id=os.environ['GIMP_STARTUP_RUN_ID'], image_name=os.environ['GIMP_STARTUP_IMAGE'])))
deadline = time.monotonic() + 120
while not (target/'observer-ready').exists():
    assert time.monotonic() < deadline, 'Startup observer did not acknowledge'
    time.sleep(0.02)
if not gui:
    reopened.delete()
print('INSTALLED_NORMAL_RESTORE_SAVE_REOPEN_OK')
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--forbid', action='append', required=True)
    parser.add_argument('--gui', action='store_true')
    parser.add_argument('--gui-observation', type=Path,
                        help='Externally recorded visible GIMP window observation; required for --gui')
    parser.add_argument('--user-resources', action='store_true',
                        help='Seed a synthetic current-format user brush and layer preset')
    args = parser.parse_args()
    if args.gui and not args.gui_observation:
        parser.error('--gui requires --gui-observation')
    if args.gui and args.gui_observation.exists():
        parser.error('GUI observation must be new, not a previous run receipt')
    bundle, output = args.bundle.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    absent = {name: not Path(name).exists() for name in args.forbid}
    if not all(absent.values()):
        raise RuntimeError('Development paths remain accessible: ' +
                           ', '.join(name for name, missing in absent.items() if not missing))
    profile = output/'profile'
    run_id = uuid.uuid4().hex
    image_name = 'startup-'+output.name+'.xcf'
    modules = installed_modules(bundle)
    for name in ('config', 'cache', 'data', 'runtime'):
        (profile/name).mkdir(parents=True, mode=0o700)
    user_inputs = {}
    if args.user_resources:
        brush = profile/'gimp/painter-mypaint-brushes/installed-user-probe.myb'
        brush.parent.mkdir(parents=True)
        source = bundle/'usr/share/gimp/3.0/painter-mypaint-brushes/tanda/pencil-2b.myb'
        shutil.copy2(source, brush)
        preset = profile/'gimp/layer-presets/installed-user-probe.json'
        preset.parent.mkdir(parents=True)
        preset.write_text(json.dumps({'version': 1, 'name': 'Installed user probe',
                                      'source-layer': {'type': 'any'},
                                      'replacement-layer': []})+'\n')
        user_inputs = {str(path): sha(path) for path in (brush, preset)}
    environment = {'PATH': '/usr/bin:/bin', 'LANG': 'C.UTF-8', 'LC_ALL': 'C.UTF-8',
                   'PYTHONDONTWRITEBYTECODE': '1',
                   'XDG_CONFIG_HOME': str(profile/'config'),
                   'XDG_CACHE_HOME': str(profile/'cache'),
                   'XDG_DATA_HOME': str(profile/'data'),
                   'XDG_RUNTIME_DIR': str(profile/'runtime'),
                   'GIMP_PAINTER_PROFILE': str(profile/'gimp'),
                   'GIMP_PAINTER_CACHE': str(profile/'gimp-cache'),
                   'GIMP_STARTUP_OUTPUT': str(output),
                   'GIMP_STARTUP_RUN_ID': run_id,
                   'GIMP_STARTUP_IMAGE': image_name,
                   'GIMP_STARTUP_GUI': '1' if args.gui else '0'}
    if args.gui:
        environment['DISPLAY'] = os.environ['DISPLAY']
        if 'XAUTHORITY' in os.environ:
            environment['XAUTHORITY'] = os.environ['XAUTHORITY']
    arguments = [str(bundle/'AppRun')]
    if not args.gui:
        arguments.append('--console')
    arguments += ['--new-instance', '--verbose', '--no-splash',
                  '--batch-interpreter=python-fu-eval', '-b', '-', '--quit']
    (output/'batch.executed.py').write_text(BATCH)
    (output/'probe.executed.py').write_bytes(Path(__file__).read_bytes())
    errors, observed_windows = [], []
    start, acknowledged = time.monotonic(), False
    with (output/'startup.log').open('w') as log:
        child = subprocess.Popen(arguments, env=environment, cwd=output,
                                 stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT,
                                 text=True, start_new_session=True)
        try:
            child.stdin.write(BATCH)
            child.stdin.close()
            while child.poll() is None:
                if not acknowledged and (output/'batch-ready.json').exists():
                    if args.gui:
                        if not args.gui_observation.exists():
                            time.sleep(.05)
                            continue
                        observation = json.loads(args.gui_observation.read_text())
                        observed_windows = observation['windows']
                        if not gui_observation_valid(observation, run_id, image_name):
                            raise RuntimeError('GUI observation does not identify this run and image')
                    (output/'observer-ready').write_text('Observed normal startup and batch result\n')
                    acknowledged = True
                if time.monotonic()-start > 240:
                    errors.append('Installed startup exceeded 240 seconds')
                    os.killpg(child.pid, signal.SIGKILL)
                    break
                time.sleep(.02)
            code = child.wait(timeout=10)
        finally:
            if child.poll() is None:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
    text = (output/'startup.log').read_text(errors='replace')
    if code or not acknowledged or 'INSTALLED_NORMAL_RESTORE_SAVE_REOPEN_OK' not in text:
        errors.append('Normal resource restore, Save/reopen or orderly batch quit failed')
    diagnostics = [line for line in text.splitlines() if any(token in line for token in
                  ('CRITICAL', 'WARNING', 'Error', 'error:', 'Traceback', 'Module ', 'segmentation'))]
    fatal = [line for line in diagnostics if any(token in line for token in
             ('CRITICAL', 'Traceback', 'load error:', 'segmentation', 'AddressSanitizer', 'runtime error:'))]
    if fatal:
        errors.append('Fatal startup diagnostic')
    tag_path = profile/'gimp/tags.xml'
    resources = [element.get('identifier', '') for element in ET.parse(tag_path).getroot()] if tag_path.exists() else []
    painter = [name for name in resources if name.startswith('external:${gimp_data_dir}/painter-mypaint-brushes/')]
    presets = [name for name in resources if name.startswith('external:${gimp_data_dir}/layer-presets/')]
    if len(painter) != 177 or len(presets) != 8:
        errors.append('Host resource containers did not retain all 177 Painter brushes and 8 presets')
    user_loaded = [name for name in resources if name.startswith('external:${gimp_dir}/') and
                   ('/painter-mypaint-brushes/' in name or '/layer-presets/' in name)]
    if args.user_resources and (len(user_loaded) != 2 or
                                not all(sha(Path(path)) == digest for path, digest in user_inputs.items())):
        errors.append('Explicit user resources did not load unchanged')
    loaded_modules = [path for path in modules if f"Loading module '{path}'" in text]
    tools = ['gimp-painter-mypaint-tool', 'gimp-bucket-fill-brush-tool',
             'gimp-painter-smudge-tool', 'gimp-perspective-guide-tool']
    toolrc = (profile/'gimp/toolrc').read_text() if args.gui and (profile/'gimp/toolrc').exists() else ''
    registered_tools = [name for name in tools if name in toolrc]
    if args.gui and (len(loaded_modules) != len(modules) or not modules or registered_tools != tools):
        errors.append('Missing installed GUI modules or Painter tool registrations')
    report = {'task': '04.016', 'status': 'failed' if errors else 'passed', 'errors': errors,
              'gui': args.gui, 'exit_code': code, 'seconds': time.monotonic()-start,
              'run_id': run_id, 'image_name': image_name,
              'command': arguments, 'development_paths_absent': absent,
              'observed_windows': observed_windows, 'registered_tools': registered_tools,
              'installed_modules': modules, 'loaded_modules': loaded_modules,
              'painter_resources': painter, 'layer_presets': presets,
              'user_inputs_sha256': user_inputs, 'user_resources': user_loaded,
              'diagnostics': diagnostics, 'probe_sha256': sha(Path(__file__)),
              'log_sha256': sha(output/'startup.log'), 'tags_sha256': sha(tag_path) if tag_path.exists() else None,
              'batch_result': json.loads((output/'batch-ready.json').read_text())
                              if (output/'batch-ready.json').exists() else None}
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({'status': report['status'], 'errors': errors, 'gui': args.gui,
                      'painter_resources': len(painter), 'layer_presets': len(presets),
                      'loaded_modules': len(loaded_modules)}))
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
