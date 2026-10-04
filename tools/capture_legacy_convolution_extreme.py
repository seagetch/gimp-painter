#!/usr/bin/env python3
"""Observe a finite extreme-coefficient counterexample in the real old PDB.

Uses only 0/1 source pixels, center coefficient=divisor=3e38, offset=0.
The regular sealed corpus is not modified. Source the legacy env.sh first.
"""
from datetime import datetime, timezone
import argparse
import difflib
import gzip
import hashlib
import json
from pathlib import Path
import subprocess

import capture_legacy_convolution as capture

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'migration/fixtures/legacy-convolution-extreme')
    parser.add_argument('--source', type=Path, default=Path('/workspace/shared/gimp-legacy-convolution/source/gimp'))
    parser.add_argument('--prefix', type=Path, default=Path('/workspace/shared/gimp-legacy-build/prefix'))
    parser.add_argument('--work', type=Path, default=Path('/workspace/shared/gimp-legacy-convolution/extreme-harness'))
    args = parser.parse_args()
    out, source, prefix, work = (p.resolve() for p in (args.output,args.source,args.prefix,args.work))
    if (out/'capture-report.json').exists(): raise SystemExit('Existing sealed observation; choose another output')
    commit = subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
    assert commit == capture.COMMIT
    source_hashes = {}
    for name in capture.SOURCES:
        actual = (source/name).read_bytes()
        pinned = subprocess.check_output(['git','-C',str(source),'show',commit+':'+name])
        assert actual == pinned, 'Changed original source: '+name
        source_hashes[name] = hashlib.sha256(actual).hexdigest()
    out.mkdir(parents=True,exist_ok=True); work.mkdir(parents=True,exist_ok=True)
    harness = ROOT/'migration/tests/legacy-convolution-capture.c'
    original = harness.read_text()
    expression = '(x * (37 + c * 17) + y * (61 + c * 23) + variant * 97 + c * 51 + 11) % 256'
    assert original.count(expression) == 1
    changed = original.replace(expression,'(x + y + c) % 2')
    temporary_root = work/'harness-root'
    temporary_harness = temporary_root/'migration/tests/legacy-convolution-capture.c'
    temporary_harness.parent.mkdir(parents=True,exist_ok=True)
    temporary_harness.write_text(changed)
    patch = ''.join(difflib.unified_diff(original.splitlines(True),changed.splitlines(True),
                                       fromfile='a/legacy-convolution-capture.c',tofile='b/legacy-convolution-capture.c'))
    (out/'input-pattern.patch').write_text(patch)
    cases = []
    matrix = [0.] * 25; matrix[12] = 3e38
    for components in (1,3):
        cases.append(dict(id=f'pdb-c{components}-3x3-extreme-center',width=3,height=3,components=components,
            alpha_weight=0,border=0,selection=0,live=0,final_selection=0,zero_alpha=0,
            matrix=matrix,divisor=3e38,offset=0.,channels=[1]*5,expected_status=3))
    capture.write_header(work,cases)
    (out/'capture-plan.json').write_text(json.dumps(cases,indent=2)+'\n')
    capture.ROOT = temporary_root
    started = datetime.now(timezone.utc).isoformat()
    executable, commands = capture.build(source,work,out)
    code, command = capture.run(executable,prefix,out,120)
    errors, count = capture.package(out,cases,code)
    fixtures = json.loads((out/'fixtures.json').read_text())
    pixels = (out/'pixels.bin').read_bytes()
    observations=[]
    for case in fixtures['cases']:
        def data(name):
            entry=case['buffers'][name]
            return pixels[entry['offset']:entry['offset']+entry['length']]
        initial, shadow, final = data('input'),data('merge1-shadow'),data('output')
        expected=bytes((x+y+c)%2 for y in range(3) for x in range(3) for c in range(case['components']))
        same = initial == shadow == final == expected
        if not same: errors.append('Non-identity result: '+case['id'])
        observations.append(dict(id=case['id'],input=list(initial),shadow=list(shadow),output=list(final),
                                 exact_identity=same,status=case['status']))
    (out/'build-commands.json.gz').write_bytes(gzip.compress(json.dumps(commands,indent=2).encode(),mtime=0))
    for name in ('build.log','capture.log'):
        path=out/name
        (out/(name+'.gz')).write_bytes(gzip.compress(path.read_bytes(),mtime=0));path.unlink()
    report=dict(schema_version=1,status='passed' if not errors else 'failed',errors=errors,
        started_utc=started,finished_utc=datetime.now(timezone.utc).isoformat(),source_commit=commit,
        source_sha256=source_hashes,case_count=count,command=command,exit_code=code,
        observations=observations,
        interpretation='The real old PDB returns exact input identity for 0/1 Gray and RGB pixels when center coefficient=divisor=3e38. The coefficient*255 worst-case bound overflows float, but no captured pixel reaches that bound.',
        original_harness_sha256=capture.sha(harness),adapted_harness_sha256=capture.sha(temporary_harness),
        capture_script_sha256=capture.sha(Path(__file__)),
        build_helper_sha256=capture.sha(ROOT/'tools/capture_legacy_convolution.py'),
        executable_sha256={str(p):capture.sha(p) for p in (work/'.libs/legacy-convolution',prefix/'bin/gimp-2.8',prefix/'lib/gimp/2.0/plug-ins/convolution-matrix')},
        files_sha256={p.name:capture.sha(p) for p in sorted(out.iterdir()) if p.is_file()})
    (out/'capture-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['status'],count,errors)
    return 0 if not errors else 1

if __name__ == '__main__': raise SystemExit(main())
