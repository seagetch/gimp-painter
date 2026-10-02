#!/usr/bin/env python3
"""Validate WBS foundation acceptance coverage and exact source/test evidence.

This does not toggle dependency-gated WBS checkboxes or rewrite old inventories.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
def validate(root=ROOT, matrix=None):
    root=Path(root);m=matrix if matrix is not None else json.loads((root/'migration/acceptance/foundation.json').read_text());errors=[]
    expected=set(re.findall(r'^\| (0[67]\.\d{3}(?:/[\w.-]+)?) \|', (root/'tasks.md').read_text(),re.M))
    task_deps={m[1]:m[2].strip() for m in re.finditer(r'^\| (0[67]\.\d{3}(?:/[\w.-]+)?) \| \[[ x]\] \| [^|]+ \| ([^|]+) \|', (root/'tasks.md').read_text(),re.M)}
    rows=m['rows'];ids=[r['id'] for r in rows]
    if len(ids)!=len(set(ids)) or set(ids)!=expected:errors.append('WBS06/07 coverage is incomplete or duplicated')
    for row in rows:
        name=row['id'];state=row['state']
        if row['dependencies']!=task_deps.get(name):errors.append(name+': stale WBS dependency snapshot')
        if state not in ('COMPONENT_VERIFIED','PARTIAL','OPEN') or not row['scope_or_gap']:errors.append(name+': missing state/gap')
        for src in row['implementation_paths']:
            if not (root/src).is_file():errors.append(name+': missing source '+src)
        if state!='COMPONENT_VERIFIED':continue
        if not row['test_ids'] or len(row['reports'])<2:errors.append(name+': missing native/sanitizer evidence');continue
        for report in row['reports']:
            raw=(root/report).read_text()
            try:data=json.loads(raw)
            except json.JSONDecodeError:data=[json.loads(line) for line in raw.splitlines()]
            if isinstance(data,list):
                matched=[r for r in data if r['name'].endswith('gimp-filter-layer')]
                if len(matched)!=1 or matched[0]['result']!='OK':errors.append(name+': native target failed')
                output=matched[0]['stdout'] if matched else ''
            else:
                if data.get('status','PASS')!='PASS' or data.get('exit_code',0)!=0 or any(data.get(key,[]) for key in ('changed_after_compile','changed_during_build','changed_during_run','changed_after_run')):errors.append(name+': failed/stale report '+report)
                output=data.get('stdout','')+'\n'+'\n'.join(data.get('output',[]))
                for src in row['implementation_paths']:
                    digest=data.get('source_sha256',{}).get(src)
                    if digest!=hashlib.sha256((root/src).read_bytes()).hexdigest():errors.append(name+': report/source mismatch '+src)
            for test in row['test_ids']:
                if not re.search(r'^ok \d+ '+re.escape(test)+r'$',output,re.M):errors.append(name+': test not passed '+test+' in '+report)
    return errors
if __name__=='__main__':
    errors=validate()
    if errors:raise SystemExit('\n'.join(errors))
    print('WBS06/07 component source/test acceptance coverage: OK; parent dependencies and release gates remain separate')
