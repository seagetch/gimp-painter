#!/usr/bin/env python3
"""Link private counterfactual providers into the native catalog test.

Run under the same authenticated X display and dependency environment as Meson.
Only files below --output are written; the normal build/provider stay unchanged.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

BASE = 'aec6d7296506f51c1991ab3f27a84fc61e3de4df'
SOURCE = 'app/presets/gimppresetdescriptor.c'
REPO = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
build = args.build.resolve()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
source = REPO / SOURCE
original_bytes = source.read_bytes()
sha = lambda data: hashlib.sha256(data).hexdigest()
entry = next(e for e in json.loads((build / 'compile_commands.json').read_text())
             if Path(e['directory'], e['file']).resolve() == source)
provider_object = (build / entry['output']).resolve()
link = shlex.split(subprocess.check_output(
    ['ninja', '-t', 'commands', 'app/tests/preset-catalog'],
    cwd=build, text=True).strip().splitlines()[-1])
archive = next(arg for arg in link if arg.endswith('.a') and provider_object in
               [Path(build, member).resolve() for member in subprocess.check_output(
                   ['ar', 't', arg], cwd=build, text=True).splitlines()])
members = [Path(build, member).resolve() for member in subprocess.check_output(
    ['ar', 't', archive], cwd=build, text=True).splitlines()]
old = subprocess.check_output(['git', 'show', f'{BASE}:{SOURCE}'], cwd=REPO)
needle = '''  else if (!catalog_present)
    /* A retired owner's outstanding lease may still reserve the catalog. A
     * successful new owner nevertheless makes the current metadata present. */
    select_catalog (catalog, TRUE);
'''
assert original_bytes.decode().count(needle) == 1
cases = [
    ('original-provider', old, 'empty-preentry-getters', '(1 == 0)'),
    ('missing-stale-lease-presence', original_bytes.decode().replace(needle, '').encode(),
     'new-owner-before-stale-lease-release', '(0 == 1)'),
]
results = []
for label, contents, case, assertion in cases:
    mutant = out / f'{label}.c'
    mutant.write_bytes(contents)
    obj = out / f'{label}.o'
    command = shlex.split(entry['command'])
    command = [str(mutant) if Path(entry['directory'], arg).resolve() == source else arg
               for arg in command]
    command[command.index('-o') + 1] = str(obj)
    # Relative provider-local headers must still be the final native headers.
    command += ['-I' + str(source.parent)]
    subprocess.run(command, cwd=build, check=True, capture_output=True)
    replacement = out / f'{label}.a'
    replacement.unlink(missing_ok=True)
    subprocess.run(['ar', 'crsT', str(replacement),
                    *[str(obj if member == provider_object else member)
                      for member in members]], cwd=build, check=True)
    executable = out / label
    command = [str(replacement) if arg == archive else arg for arg in link]
    command[command.index('-o') + 1] = str(executable)
    subprocess.run(command, cwd=build, check=True, capture_output=True)
    result = subprocess.run([str(executable), '-p', '/preset-catalog/' + case],
                            cwd=build, text=True, capture_output=True, timeout=90)
    record = dict(name=label, case=case, returncode=result.returncode,
                  stdout=result.stdout, stderr=result.stderr,
                  expected_assertion=assertion, source_sha256=sha(contents))
    results.append(record)
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    assert result.returncode != 0 and assertion in result.stdout + result.stderr, record
assert source.read_bytes() == original_bytes
print('PASS: original false entry and missing stale-lease presence rejected; normal provider unchanged')
