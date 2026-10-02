#!/usr/bin/env python3
"""Expand every source assignment into independently closable WBS actions.

Immutable source/task identity is reproducible. Mutable execution status and
evidence are preserved on regeneration, and can never be inferred from an
inventory DONE state. --require-closed is the release gate, not the audit gate.
"""
import argparse
import csv
import hashlib
import json
from collections import Counter
from pathlib import Path
from assign_legacy_hunks import ROOT,INV,read_tsv,tsv,task_catalog,TASK_ID,sha

FIELDS=('work_id','source_kind','source_id','path','source_blob','source_range',
        'source_sha256','source_scope','phase','wbs_task','action','acceptance',
        'status','owner','artifact','test','result','limitations')
MUTABLE=('status','owner','artifact','test','result','limitations')
AUDIT_FIELDS=('child_id','status','source_kind','source_id','path','source_range',
              'implementation_actions','verification_actions','work_ids','result')


def specification(catalog=None, inventory=INV):
    catalog=task_catalog() if catalog is None else catalog;expected=[];checks=[]
    for kind,filename in [('hunk','hunk-wbs.tsv'),('asset','asset-wbs.tsv')]:
        for source in read_tsv(inventory/filename):
            sid=source['hunk_id'] if kind=='hunk' else source['child_id']
            source_digest=source['payload_sha256'] if kind=='hunk' else sha((source['path']+'\0'+source['source_blob']+'\0'+source['base_blob']).encode())
            scope=source['source_scope'] if kind=='hunk' else 'asset identity, contents, resource relationship and installation'
            source_range=source['old_new_lines'] if kind=='hunk' else 'whole asset/manifest'
            source_work=[];counts={}
            for phase,key in [('implementation','implementation_tasks'),('verification','verification_tasks')]:
                ids=source[key].split(',')
                if len(ids)!=len(set(ids)) or any(not TASK_ID.fullmatch(i) or i not in catalog for i in ids):
                    raise ValueError(f'{sid}: non-concrete or duplicate WBS assignment')
                if not ids:raise ValueError(f'{sid}: no {phase} action')
                counts[phase]=len(ids)
                for task in ids:
                    # Hash only the stable source/phase/task key, not row order.
                    wid='legacy-'+hashlib.sha256((kind+'\0'+sid+'\0'+phase+'\0'+task).encode()).hexdigest()[:20]
                    row=dict(work_id=wid,source_kind=kind,source_id=sid,path=source['path'],
                        source_blob=source['source_blob'],source_range=source_range,source_sha256=source_digest,
                        source_scope=scope,phase=phase,wbs_task=task,action=catalog[task]['title'],
                        acceptance=catalog[task]['acceptance'],status='TODO',owner='',artifact='',test='',result='',
                        limitations='Assigned only; requires this source-specific action and evidence')
                    expected.append(row);source_work.append(wid)
            checks.append(dict(child_id=f'01.017/{kind}-{len(checks)+1:06d}',status='DONE',source_kind=kind,source_id=sid,path=source['path'],source_range=source_range,implementation_actions=str(counts['implementation']),verification_actions=str(counts['verification']),work_ids=','.join(source_work),result='Every assigned action expanded separately; execution remains in work-item status/evidence'))
    if len({r['work_id'] for r in expected})!=len(expected):raise ValueError('Work ID collision')
    if len(checks)!=2489+402:raise ValueError('Source granularity coverage changed')
    return expected,checks


def validate(expected,actual,require_closed=False):
    errors=[];wanted={r['work_id']:r for r in expected};found={}
    for r in actual:
        wid=r.get('work_id','')
        if wid in found:errors.append('duplicate work item: '+wid)
        found[wid]=r
        if wid not in wanted:errors.append('unassigned work item: '+wid);continue
        for key in FIELDS:
            if key not in MUTABLE and r.get(key)!=wanted[wid][key]:errors.append(wid+': changed source/action contract '+key)
        if r.get('status') not in ('TODO','DOING','BLOCKED','DONE'):errors.append(wid+': invalid state')
        if r.get('status')=='DONE' and any(not r.get(k,'').strip() or r[k].strip()=='-' for k in ('owner','artifact','test','result')):
            errors.append(wid+': DONE requires owner, artifact, test and result')
        if require_closed and r.get('status')!='DONE':errors.append(wid+': release has unfinished '+r.get('status','missing'))
    for missing in sorted(wanted.keys()-found.keys()):errors.append('missing work item: '+missing)
    return errors


def render(check=False,require_closed=False):
    expected,checks=specification();work_path=INV/'legacy-port-work-items.tsv';check_path=INV/'granularity-review.tsv'
    actual=read_tsv(work_path) if work_path.exists() else []
    if check:
        errors=validate(expected,actual,require_closed)
        if not check_path.exists() or check_path.read_text()!=tsv(AUDIT_FIELDS,checks):errors.append('granularity checklist drift')
        if errors:raise ValueError('\n'.join(errors[:30])+('\n... further errors omitted' if len(errors)>30 else ''))
    else:
        previous={r['work_id']:r for r in actual}
        for r in expected:
            old=previous.get(r['work_id'])
            if old:
                # Never carry completion across changed source contracts.
                if any(old.get(k)!=r[k] for k in FIELDS if k not in MUTABLE):
                    if old.get('status')!='TODO':raise ValueError('Review changed active/completed work before regeneration: '+r['work_id'])
                else:
                    for k in MUTABLE:r[k]=old[k]
        stale=set(previous)-{r['work_id'] for r in expected}
        if any(previous[k]['status']!='TODO' for k in stale):raise ValueError('Regeneration would discard execution evidence')
        actual=expected
        work_path.write_text(tsv(FIELDS,actual));check_path.write_text(tsv(AUDIT_FIELDS,checks))
    summary=dict(source_checks=len(checks),hunks=sum(r['source_kind']=='hunk' for r in checks),assets=sum(r['source_kind']=='asset' for r in checks),implementation_actions=sum(r['phase']=='implementation' for r in expected),verification_actions=sum(r['phase']=='verification' for r in expected),statuses=dict(sorted(Counter(r['status'] for r in actual).items())),unassigned_sources=0,compound_action_rows=0)
    summary_path=INV/'granularity-summary.json'
    # Status totals are dynamic and are not treated as frozen execution truth.
    immutable={k:v for k,v in summary.items() if k!='statuses'}
    text=json.dumps(immutable,indent=2)+'\n'
    if check:
        if not summary_path.exists() or summary_path.read_text()!=text:raise ValueError('Granularity summary drift')
    else:summary_path.write_text(text)
    return summary


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--check',action='store_true');ap.add_argument('--require-closed',action='store_true');a=ap.parse_args()
    try:summary=render(a.check or a.require_closed,a.require_closed)
    except ValueError as e:raise SystemExit(str(e))
    print(json.dumps(summary,sort_keys=True))
if __name__=='__main__':main()
