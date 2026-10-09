#!/usr/bin/env python3
"""Reproduce the one pinned mutex-source assignment addition; not a full-tree audit."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tools'))
from assign_legacy_hunks import tsv,task_catalog
from legacy_assignment_rules import route
import audit_legacy_granularity as granularity
BASE='d7008b23c6fe7cb28efb83648dc3fe351d4d3ae5'
SID='01.002/000047'
SOURCE='app/base/glib-cxx-utils.hpp'
ADDED='legacy-d9601d797e1afecd978e'
COMPLETED={ADDED,'legacy-2e934e579dea6c8eafd8'}
INV=ROOT/'migration/inventory'
def digest(data):return hashlib.sha256(data).hexdigest()
def prior(name):return subprocess.check_output(['git','show',BASE+':migration/inventory/'+name],cwd=ROOT)
def rows(data):return list(csv.DictReader(io.StringIO(data.decode()),delimiter='\t'))
def reproduce():
 names=['changed-hunks.tsv','hunk-wbs.tsv','cleanup-candidate-review.tsv','legacy-port-work-items.tsv']
 tables={n:rows(prior(n)) for n in names};old={r['work_id']:r for r in tables[names[3]]}
 assignment=next(r for r in tables[names[1]] if r['hunk_id']==SID)
 hunk=next(r for r in tables[names[0]] if r['child_id']==SID)
 assert assignment['path']==hunk['path']==SOURCE and assignment['base_blob']==''
 with tarfile.open(Path(__file__).parent/'evidence.tar.gz') as archive:source=archive.extractfile('legacy/glib-cxx-utils.hpp').read()
 blob=hashlib.sha1(b'blob '+str(len(source)).encode()+b'\0'+source).hexdigest();payload=b''.join(b'+'+line for line in source.splitlines(keepends=True))
 assert blob==assignment['source_blob']=='a1a77acd0ebcba4ad5f400bfe362628259788922'
 assert digest(payload)==assignment['payload_sha256'] and len(source.splitlines())==1228 and hunk['old_new_lines']=='-0,0 -> 1,1228'
 assert b'class synchronized : public ScopeGuard<GMutex*' in source
 result=route(hunk,source.decode().splitlines(),[]);previous=assignment['implementation_tasks'].split(',')
 assert result['profile']=='cpp-values' and set(result['tasks'])-set(previous)=={'06.022'} and set(previous)-set(result['tasks'])==set()
 assert result['tests']==assignment['verification_tasks'].split(',')
 hunk.update(feature_or_base=result['feature'],disposition_task=','.join(result['tasks']))
 assignment.update(implementation_tasks=','.join(result['tasks']),reason=result['reason'])
 candidate=next(r for r in tables[names[2]] if r['path']==SOURCE);candidate['implementation_tasks']=','.join(result['tasks'])
 outputs={n:tsv(tuple(tables[n][0]),tables[n]).encode() for n in names[:3]}
 with tempfile.TemporaryDirectory(prefix='painter-mutex-routing-') as directory:
  target=Path(directory);(target/'hunk-wbs.tsv').write_bytes(outputs['hunk-wbs.tsv']);(target/'asset-wbs.tsv').write_bytes(prior('asset-wbs.tsv'));expected,checks=granularity.specification(task_catalog(),target)
 assert len(old)==22942 and len(expected)==22943
 assert {r['work_id'] for r in expected}-set(old)=={ADDED} and set(old)-{r['work_id'] for r in expected}==set()
 for row in expected:
  if row['work_id']==ADDED:
   assert row['source_id']==SID and row['wbs_task']=='06.022' and row['status']=='TODO'
  else:
   before=old[row['work_id']];assert all(row[k]==before[k] for k in granularity.FIELDS if k not in granularity.MUTABLE);row.update({k:before[k] for k in granularity.MUTABLE});assert row==before
 outputs[names[3]]=tsv(granularity.FIELDS,expected).encode();outputs['granularity-review.tsv']=tsv(granularity.AUDIT_FIELDS,checks).encode()
 summary=json.loads(prior('wbs-assignment-summary.json'));summary['hunk_ledger_sha256']=digest(outputs['hunk-wbs.tsv']);outputs['wbs-assignment-summary.json']=(json.dumps(summary,indent=2)+'\n').encode()
 summary=json.loads(prior('changed-hunks.json'));summary['inventory_sha256']=digest(outputs['changed-hunks.tsv']);outputs['changed-hunks.json']=(json.dumps(summary,indent=2)+'\n').encode()
 summary=json.loads(prior('granularity-summary.json'));summary['implementation_actions']+=1;outputs['granularity-summary.json']=(json.dumps(summary,indent=2)+'\n').encode()
 for name in ['changed-files.tsv','asset-wbs.tsv','auxiliary-script-review.tsv','auxiliary-generator-relations.tsv']:assert (INV/name).read_bytes()==prior(name),'Unrelated inventory changed: '+name
 report={'baseline':BASE,'source':SOURCE,'hunk':SID,'source_blob':blob,'payload_sha256':digest(payload),'added_todo_work_id':ADDED,'removed_work_ids':[],'preserved_work_items':len(old),'done_before_acceptance':sum(x['status']=='DONE' for x in expected),'assignment_addition_added_done':0,'full_historical_generator_pass':False,'limitation':'Only the byte-verified added file is rerouted; absent complete historical trees are not claimed regenerated.','table_sha256_before_acceptance':{n:digest(data) for n,data in outputs.items()}}
 return outputs,report
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--write',action='store_true');args=parser.parse_args();outputs,report=reproduce()
 if args.write:
  for name,data in outputs.items():assert (INV/name).read_bytes() in (prior(name),data),'Refusing to overwrite later work: '+name
  for name,data in outputs.items():(INV/name).write_bytes(data)
 else:
  for name,data in outputs.items():
   actual=(INV/name).read_bytes()
   if name=='legacy-port-work-items.tsv':
    expected_rows,actual_rows=rows(data),rows(actual);assert len(expected_rows)==len(actual_rows)
    for old,new in zip(expected_rows,actual_rows):
     assert old['work_id']==new['work_id']
     for key in granularity.FIELDS:
      if old['work_id'] in COMPLETED and key in granularity.MUTABLE:continue
      assert old[key]==new[key],(old['work_id'],key)
   else:assert actual==data,'Routing output differs: '+name
 print(json.dumps(report,indent=2))
if __name__=='__main__':main()
