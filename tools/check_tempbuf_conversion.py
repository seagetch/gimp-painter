#!/usr/bin/env python3
"""Preserve 34 historical duties while verifying original TempBuf conversion scope."""
import hashlib,json,pathlib,subprocess
from check_drawable_buffer_read_resolution import load,require
ROOT=pathlib.Path(__file__).resolve().parents[1];MANIFEST=ROOT/'migration/contracts/tempbuf-conversion.json'
def validate(m,args=None):
 _,inventory,tasks,progress=load() if args is None else args
 require(m['task']=='09.005' and m['criterion_status']=='DONE','Wrong conversion criterion')
 require(m['whole_hunk_checkpoint_status']=='TODO','Whole-hunk completion not proven')
 require(m['original_action']=='TempBuf の変換を実装する' and m['original_acceptance']=='stride・format・所有権を保持','Original conversion criterion changed')
 require(m['dependencies']==['09.004'] and progress['09.004']['status']=='DONE','Write dependency not complete')
 require(tasks['09.005']==['[x]',m['original_action'],'09.004',m['original_acceptance']] and progress['09.005']['status']=='DONE','Conversion ledger mismatch')
 expected={k:r for k,r in inventory.items() if r['wbs_task']=='09.005'};rows=m['duties']
 require(len(rows)==34 and len({r['work_id'] for r in rows})==34 and set(expected)=={r['work_id'] for r in rows},'Missing/duplicate source duty')
 require(m['legacy_commit']=='afa43fae3e920210146abed514f136fd49f671b5','Wrong pinned legacy revision')
 ancestors={}
 for path in ('migration/contracts/drawable-buffer-read-resolution.json','migration/contracts/drawable-buffer-write.json'):
  ancestor=json.loads((ROOT/path).read_text());ancestors[ancestor['task']]=ancestor['duties']
 blobs={}
 for d in rows:
  orig=expected[d['work_id']]
  for key in ('source_id','path','source_blob','source_range','source_sha256','source_scope'):require(d[key]==orig[key],'Changed source identity: '+key)
  if d['path'] not in blobs:
   blob=subprocess.check_output(['git','rev-parse',m['legacy_commit']+':'+d['path']],cwd=ROOT,text=True).strip()
   data=subprocess.check_output(['git','show',m['legacy_commit']+':'+d['path']],cwd=ROOT);blobs[d['path']]=(blob,hashlib.sha256(data).hexdigest())
  require(blobs[d['path']]==(d['source_blob'],d['source_blob_sha256']),'Changed pinned blob')
  required_relations=[]
  for criterion,records in ancestors.items():
   for a in records:
    if (a['path'],a['source_range'],a['source_sha256'])==(d['path'],d['source_range'],d['source_sha256']):required_relations.append({'criterion':criterion,'work_id':a['work_id'],'inherited_09_005_obligation':a['successor_obligations'].get('09.005')})
  require(d['related_obligations']==required_relations,'Dropped or changed inherited obligation')
  require(d['whole_hunk_checkpoint_status']=='TODO' and d['historical_source_duty'].strip(),'Checkpoint completion or missing duty')
  require(len(d['criterion_evidence'])==6,'Missing conversion coverage')
  owners=d['successor_obligations'];needed={'31.017','09.015'} if d['path'].endswith('pixel.hpp') else {'31.017'}
  require(needed<=set(owners) and '09.005' not in owners,'Dropped successor or self dependency')
  require(orig['status'] in ('TODO','DOING','BLOCKED','DONE'),'Unknown current inventory status')
  for owner,text in owners.items():
   require(owner in tasks and owner in progress and text.strip(),'Unknown owner or missing obligation')
   record=progress[owner];require('tempbuf-conversion.json#'+owner in record['commit_or_artifact'],'Missing reverse cross-link')
   if orig['status']=='DONE':
    require(record['status']=='DONE' and tasks[owner][0]=='[x]','Premature whole-hunk completion')
    require(all(record[k].strip() not in ('','-','pending','tasks.md') for k in ('commit_or_artifact','test','result')),'Missing completion evidence')
    require(record['test']!='WBS/progress ID reconciliation' and record['result']!='Existing unchecked task recorded explicitly as TODO','Placeholder completion evidence')
    require(any(x.strip() not in ('','-','tasks.md') and '/contracts/' not in x for x in record['commit_or_artifact'].split(';')),'Cross-link alone is not completion evidence')
 require(all((ROOT/p).is_file() for p in m['evidence']),'Missing runtime evidence')
 return len(rows)
if __name__=='__main__':print(f'{validate(json.loads(MANIFEST.read_text()))} unchanged source identities; original TempBuf conversion criterion and successors verified')
