#!/usr/bin/env python3
"""Validate original write criterion, actual producer evidence, and retained duties."""
import csv,json,pathlib,re
from check_drawable_buffer_read_resolution import load,require
ROOT=pathlib.Path(__file__).resolve().parents[1];MANIFEST=ROOT/'migration/contracts/drawable-buffer-write.json'
def validate(m,args=None):
 _,inventory,tasks,progress=load() if args is None else args
 require(m['task']=='09.004' and m['criterion_status']=='DONE','Wrong write criterion')
 require(m['original_action']=='DrawableFeature の buffer write を実装する' and m['original_acceptance']=='範囲外に書かず dirty 領域を返す','Original write criterion changed')
 require(m['dependencies']==['09.003'] and progress['09.003']['status']=='DONE','Read dependency not complete')
 require(tasks['09.004']==['[x]',m['original_action'],'09.003',m['original_acceptance']] and progress['09.004']['status']=='DONE','Write ledger mismatch')
 expected={k:r for k,r in inventory.items() if r['wbs_task']=='09.004'};rows=m['duties']
 require(len(rows)==14 and len({r['work_id'] for r in rows})==14 and set(expected)=={r['work_id'] for r in rows},'Missing/duplicate duty')
 read={d['work_id']:d for d in json.loads((ROOT/m['related_read_resolution']).read_text())['duties']}
 for d in rows:
  orig=expected[d['work_id']];related=read[d['related_read_work_id']]
  for key in ('source_id','path','source_blob','source_range','source_sha256'):
   require(d[key]==orig[key]==related[key],'Changed source identity: '+key)
  require(d['source_scope']==orig['source_scope'] and d['historical_source_duty']==related['source_duty'],'Changed source duty')
  require(d['inherited_09_004_obligation']==related['successor_obligations']['09.004'],'Dropped inherited read duty')
  require(d['whole_hunk_checkpoint_status']=='TODO','Checkpoint hunk completion not proven')
  owners=d['successor_obligations'];path=d['path']
  needed={'31.017'} if path.endswith('pixel-processor.c') else {'31.017','09.015'} if path.endswith('pixel.hpp') else {'09.005','31.017'} if 'temp-buf' in path else {'09.005','09.006','09.007','09.008','09.009','09.010','09.011','09.012','09.003/region-lifetime'}
  require(needed<=set(owners) and '09.004' not in owners,'Missing successor or self dependency')
  for owner,text in owners.items():
   require(owner in tasks and owner in progress and bool(text.strip()),'Unknown owner or missing obligation')
   require('drawable-buffer-write.json#'+owner in progress[owner]['commit_or_artifact'],'Missing reverse owner link')
   if orig['status']=='DONE':
    record=progress[owner];require(record['status']=='DONE' and tasks[owner][0]=='[x]','Premature whole-hunk completion')
    require(all(record[k].strip() not in ('','-','pending','tasks.md') for k in ('commit_or_artifact','test','result')),'Missing successor completion evidence')
    require(record['test']!='WBS/progress ID reconciliation' and record['result']!='Existing unchecked task recorded explicitly as TODO','Placeholder completion evidence')
    require(any(x.strip() not in ('','-','tasks.md') and 'drawable-buffer-' not in x for x in record['commit_or_artifact'].split(';')),'Cross-link only completion evidence')
  require(orig['status'] in ('TODO','DOING','BLOCKED','DONE'),'Unknown inventory state')
  require(len(d['write_criterion_evidence'])==5,'Missing executable write coverage')
 require(all((ROOT/x).is_file() for x in m['write_evidence']),'Missing write evidence')
 return len(rows)
if __name__=='__main__':print(f'{validate(json.loads(MANIFEST.read_text()))} unchanged source-duty identities; original write criterion and surviving owners verified')
