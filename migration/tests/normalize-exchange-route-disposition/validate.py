#!/usr/bin/env python3
"""Current bounded disposition gate + immutable prior-context replay."""
from pathlib import Path
import csv,io,json,hashlib,ast,subprocess,copy,tempfile,shutil
P=Path(__file__).resolve().parent;R=P.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
def check(b,m):
 if not b:raise ValueError(m)
def rows(b):return list(csv.DictReader(io.StringIO(b.decode()),delimiter='\t'))
def routes(s):
 node=next(n for n in ast.parse(s).body if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='BOUNDED_CONTEXT_ROUTES'for t in n.targets));return ast.literal_eval(node.value)
def transition(before,after,j):
 check(len(before)==len(after)==108 and [x['procedure']for x in before]==[x['procedure']for x in after] and len({x['procedure']for x in after})==108,'procedure identities')
 for a,b in zip(before,after):
  name=a['procedure']
  if name not in j['bounded_fields']:check(a==b,'unrelated row')
  else:
   check({k for k in a if a[k]!=b[k]}=={'executor_route','missing_port_work','dependency_status'},'only three bounded fields')
   for k,v in j['bounded_fields'][name].items():check(b[k]==v,'bounded field or residual gate')
 check(sum('required'in x['executor_route']for x in after)==90,'required labels, not tasks')
def proof_pins(j):
 for name,h in j['proofs'].items():
  f=R/'migration/tests'/name/'artifact-sha256.json';check(f.is_file()and sha(f.read_bytes())==h,'missing/changed successor evidence')
def main():
 artifacts=json.loads((P/'artifact-sha256.json').read_text());check(set(artifacts)=={str(p.relative_to(P))for p in P.rglob('*')if p.is_file()and p.name!='artifact-sha256.json'and '__pycache__'not in p.parts},'artifact set')
 for n,h in artifacts.items():check(sha((P/n).read_bytes())==h,n)
 meta=R/'migration/tests/normalize-exchange-route-inventory.json';check(sha(meta.read_bytes())==(P/'metadata-sha256.txt').read_text().strip(),'metadata pin');j=json.loads(meta.read_text())
 check(set(j['bounded_fields'])=={'plug-in-normalize','plug-in-exchange'} and j['total_procedure_identities']==108 and j['remaining_required_labels']==90,'bounded scope')
 check(not any(j['claims'].values())and 'residual gates remain open'in j['status'],'no completion overclaim')
 b=(P/'before.tsv').read_bytes();a=(P/'after.tsv').read_bytes();check(sha(b)==j['old_routes_sha256']and sha(a)==j['new_routes_sha256'],'row snapshots');before,after=rows(b),rows(a);transition(before,after,j)
 remaining={}
 for row in after:
  if 'required'in row['executor_route']:remaining.setdefault(row['executor_route'],[]).append(row['procedure'])
 check(remaining==j['remaining_by_label'],'required labels')
 oldgen=(P/'generator-before.py').read_text();newgen=(P/'generator-after.py').read_text();check(sha(oldgen.encode())==j['generator_before_sha256']and sha(newgen.encode())==j['generator_after_sha256'],'generator pins')
 olddict,newdict=routes(oldgen),routes(newgen);check('plug-in-normalize'not in olddict,'new Normalize disposition')
 for n,v in olddict.items():
  if n!='plug-in-exchange':check(newdict[n]==v,'unrelated generator route')
 check(set(newdict)==set(olddict)|{'plug-in-normalize'},'generator key set')
 for n,v in j['bounded_fields'].items():check(newdict[n]==v,'literal generator agrees')
 # Removing the exact new literal block must recover every unrelated byte.
 start=newgen.index('    "plug-in-normalize": {');end=newgen.index('    "plug-in-red-eye-removal": {',start);x=oldgen.index('    "plug-in-exchange": {');y=oldgen.index('    "plug-in-red-eye-removal": {',x)
 check(newgen[:start]+newgen[end:]==oldgen[:x]+oldgen[y:],'unrelated generator code')
 check((R/'migration/inventory/filter-procedure-routes.tsv').read_bytes()==a and (R/'tools/inventory_filter_routes.py').read_text()==newgen,'current exact route/generator')
 proof_pins(j)
 for name in j['proofs']:subprocess.run(['python3','-B',str(R/'migration/tests'/name/'validate.py')],check=True,stdout=subprocess.DEVNULL)
 # Original validator is replayed UNMODIFIED in its original live-row context.
 # Do not pretend its historical native-only live assertion checks the new row.
 old=R/'migration/tests/color-exchange-route-disposition';h=j['historical_exchange'];oldmeta=R/'migration/tests/color-exchange-route-inventory.json'
 for f,key in [(old/'artifact-sha256.json','artifacts_sha256'),(old/'validate.py','validator_sha256'),(old/'after.tsv','row_context_sha256'),(old/'generator-after.py','generator_context_sha256'),(oldmeta,'metadata_sha256')]:check(sha(f.read_bytes())==h[key],'immutable historical pin')
 with tempfile.TemporaryDirectory(prefix='historical-filter-route-')as d:
  root=Path(d);(root/'.git').symlink_to(R/'.git');(root/'migration/tests').mkdir(parents=True);(root/'migration/inventory').mkdir();(root/'tools').mkdir()
  names=['color-exchange-route-disposition',*json.loads(oldmeta.read_text())['proofs']]
  for name in names:shutil.copytree(R/'migration/tests'/name,root/'migration/tests'/name)
  shutil.copy2(oldmeta,root/'migration/tests/color-exchange-route-inventory.json');shutil.copy2(old/'after.tsv',root/'migration/inventory/filter-procedure-routes.tsv');shutil.copy2(old/'generator-after.py',root/'tools/inventory_filter_routes.py')
  q=subprocess.run(['python3','-B',str(root/'migration/tests/color-exchange-route-disposition/validate.py')],capture_output=True,text=True);check(q.returncode==0,'historical replay: '+q.stdout+q.stderr)
 # Active controls mutate inputs, never weaken a validator or rewrite evidence.
 bads=[]
 m=copy.deepcopy(after);m.pop();bads.append(m)
 m=copy.deepcopy(after);m[0]['legacy_source']='unrelated.c';bads.append(m)
 m=copy.deepcopy(after);next(r for r in m if r['procedure']=='plug-in-normalize')['executor_route']='fully complete';bads.append(m)
 m=copy.deepcopy(after);next(r for r in m if r['procedure']=='plug-in-exchange')['argument_types']='changed';bads.append(m)
 m=copy.deepcopy(after);next(r for r in m if r['procedure']=='plug-in-normalize')['missing_port_work']='';bads.append(m)
 for m in bads:
  try:transition(before,m,j)
  except ValueError:pass
  else:raise ValueError('invalid row transition admitted')
 for name in ['filter-compat-portrait','normalize-xcf']:
  m=copy.deepcopy(j);m['proofs'][name]='0'*64
  try:proof_pins(m)
  except ValueError:pass
  else:raise ValueError('changed proof source hash admitted')
 m=copy.deepcopy(j);m['proofs']['missing-successor-proof']='0'*64
 try:proof_pins(m)
 except ValueError:pass
 else:raise ValueError('missing proof admitted')
 print('PASS current Normalize+Exchange three-field successor;108 identities/106 other rows unchanged;90 required labels not tasks;immutable historical replay;8 negative controls')
if __name__=='__main__':main()
