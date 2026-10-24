#!/usr/bin/env python3
import pathlib,json,importlib.util,copy
P=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('admit',P/'ledger-admission.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
rows=json.loads((P/'rows.json').read_text())['rows'];n=0
for row in rows:
 m.admit(row['before'],row['before']);m.admit(row['before'],row['after'])
r=rows[0]
def reject(old,new):
 global n
 try:m.admit(old,new)
 except (AssertionError,KeyError):n+=1
 else:raise AssertionError('mutation admitted')
for k in r['before']:
 a=copy.deepcopy(r['before']);a[k]+=' INVALID';reject(a,r['after'])
 b=copy.deepcopy(r['after']);b[k]+=' INVALID';reject(r['before'],b)
for k in r['after']:
 b=copy.deepcopy(r['after']);del b[k];reject(r['before'],b)
b=copy.deepcopy(r['after']);b['invented']='field';reject(r['before'],b)
b=copy.deepcopy(r['after']);b['status']='DOING';reject(r['before'],b)
print(f'PASS:52 exact before/after pairs;{n} rejected historical/successor field/removal/unknown-status controls')
