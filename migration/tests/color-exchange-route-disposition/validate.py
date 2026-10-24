#!/usr/bin/env python3
"""Prove a single bounded disposition update, not discovery or task completion."""
from pathlib import Path
import csv,io,json,hashlib,ast,subprocess,copy
P=Path(__file__).resolve().parent;R=P.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
def rows(b):return list(csv.DictReader(io.StringIO(b.decode()),delimiter='\t'))
def transition(before,after,j):
 assert len(before)==len(after)==108 and [x['procedure']for x in before]==[x['procedure']for x in after]
 assert len({x['procedure']for x in after})==108
 for a,b in zip(before,after):
  if a['procedure']!='plug-in-exchange':assert a==b
  else:
   changed={k for k in a if a[k]!=b[k]};assert changed==set(j['bounded_fields'])
   for k,v in j['bounded_fields'].items():assert b[k]==v
 assert sum('required'in x['executor_route']for x in after)==91
artifacts=json.loads((P/'artifact-sha256.json').read_text());assert set(artifacts)=={str(p.relative_to(P))for p in P.rglob('*')if p.is_file()and p.name!='artifact-sha256.json'and '__pycache__'not in p.parts}
for name,h in artifacts.items():assert sha((P/name).read_bytes())==h,name
j=json.loads((R/'migration/tests/color-exchange-route-inventory.json').read_text())
b=(P/'before.tsv').read_bytes();a=(P/'after.tsv').read_bytes()
assert sha(b)==j['old_routes_sha256'] and sha(a)==j['new_routes_sha256']
before,after=rows(b),rows(a);transition(before,after,j)
assert j['native_checkpoint'].startswith('162fd09bdf')and j['writer_checkpoint'].startswith('d3969eb27b')
assert j['total_procedure_identities']==108 and j['remaining_required_labels']==91
assert 'XCF/UI/editor still open'in j['status'] and 'not original-task closure'in j['status']
remaining={}
for row in after:
 if 'required'in row['executor_route']:remaining.setdefault(row['executor_route'],[]).append(row['procedure'])
assert remaining==j['remaining_by_label']
s=(P/'generator-after.py').read_text();start=s.index('    "plug-in-exchange": {');end=s.index('    "plug-in-red-eye-removal"',start)
assert sha((s[:start]+s[end:]).encode())==j['generator_predecessor_sha256']
node=next(x for x in ast.parse(s).body if isinstance(x,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='BOUNDED_CONTEXT_ROUTES'for t in x.targets))
assert ast.literal_eval(node.value)['plug-in-exchange']==j['bounded_fields']
live=rows((R/'migration/inventory/filter-procedure-routes.tsv').read_bytes());assert next(x for x in live if x['procedure']=='plug-in-exchange')==next(x for x in after if x['procedure']=='plug-in-exchange')
current=ast.parse((R/'tools/inventory_filter_routes.py').read_text());node=next(x for x in current.body if isinstance(x,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='BOUNDED_CONTEXT_ROUTES'for t in x.targets));assert ast.literal_eval(node.value)['plug-in-exchange']==j['bounded_fields']
for name,h in j['proofs'].items():
 p=R/'migration/tests'/name
 assert sha((p/'artifact-sha256.json').read_bytes())==h,name
 proof=subprocess.run(['python3','-B',str(p/'validate.py')],capture_output=True,text=True)
 assert proof.returncode==0,(name,proof.stdout,proof.stderr)
# Active negative controls preserve the comparison algorithm, mutate only input.
mutants=[]
m=copy.deepcopy(after);m.pop();mutants.append(m)
m=copy.deepcopy(after);m[0]['legacy_source']='changed.c';mutants.append(m)
m=copy.deepcopy(after);next(x for x in m if x['procedure']=='plug-in-exchange')['executor_route']='all work complete';mutants.append(m)
m=copy.deepcopy(after);next(x for x in m if x['procedure']=='plug-in-exchange')['argument_types']='wrong';mutants.append(m)
m=copy.deepcopy(after);next(x for x in m if x['procedure']=='plug-in-exchange')['missing_port_work']='';mutants.append(m)
for m in mutants:
 try:transition(before,m,j)
 except AssertionError:pass
 else:raise AssertionError('invalid transition accepted')
print('PASS Exchange single3-column bounded disposition;108 identities/107 other rows preserved;checkpoint91 required labels,not tasks;5 controls;XCF/UI/editor open')
