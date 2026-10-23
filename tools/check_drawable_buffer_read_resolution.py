#!/usr/bin/env python3
"""Keep criterion completion separate from 48 unfinished historical source duties."""
import csv,json,pathlib,re
ROOT=pathlib.Path(__file__).resolve().parents[1]
MANIFEST=ROOT/'migration/contracts/drawable-buffer-read-resolution.json'
def require(ok,message):
 if not ok:raise ValueError(message)
def validate(current, historical, inventory, tasks, progress):
 require(current['task']=='09.003' and current['criterion_status']=='DONE','Wrong criterion')
 require(current['whole_hunk_status']=='TODO','Whole-hunk completion not proven')
 require(current['original_action']=='DrawableFeature の buffer read を実装する','Original action changed')
 require(current['original_acceptance']=='負の offset と部分領域を読める','Original acceptance changed')
 require(current['dependencies']==['09.002'] and progress['09.002']['status']=='DONE','Read dependency not met')
 require(tasks['09.003'][1:]==[current['original_action'],'09.002',current['original_acceptance']],'Original WBS meaning changed')
 require(tasks['09.003'][0]=='[x]' and progress['09.003']['status']=='DONE','Criterion ledger mismatch')
 expected={d['work_id']:d for d in historical['duties']};rows=current['duties']
 require(len(rows)==48 and len({r['work_id'] for r in rows})==48 and set(expected)=={r['work_id'] for r in rows},'Missing/duplicate source duty')
 for r in rows:
  d=expected[r['work_id']];inv=inventory[r['work_id']]
  for k in ('source_id','path','source_blob','source_range','source_sha256'):
   require(r[k]==d[k]==inv[k],'Changed source identity: '+k)
  require(r['source_duty']==d['source_duty'],'Historical duty weakened')
  require(inv['wbs_task']=='09.003' and r['whole_hunk_status']=='TODO','Checkpoint whole hunk changed or inventory reassigned')
  require(inv['status'] in ('TODO','DOING','BLOCKED','DONE'),'Unknown current inventory status')
  require(r['historical_remaining_tasks']==d['remaining_tasks'],'Historical successors changed')
  owners=r['successor_obligations']
  require(set(d['remaining_tasks'])-{'09.003'} <= set(owners),'Dropped preexisting successor')
  require(owners and '09.003' not in owners,'Read criterion used as unresolved self-dependency')
  if 'temp-buf' in d['path']:require('31.017' in owners,'Missing downstream TempBuf consumer owner')
  if d['path'].endswith('drawablefeature.hpp'):
   require({'09.004','09.005','09.006','09.007','09.008','09.009','09.011','09.012','09.003/region-lifetime'}<=set(owners),'Dropped DrawableFeature behavior')
  for owner,obligation in owners.items():
   require(owner in tasks and owner in progress and bool(obligation.strip()),'Missing owner task or obligation')
   require('drawable-buffer-read-resolution' in progress[owner]['commit_or_artifact'],'Owner has no reverse cross-link')
  if inv['status']=='DONE':
   # The manifest preserves the read-checkpoint TODO; current inventory may
   # advance only after every surviving obligation has evidenced completion.
   for owner in owners:
    record=progress[owner]
    require(record['status']=='DONE' and tasks[owner][0]=='[x]','Premature whole-hunk completion')
    for key in ('commit_or_artifact','test','result'):
     require(record[key].strip() not in ('','-','pending','tasks.md'),'Missing successor completion evidence: '+key)
    require(any(part.strip() not in ('','-','tasks.md') and 'drawable-buffer-read-resolution' not in part for part in record['commit_or_artifact'].split(';')),'Cross-link alone is not completion evidence')
    require(record['test']!='WBS/progress ID reconciliation' and record['result']!='Existing unchecked task recorded explicitly as TODO','Placeholder is not completion evidence')
  require(r['read_proof'].endswith('#'+r['work_id']),'Missing per-duty read evidence')
 require(len(current['read_evidence'])==4 and all((ROOT/p).is_file() for p in current['read_evidence']),'Missing read proof')
 return len(rows)
def load():
 historical=json.loads((ROOT/'migration/contracts/drawable-buffer-read.json').read_text())
 def tsv(p,key):
  with (ROOT/p).open() as stream:return {r[key]:r for r in csv.DictReader(stream,delimiter='\t')}
 tasks={}
 for line in (ROOT/'tasks.md').read_text().splitlines():
  if line.startswith('| '):
   cells=[x.strip() for x in line.strip('|').split('|')]
   if len(cells)==5 and re.fullmatch(r'\d\d\.\d{3}(?:/[\w-]+)?',cells[0]):tasks[cells[0]]=cells[1:]
 return historical,tsv('migration/inventory/legacy-port-work-items.tsv','work_id'),tasks,tsv('migration/progress.tsv','id')
if __name__=='__main__':
 print(f'{validate(json.loads(MANIFEST.read_text()),*load())} unchanged source-duty identities; read checkpoint and current successor completion rules verified')
