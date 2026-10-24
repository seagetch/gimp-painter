#!/usr/bin/env python3
import pathlib,json,hashlib,csv,importlib.util,sys,subprocess,io
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def check(preflight=False,commit=None):
 def read(n):return subprocess.check_output(['git','show',commit+':'+n],cwd=R) if commit else (R/n).read_bytes()
 for n,h in json.loads((P/'manifest.json').read_text()).items():assert sha((P/n).read_bytes())==h,n
 j=json.loads((P/'rows.json').read_text());assert j['criterion']=='初期採色と更新が一致'and j['dependency']=='23.001'and len(j['rows'])==52
 current={r['work_id']:r for r in csv.DictReader(io.StringIO(read('migration/inventory/legacy-port-work-items.tsv').decode()),delimiter='\t')}
 original=json.loads((R/'migration/tests/smudge-alpha-centers/audit.json').read_text())['rows'];old={x['work_id']:x for x in original}
 assert set(old)=={r['before']['work_id']for r in j['rows']}
 mutable={'status','owner','artifact','test','result','limitations'}
 for row in j['rows']:
  a,b=row['before'],row['after'];assert a['status']=='TODO'and b['status']=='DONE'and a['wbs_task']==b['wbs_task']=='23.002'
  assert {k:v for k,v in a.items()if k not in mutable}=={k:v for k,v in b.items()if k not in mutable}
  assert a=={k:v for k,v in old[a['work_id']].items()if k not in ['payload','other_whole_hunk_owners']}
  assert current[a['work_id']] in ([a,b]if preflight else[b])
  assert sha((R/'migration/tests/smudge-initial-sampling'/row['payload']).read_bytes())==a['source_sha256']
  assert row['verified_present_role'] and row['future_obligation'] and row['current_route']
  expected={(x['work_id'],x['task'])for x in old[a['work_id']]['other_whole_hunk_owners']}
  assert {(x['work_id'],x['wbs_task'])for x in row['other_owners_at_review']}==expected
  assert {(x['work_id'],x['wbs_task'])for x in current.values()if x['source_id']==a['source_id']and x['work_id']!=a['work_id']}==expected
  for owner in row['other_owners_at_review']:
   for k in ['source_id','source_sha256','wbs_task','action','acceptance']:assert current[owner['work_id']][k]==owner[k]
   if owner['wbs_task']in['23.003','23.004','23.005']:assert owner['status']=='TODO' # immutable state at closure, not a forever-TODO assertion
 progress_text=subprocess.check_output(['git','show','HEAD:migration/progress.tsv'],cwd=R).decode() if preflight else read('migration/progress.tsv').decode()
 progress={r['id']:r for r in csv.DictReader(io.StringIO(progress_text),delimiter='\t')}
 assert progress['23.001']['status']=='DONE'
 if not preflight:assert progress['23.002']['status']=='DONE'
 for n,h in json.loads((P/'current-source-pins.json').read_text()).items():assert sha(read(n))==h,n
 for name,fn in [('smudge-initial-sampling','check-initial-sampling.py'),('smudge-mask-format','check-mask-format.py'),('smudge-alpha-centers','check-alpha-centers.py'),('smudge-dynamics-current','check-current-dynamics.py')]:
  path=R/'migration/tests'/name/fn;spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check()
 print('PASS:52 original23.002 roles, unchanged criterion/dependency, exact ledger successor; cross-owners retained; four final-source evidence gates replayed (no new runtime)')
if __name__=='__main__':
 import argparse
 parser=argparse.ArgumentParser();parser.add_argument('--preflight',action='store_true');parser.add_argument('--commit');args=parser.parse_args();check(args.preflight,args.commit)
