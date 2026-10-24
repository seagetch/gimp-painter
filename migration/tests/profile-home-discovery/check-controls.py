#!/usr/bin/env python3
import copy
import importlib.util
from pathlib import Path
p=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('proof',p/'validate.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
a,b,s,r=m.load();m.validate(a,b,s,r)
cases=[]
x=copy.deepcopy(b);x['linked_inputs_changed']=['unrelated.o'];cases.append((a,x,s,r))
x=copy.deepcopy(b);x['results']=[v for v in x['results'] if v['name']!='normal-xdg-precedence-run'];cases.append((a,x,s,r))
x=copy.deepcopy(b);next(v for v in x['results'] if v['name']=='asan-migrate-retry-run')['returncode']=1;cases.append((a,x,s,r))
x=copy.deepcopy(a);next(v for v in x['results'] if v['name'].endswith('-run'))['returncode']=0;cases.append((x,b,s,r))
x=copy.deepcopy(b);next(v for v in x['results'] if v['name'].startswith('asan-compile-'))['command'].remove('-fsanitize=address,undefined');cases.append((a,x,s,r))
x=copy.deepcopy(s);x['after.c']+=b'\n/* unrelated change */';cases.append((a,b,x,r))
for i,args in enumerate(cases):
 try:m.validate(*args)
 except AssertionError:continue
 raise AssertionError(('control accepted',i))
print('PASS: 6 causal receipt/source controls rejected')
