#!/usr/bin/env python3
import pathlib,json,hashlib,tarfile,re,ast,csv,io,subprocess,importlib.util
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def check(p=P,commit=None):
 for n,h in json.loads((p/'manifest.json').read_text()).items():assert sha((p/n).read_bytes())==h,n
 review=json.loads((p/'source-duty-review.json').read_text());assert review['task']=='23.003'and review['criterion']=='旧 blending-output を再現'and review['dependency']=='23.002'and len(review['rows'])==52
 baseline=list(csv.DictReader(io.StringIO(subprocess.check_output(['git','show',review['baseline']+':migration/inventory/legacy-port-work-items.tsv'],cwd=R).decode()),delimiter='\t'))
 before={r['work_id']:r for r in baseline if r['wbs_task']=='23.003'};assert len(before)==52 and set(before)=={r['before']['work_id']for r in review['rows']}
 for row in review['rows']:
  a=row['before'];assert a==before[a['work_id']]and a['status']=='TODO'and a['acceptance']=='旧 blending-output を再現'
  assert row['verified_present_role']and row['current_route']and row['future_obligation']
  assert row['other_owners_at_review']==[x for x in baseline if x['source_id']==a['source_id']and x['work_id']!=a['work_id']]
  assert sha((R/'migration/tests/smudge-initial-sampling'/row['payload']).read_bytes())==a['source_sha256']
 current=list(csv.DictReader(io.StringIO(subprocess.check_output(['git','show',(commit or 'HEAD')+':migration/inventory/legacy-port-work-items.tsv'],cwd=R).decode()),delimiter='\t'));lookup={x['work_id']:x for x in current}
 for row in review['rows']:
  a,b=row['before'],row['after'];assert b['status']=='DONE'
  assert {k:v for k,v in a.items()if k not in ['status','owner','artifact','test','result','limitations']}=={k:v for k,v in b.items()if k not in ['status','owner','artifact','test','result','limitations']}
  assert lookup[a['work_id']]in([b]if commit else[a,b])
  assert {(x['work_id'],x['wbs_task'])for x in current if x['source_id']==a['source_id']and x['work_id']!=a['work_id']}=={(x['work_id'],x['wbs_task'])for x in row['other_owners_at_review']}
  for other in row['other_owners_at_review']:
   for k in ['source_id','source_sha256','action','acceptance','wbs_task']:assert lookup[other['work_id']][k]==other[k]
 progress={x['id']:x for x in csv.DictReader(io.StringIO(subprocess.check_output(['git','show',(commit or 'HEAD')+':migration/progress.tsv'],cwd=R).decode()),delimiter='\t')}
 assert progress['23.002']['status']=='DONE'
 if commit:assert progress['23.003']['status']=='DONE'
 seal=json.loads((p/'final/source-before-build.json').read_text());assert len(seal)==74 and seal==json.loads((p/'final/source-after-build.json').read_text())
 with tarfile.open(p/'final/tested-sources.tar.gz','r:gz')as t:
  assert set(t.getnames())==set(seal);src={n:t.extractfile(n).read()for n in seal}
  for n,h in seal.items():assert sha(src[n])==h,n
 prod='app/paint/gimppaintersmudge.cpp';assert sha((R/prod).read_bytes())==seal[prod]
 assert hashlib.sha1(b'blob '+str(len(src[prod])).encode()+b'\0'+src[prod]).hexdigest()=='40c57e21ae07ffb4cc2500977e4033b983750d59'
 held=json.loads((p/'final/held-source-isolation.json').read_text())['rows'];assert len(held)==10
 for row in held:
  if row.get('native_exists')is False:assert row['path']not in seal
  else:assert seal[row['path']]==row['sha256']
 assert next(x for x in held if x['path'].endswith('gimp-painter-options.cpp'))['git_blob']=='12fd73dc406128bc94167ee3444caa05cbdd9a9d'
 pairs=[('flow-alias','const double blending = gimp_dynamics_get_linear_value (brush->dynamics, GIMP_DYNAMICS_OUTPUT_BLENDING,','const double blending = gimp_dynamics_get_linear_value (brush->dynamics, GIMP_DYNAMICS_OUTPUT_FLOW,',set(i for i in range(48)if(i//2)%2)),('shade-feedback','    pixels = shaded.data ();','    gegl_buffer_set (state.accumulator.get (), &accum_area, 0, native, shaded.data (), GEGL_AUTO_ROWSTRIDE);\n    pixels = shaded.data ();',{6,7,10,11,14,15,18,19,22,23,30,31,34,35,38,39,42,43,46,47})]
 for name,old,new,affected in pairs:
  q=p/'final'/name;j=json.loads((q/'result.json').read_text());assert j['result']=='FAIL'and j['returncode']!=0 and j['duration']<60 and 'Legacy Smudge full stroke mismatch'in j['stderr']
  match=re.search(r'Mismatched finish cases/count: (.*)',j['stdout']);assert match
  results=ast.literal_eval(match[1]);assert {i for i,count in results}==affected and all(count>0 for i,count in results)
  mutation=(q/'mutated.cpp').read_bytes();assert src[prod].count(old.encode())==1 and mutation==src[prod].replace(old.encode(),new.encode())
  pins=json.loads((q/'source-before-build.json').read_text());assert set(pins)==set(seal)and {n for n in seal if pins[n]!=seal[n]}=={prod}and pins[prod]==sha(mutation)
  assert re.fullmatch('[a-f0-9]{64}',j['executable_sha256'])
 normal=json.loads((p/'final/normal.json').read_text());assert len(normal)==4 and all(r['result']=='OK'and r['returncode']==0 and r['duration']<60 for r in normal)
 def native(out):assert len(re.findall(r'^ok \d+ /',out,re.M))==26 and 'not ok'not in out and '# SKIP'not in out and 'alpha matrix192 scenes; actual updated centers=1008'in out
 native(next(x['stdout']for x in normal if x['name'].endswith('/ gimp-painter-smudge')))
 for x in normal:
  if x['name'].endswith('-strokes'):assert '2952800 bytes, 193 records'in x['stdout']
  if x['name'].endswith('-pixels'):assert '1024'in x['stdout']and '891487'in x['stdout']
 assert '1080x1920x24, authenticated accepted, empty authority rejected'in(p/'run.log').read_text()
 ex=json.loads((p/'final/normal-executables.json').read_text());assert len(ex)==3 and all(re.fullmatch('[a-f0-9]{64}',h)for h in ex.values())
 san=json.loads((p/'final/focused-native.json').read_text());assert san['scope'].startswith('18 focused native units')and len(san['sources_sha256'])==48 and not san['changed_during_run']
 for n,h in san['sources_sha256'].items():assert seal[n]==h,n
 assert len(san['runs'])==5 and all(x['exit_code']==0 for x in san['runs']);native(san['runs'][0]['stdout'])
 assert {tuple(x['args'])for x in san['runs'][1:]}=={(),('owned',),('paper',),('paper','owned')}
 for x in san['runs'][1:]:assert '2952800 bytes,193 records'in x['stdout']
 assert all('-fno-sanitize'not in' '.join(c)for c in san['commands'])and sum('-fsanitize=address,undefined,float-cast-overflow'in c for c in san['commands'])==20
 # Fresh execution here does not replace immutable earlier model/kernel proofs.
 q=R/'migration/tests/smudge-accumulator-closure/check-closure.py';spec=importlib.util.spec_from_file_location('accumulator_closure',q);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check(commit=commit or 'HEAD')
 print('PASS:23.003 native Flow alias24/shade feedback20 causal failures;exact restored26core+48old routes+1024kernels;18-unit sanitizer26+four48routes;52 source roles retained')
if __name__=='__main__':
 import argparse
 a=argparse.ArgumentParser();a.add_argument('--commit');args=a.parse_args();check(commit=args.commit)
