#!/usr/bin/env python3
"""Validate complete real-old geometry oracles independently of the port."""
import gzip,hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[2]
for fixture,count in [('legacy-brush-geometry',288),('legacy-brush-geometry-pixmap',48),('legacy-brush-geometry-angle-tie',144)]:
 folder=root/'migration/fixtures'/fixture;report=json.loads((folder/'runtime.json').read_text())
 assert report['commit']=='afa43fae3e920210146abed514f136fd49f671b5' and report['exit_code']==0
 assert hashlib.sha256((root/report['harness_source']).read_bytes()).hexdigest()==report['harness_sha256']
 rows=gzip.decompress((folder/'pixels.tsv.gz').read_bytes()).decode().splitlines();assert rows[-1]=='GEOMETRY_CAPTURE_COMPLETE'
 axes=[row for row in rows if row.startswith('GEOMETRY_AXES ')];assert len(axes)==(1 if fixture.endswith('angle-tie') else 0)
 rows=[row for row in rows if not row.startswith('GEOMETRY_AXES ')];assert len(rows)==4*count+1
 grouped={}
 for row in rows[:-1]:
  tag,ident,phase,channels,hexdata=row.split();assert tag=='GEOMETRY_STROKE';data=bytes.fromhex(hexdata);assert len(data)==48*40*int(channels)
  grouped.setdefault(int(ident),{})[phase]=data
 assert len(grouped)==count
 for phases in grouped.values():
  assert set(phases)=={'initial','finish','undo','redo'}
  assert phases['initial']==phases['undo'] and phases['finish']==phases['redo']
  assert phases['initial']!=phases['finish']
 print(fixture,count,'actual old strokes + Undo/Redo verified')

folder=root/'migration/fixtures/legacy-brush-geometry'
report=json.loads((folder/'default-dynamics.json').read_text())
assert report['commit']=='afa43fae3e920210146abed514f136fd49f671b5' and report['exit_code']==0
assert hashlib.sha256((root/report['harness_source']).read_bytes()).hexdigest()==report['harness_sha256']
rows=gzip.decompress((folder/'default-dynamics.tsv.gz').read_bytes()).decode().splitlines()
assert rows[0]=='CURVE_DEFAULT 0 17 256' and len(rows)==257
for i,row in enumerate(rows[1:]):
 tag,index,value=row.split();assert tag=='CURVE_SAMPLE' and int(index)==i and 0<=float.fromhex(value)<=1
print('legacy default dynamics:256 actual old cubic samples verified')
