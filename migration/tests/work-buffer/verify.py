#!/usr/bin/env python3
"""Verify current work-buffer checkpoint sources and unedited runtime records."""
import hashlib,json,pathlib
HERE=pathlib.Path(__file__).resolve().parent;ROOT=HERE.parents[2]
m=json.loads((HERE/'report.json').read_text());assert m['task']=='09.009' and m['native_tree_hashes_match']
for p,h in m['source_sha256'].items():assert hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==h,p
for p,h in m['evidence_sha256'].items():assert hashlib.sha256((HERE/p).read_bytes()).hexdigest()==h,p
runs=json.loads((HERE/'registered-native.json').read_text());assert len(runs)==4
assert all(r['result']=='OK' and r['returncode']==0 for r in runs)
assert sum(r['stdout'].count('\nok ') for r in runs)==33
standalone=json.loads((HERE/'standalone.json').read_text());assert len(standalone['runs'])==2
assert {r['sanitized'] for r in standalone['runs']}=={False,True}
assert all(r['exit_code']==0 and r['stdout'].count('\nok ')==5 for r in standalone['runs'])
for p,h in standalone['source_sha256'].items():assert hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==h,p
mutants=json.loads((HERE/'mutation-results.json').read_text());assert len(mutants)==2 and all(r['exit_code']!=0 for r in mutants)
s=(ROOT/'app/paint/painter-mypaint-surface/gegl-surface.cpp').read_text();start=s.index('Dab coverage (');end=s.index('bool draw_wide(',start);coverage=s[start:end]
assert coverage.index('WorkBudget::require (w, h, transformed.pixels.size ())')<coverage.index('result.mask.resize')
assert coverage.index('if (selection && !BufferRead::intersect')<coverage.index('WorkBudget::require')
assert 'std::length_error' in (ROOT/'app/paint/painter-mypaint-surface/work-budget.hpp').read_text()
print('33 native TAP,5 normal+focused sanitizer groups,2 production-helper controls and exact source/evidence hashes PASS')
