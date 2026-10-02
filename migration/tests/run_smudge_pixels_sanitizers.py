#!/usr/bin/env python3
"""Compare all captured native Smudge integer kernels with focused sanitizers.
Hold the shared build lock. No production object is replaced.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
p = argparse.ArgumentParser()
p.add_argument('build', type=Path)
p.add_argument('--report', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
build = a.build.resolve()
out = build / 'smudge-pixels-sanitizers'
out.mkdir(exist_ok=True)
sources = ['app/paint/painter-smudge/legacy-pixels.hpp', 'app/paint/painter-smudge/tests/pixels-trace.cpp']
hashes = {s: hashlib.sha256((root / s).read_bytes()).hexdigest() for s in sources}
flags = ['-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-O1']
compile_entry = next(x for x in json.loads((build / 'compile_commands.json').read_text()) if (Path(x['directory']) / x['file']).resolve() == root / sources[1])
cmd = shlex.split(compile_entry['command'])
obj = out / 'pixels-trace.o'
cmd[cmd.index('-o') + 1] = str(obj)
cmd += flags
subprocess.run(cmd, cwd=build, check=True)
link = shlex.split(subprocess.check_output(['ninja', '-t', 'commands', 'app/paint/painter-smudge/painter-smudge-pixels-trace'], cwd=build, text=True).strip().splitlines()[-1])
exe = out / 'painter-smudge-pixels-trace'
link[link.index('-o') + 1] = str(exe)
link = [str(obj) if x == compile_entry['output'] else x for x in link]
link[1:1] = flags
subprocess.run(link, cwd=build, check=True)
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
run = subprocess.run([str(exe)], cwd=build, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
expected = gzip.decompress((root / 'migration/fixtures/legacy-smudge-pixels/pixels.tsv.gz').read_bytes())
changed = [s for s, h in hashes.items() if hashlib.sha256((root / s).read_bytes()).hexdigest() != h]
report = {'scope': 'Exact legacy accumulation, shading and native paint composition kernels; one C++ test translation unit plus header; no full app claim',
          'sanitizers': ['address', 'undefined', 'float-cast-overflow'], 'leak_detection': False,
          'sources_sha256': hashes, 'changed_during_run': changed, 'commands': [cmd, link],
          'exit_code': run.returncode, 'stderr': run.stderr.decode(), 'exact_legacy_match': run.stdout == expected,
          'expected_bytes': len(expected), 'cases': 1024, 'expected_sha256': hashlib.sha256(expected).hexdigest()}
a.report.write_text(json.dumps(report, indent=2) + '\n')
assert not changed and run.returncode == 0 and run.stdout == expected
print(f'1024 native legacy kernel cases exact; {len(expected)} bytes; ASan/UBSan passed')
