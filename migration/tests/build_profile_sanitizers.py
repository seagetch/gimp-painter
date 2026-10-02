#!/usr/bin/env python3
"""Build private, focused profile-import and native GimpConfig instrumentation.

Hold /workspace/shared/gimp-painter-build.lock. Ordinary objects are unchanged.
The remaining application, GTK and other shared libraries are uninstrumented.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

p = argparse.ArgumentParser()
p.add_argument('build', type=Path)
p.add_argument('--report', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
build = a.build.resolve()
out = build / 'profile-sanitizers'
out.mkdir(exist_ok=True)
sources = {
    'app/core/gimppainterprofile.cpp', 'app/core/gimp-user-install.c',
    'app/core/gimptoolgroup.c', 'app/core/gimptooloptions.c',
    'app/core/gimptoolpreset.c', 'app/core/gimpcontext.c',
    'app/config/gimpcoreconfig.c', 'app/tools/gimp-tools.c',
    'app/tools/gimp-tool-options-manager.c', 'app/widgets/gimpdeviceinfo.c',
    'app/tests/test-painter-profile-native.cpp',
}
headers = {
    'app/core/gimppainterprofile.h', 'app/core/gimptooloptions.h',
    'app/core/gimptoolpreset.h', 'app/core/gimptoolgroup.h',
    'app/core/gimpcontext.h', 'app/config/gimpcoreconfig.h',
    'app/painter/gio-type-traits.hpp', 'app/painter/object-ref.hpp',
    'app/painter/resources.hpp', 'app/painter/boundary.hpp',
    'app/paint/gimppaintoptions.h', 'app/paint/gimpbrushcore.h',
    'app/widgets/gimpdeviceinfo.h',
}
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
hashes = {s: sha(root / s) for s in sources | headers}
entries = json.loads((build / 'compile_commands.json').read_text())
objects, originals, commands = {}, {}, []
flags = ['-fsanitize=address,undefined,float-cast-overflow',
         '-fno-omit-frame-pointer', '-O1']
for source in sorted(sources):
    candidates = [e for e in entries if (Path(e['directory']) / e['file']).resolve() == root / source]
    e = next((e for e in candidates if e['output'].startswith(str(Path(source).parent) + '/')), candidates[0])
    cmd = shlex.split(e['command'])
    clean, skip = [], False
    for arg in cmd:
        if skip:
            skip = False
            continue
        if arg in ['-MF', '-MQ', '-MT']:
            skip = True
            continue
        if arg in ['-MD', '-MMD']:
            continue
        clean.append(arg)
    obj = out / (Path(source).name + '.o')
    clean[clean.index('-o') + 1] = str(obj)
    clean += flags
    if source.endswith('.cpp'):
        clean += ['-frtti']
    commands.append(clean)
    # A rebuilt ordinary object covers transitive Meson dependency changes.
    # Cache only exact compiler commands, source/header bytes and that object.
    key = hashlib.sha256(json.dumps({
        'command': clean, 'source': hashes[source],
        'headers': {h: hashes[h] for h in headers},
        'ordinary_object': sha(build / e['output']),
    }, sort_keys=True).encode()).hexdigest()
    keyfile = obj.with_suffix('.key')
    if not (obj.exists() and keyfile.exists() and keyfile.read_text() == key):
        subprocess.run(clean, cwd=build, check=True)
        keyfile.write_text(key)
    objects[source] = str(obj)
    originals[e['output']] = source
replaced = {}
for archive in ['app/core/libappcore.a', 'app/config/libappconfig.a',
                'app/tools/libapptools.a', 'app/widgets/libappwidgets.a']:
    members = subprocess.check_output(['ar', 't', archive], cwd=build, text=True).splitlines()
    result = [objects[originals[m]] if m in originals else str((build / m).resolve()) for m in members]
    dest = out / Path(archive).name
    if dest.exists():
        dest.unlink()
    cmd = ['ar', 'crsT', str(dest), *result]
    commands.append(cmd)
    subprocess.run(cmd, cwd=build, check=True)
    replaced[archive] = str(dest)
cmd = shlex.split(subprocess.check_output(
    ['ninja', '-t', 'commands', 'app/tests/painter-profile-native'], cwd=build,
    text=True).strip().splitlines()[-1])
exe = out / 'painter-profile-native'
cmd[cmd.index('-o') + 1] = str(exe)
cmd = [objects[originals[x]] if x in originals and originals[x].startswith('app/tests/')
       else replaced.get(x, x) for x in cmd]
cmd[1:1] = flags
commands.append(cmd)
subprocess.run(cmd, cwd=build, check=True)
changed = [s for s, h in hashes.items() if sha(root / s) != h]
a.report.write_text(json.dumps({
    'scope': '11 focused units: importer, real installer, group/context/options/preset/config/device readers and native tests; remaining host/dependencies uninstrumented; LSan disabled',
    'sources_sha256': hashes, 'changed_during_build': changed,
    'commands': commands, 'executable': str(exe),
    'executable_sha256': sha(exe),
    'run_status': 'Not run by builder; requires native GTK display',
}, indent=2) + '\n')
assert not changed
print(exe)
