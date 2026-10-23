#!/usr/bin/env python3
"""Reproduce independent extension fixtures, never historical oracle files."""
import importlib.util, pathlib, random, struct, json, hashlib
ROOT=pathlib.Path(__file__).resolve().parents[3]
p=ROOT/'migration/tests/painter-float-kernel/run.py'
spec=importlib.util.spec_from_file_location('oracle',p);o=importlib.util.module_from_spec(spec);spec.loader.exec_module(o)
r=random.Random(1301202);dest=ROOT/'migration/fixtures/painter-float/vectors.bin'
with dest.open('wb') as f:
 for mode in range(9):
  for opacity in [0,.0001,.37,.5,1]:
   opacity=o.f32(opacity);a=[];b=[];mask=[];expected=[]
   for n in range(64):
    aa=list(map(o.f32,[*[r.uniform(-4,4) for _ in range(3)],r.choice([0.,1.,r.random()])]))
    bb=list(map(o.f32,[*[r.uniform(-4,4) for _ in range(3)],r.choice([0.,1.,r.random()])]))
    m=o.f32(r.choice([0.,1.,r.random()]));a+=aa;b+=bb;mask.append(m);expected+=o.reference(aa,bb,opacity,m,mode)
   f.write(struct.pack('<if832f',mode,opacity,*a,*b,*mask,*expected))
dest.with_name('manifest.json').write_text(json.dumps({'scope':'independent Python premultiplied-double extension oracle, not old capture','seed':1301202,'format':'45 records: little-endian int mode, float opacity, float arrays backdrop256/source256/mask64/expected256','reference_source_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()},indent=2)+'\n')
