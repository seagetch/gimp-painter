#!/usr/bin/env python3
"""Exact byte comparison with actual pinned-old and unmarked-modern recordings."""
import argparse,gzip,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('binary',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--pixmap',action='store_true');a=p.parse_args();root=Path(__file__).resolve().parents[2]
def normalize(raw):return b'\n'.join(line for line in raw.splitlines() if line.startswith(b'GEOMETRY_'))+b'\n'
checks=[]
cases=[('legacy','migration/fixtures/legacy-brush-geometry-pixmap/pixels.tsv.gz',{'PAINTER_GEOMETRY_PIXMAP':'1'})] if a.pixmap else [('legacy','migration/fixtures/legacy-brush-geometry/pixels.tsv.gz',{}),('modern','migration/tests/brush-geometry-modern-before.tsv.gz',{'PAINTER_GEOMETRY_MODERN':'1'})]
for name,fixture,extra in cases:
 env={**os.environ,**extra};run=subprocess.run([str(a.binary)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=180)
 expected=normalize(gzip.decompress((root/fixture).read_bytes()));actual=normalize(run.stdout);left=expected.splitlines();right=actual.splitlines()
 mismatch=next((i for i,(x,y) in enumerate(zip(left,right)) if x!=y),min(len(left),len(right)))
 record={'kind':name,'fixture':fixture,'exit_code':run.returncode,'exact':expected==actual,'expected_records':len(left),'actual_records':len(right),'binary_sha256':hashlib.sha256(a.binary.read_bytes()).hexdigest(),'expected_sha256':hashlib.sha256(expected).hexdigest(),'actual_sha256':hashlib.sha256(actual).hexdigest(),'stderr':run.stderr.decode()}
 if expected!=actual:
  record.update(mismatch_record=mismatch,expected=left[mismatch][:200].decode() if mismatch<len(left) else '<end>',actual=right[mismatch][:200].decode() if mismatch<len(right) else '<end>')
  a.report.with_suffix('.actual.tsv').write_bytes(actual)
 checks.append(record);print(name,run.returncode,record['exact'],len(left),record.get('mismatch_record'),flush=True)
report={'checks':checks,'success':all(x['exit_code']==0 and x['exact'] for x in checks)};a.report.write_text(json.dumps(report,indent=2)+'\n')
if not report['success']:raise SystemExit(1)
