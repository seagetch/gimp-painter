#!/usr/bin/env python3
"""Build private focused HTTP/REST native and GTK ASan/UBSan/vptr harnesses.

Requires the enabled, current normal build. Hold the shared build/test lock.
No production object/archive is replaced and no listener is started by this builder.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
from painter_sanitizer_scope import bridge_rtti_sources

parser = argparse.ArgumentParser()
parser.add_argument('build', type=Path)
parser.add_argument('--report', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
build = args.build.resolve()
out = build / 'httpd-sanitizers'
out.mkdir(exist_ok=True)
targets = ['painter-http', 'painter-http-ui']
instrumented = {
    'app/httpd/httpd.cpp', 'app/httpd/httpd-resource.cpp',
    'app/httpd/httpd-images.cpp', 'app/httpd/httpd-pdb.cpp',
    'app/httpd/httpd-navigation.cpp',
    'app/painter/binding-store.cpp', 'app/painter/gimp-painter-binding.cpp',
    'app/painter/gimp-painter-error.cpp',
    'app/core/gimp.c', 'app/core/gimpobject.c', 'app/core/gimpimage.c', 'app/core/gimpitem.c',
    'app/core/gimpdisplay.c', 'app/core/gimpcontext.c',
    'app/display/gimpdisplay.c', 'app/pdb/gimpprocedure.c',
    'app/pdb/gimppdb.c', 'app/tests/test-painter-http.cpp',
    'app/tests/test-painter-http-ui.cpp',
}
rtti_only = bridge_rtti_sources(root, build) - instrumented
sources = instrumented | rtti_only
headers = {
    'app/httpd/httpd.h', 'app/httpd/httpd-resource.hpp',
    'app/httpd/httpd-private.hpp', 'app/core/gimp.h',
    'app/config/gimpcoreconfig.h', 'app/core/core-enums.h',
    'app/core/gimp-painter-provenance-private.h', 'app/core/gimpobject.h',
    'app/painter/binding-store.hpp', 'app/painter/boundary.hpp',
    'app/painter/object-ref.hpp', 'app/painter/connection.hpp',
    'app/painter/source.hpp', 'migration/tests/painter_sanitizer_scope.py',
}
headers |= {str(Path(source).with_suffix(suffix)) for source in rtti_only
            for suffix in ['.h', '.hpp']
            if (root / Path(source).with_suffix(suffix)).exists()}
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
hashes = {name: sha(root / name) for name in sources | headers}
entries = json.loads((build / 'compile_commands.json').read_text())
objects, originals, commands = {}, {}, []
flags = ['-fsanitize=address,undefined,float-cast-overflow',
         '-fno-omit-frame-pointer', '-O1']
for source in sorted(sources):
    candidates = [entry for entry in entries
                  if (Path(entry['directory']) / entry['file']).resolve() == root / source]
    entry = next((e for e in candidates if e['output'].startswith(str(Path(source).parent) + '/')),
                 candidates[0])
    command = shlex.split(entry['command'])
    cleaned, skip = [], False
    for arg in command:
        if skip:
            skip = False
            continue
        if arg in ['-MF', '-MQ', '-MT']:
            skip = True
            continue
        if arg not in ['-MD', '-MMD']:
            cleaned.append(arg)
    obj = out / (source.replace('/', '_') + '.o')
    cleaned[cleaned.index('-o') + 1] = str(obj)
    if source not in rtti_only:
        cleaned += flags
    if source.endswith(('.cpp', '.cc')):
        cleaned += ['-frtti']
    commands.append(cleaned)
    key = hashlib.sha256(json.dumps({
        'command': cleaned, 'source': hashes[source],
        'headers': {name: hashes[name] for name in headers},
        'ordinary_object': sha(build / entry['output']),
    }, sort_keys=True).encode()).hexdigest()
    keyfile = obj.with_suffix('.key')
    if not (obj.exists() and keyfile.exists() and keyfile.read_text() == key):
        subprocess.run(cleaned, cwd=build, check=True)
        keyfile.write_text(key)
    objects[source] = str(obj)
    originals[entry['output']] = source

absolute = {str((build / path).resolve()): objects[source]
            for path, source in originals.items()}
executables = {}
for target in targets:
    link = shlex.split(subprocess.check_output(
        ['ninja', '-t', 'commands', 'app/tests/' + target], cwd=build,
        text=True).strip().splitlines()[-1])
    exe = build / ('app/tests/' + target + '-asan')
    link[link.index('-o') + 1] = str(exe)
    for archive in sorted({arg for arg in link if arg.endswith('.a')
                           and (build / arg).resolve().is_relative_to(build)}):
        members = [str((build / member).resolve()) for member in subprocess.check_output(
            ['ar', 't', archive], cwd=build, text=True).splitlines()]
        if not any(member in absolute for member in members):
            continue
        dest = out / archive.replace('/', '_')
        if dest.exists():
            dest.unlink()
        command = ['ar', 'crsT', str(dest)] + [absolute.get(member, member) for member in members]
        commands.append(command)
        subprocess.run(command, cwd=build, check=True)
        link = [str(dest) if arg == archive else arg for arg in link]
    link = [objects[originals[arg]] if arg in originals and originals[arg].startswith('app/tests/')
            else arg for arg in link]
    link[1:1] = flags
    commands.append(link)
    subprocess.run(link, cwd=build, check=True)
    executables[str(exe)] = sha(exe)
changed = [name for name, digest in hashes.items() if sha(root / name) != digest]
report = {
    'scope': 'HTTP resources/router/transport/guide, shared BindingStore, native Gimp/image/item/display/PDB lifecycle and both test harnesses instrumented; other production C++ is RTTI-only; host libraries including Soup and GTK uninstrumented; LSan disabled',
    'instrumented_sources': sorted(instrumented),
    'rtti_compatibility_only_sources': sorted(rtti_only),
    'sources_sha256': hashes, 'changed_during_build': changed,
    'executables_sha256': executables, 'commands': commands,
    'run_status': 'Not run by builder; UI harness requires native GTK display',
}
args.report.write_text(json.dumps(report, indent=2) + '\n')
assert not changed, changed
for path in executables:
    print(path)
