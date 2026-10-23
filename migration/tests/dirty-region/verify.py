#!/usr/bin/env python3
"""Verify current dirty-region checkpoint sources and unedited runtime records."""
import hashlib,json,pathlib
HERE=pathlib.Path(__file__).resolve().parent;ROOT=HERE.parents[2]
m=json.loads((HERE/'report.json').read_text());assert m['task']=='09.010' and m['native_tree_hashes_match']
for p,h in m['source_sha256'].items():assert hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==h,p
for p,h in m['evidence_sha256'].items():assert hashlib.sha256((HERE/p).read_bytes()).hexdigest()==h,p
runs=json.loads((HERE/'registered-native.json').read_text());assert len(runs)==4
assert all(r['result']=='OK' and r['returncode']==0 for r in runs)
assert sum(r['stdout'].count('\nok ') for r in runs)==34
standalone=json.loads((HERE/'standalone.json').read_text());assert len(standalone['runs'])==2
assert {r['sanitized'] for r in standalone['runs']}=={False,True}
assert all(r['exit_code']==0 and r['stdout'].count('\nok ')==7 for r in standalone['runs'])
for p,h in standalone['source_sha256'].items():assert hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==h,p
mutants=json.loads((HERE/'mutation-results.json').read_text());assert len(mutants)==2 and all(r['test_exit_code']!=0 and r['compile_exit_code']==0 and r['result']=='FAIL' for r in mutants)
extra=json.loads((HERE/'surface-mutation.json').read_text());assert len(extra)==1 and extra[0]['test_exit_code']!=0
repeats=json.loads((HERE/'repeat20.json').read_text());assert len(repeats)==3
assert all(r['result']=='OK' and r['tap_count']==20 and r['stdout'].count('\nok ')==20 for r in repeats)
assert len({r['binary_sha256'] for r in repeats})==1
original=[json.loads(l) for l in (HERE/'original-restored34.json').read_text().splitlines() if l.strip()]
assert sum(r['stdout'].count('\nok ') for r in original)==34
correction=json.loads((HERE/'collector-correction.json').read_text());assert correction['correct_tap_count']==34 and correction['runtime_or_stale_binary_defect'] is False
print('34 native TAP,7 normal+focused sanitizer groups,3 production controls and exact source/evidence hashes PASS')
