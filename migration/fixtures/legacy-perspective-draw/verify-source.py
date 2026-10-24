#!/usr/bin/env python3
"""Verify the exact pinned draw body; optional standalone adapter run."""
import hashlib,json,pathlib,subprocess,sys,tempfile
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2];m=json.loads((P/'source.json').read_text())
src=subprocess.check_output(['git','show',m['revision']+':'+m['path']],cwd=R)
assert src==(P/'pinned-source.cpp').read_bytes()
assert hashlib.sha256(src).hexdigest()==m['source_sha256']
s=src.decode();start=s.index('void \nCanvasPerspectiveGuide::draw');end=s.index('\n\n\ncairo_region_t*',start)
body=(s[start:end]+'\n').encode();assert body==(P/'draw-source.inc').read_bytes();assert hashlib.sha256(body).hexdigest()==m['function_sha256']
assert m['source_id']=='01.002/000532' and m['work_id']=='legacy-cf0e7373680579646933'
expected='points=1 radial_segments=1 line_segments=1\npoints=2 radial_segments=1 line_segments=2\npoints=3 radial_segments=3 line_segments=3\n'
assert (P/'result.txt').read_text()==expected
if '--run' in sys.argv:
 with tempfile.TemporaryDirectory() as d:
  flags=subprocess.check_output(['pkg-config','--cflags','--libs','cairo'],text=True).split()
  subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror',str(P/'check.cpp'),*flags,'-o',d+'/check'],check=True)
  assert subprocess.check_output([d+'/check'],text=True)==expected
print('PASS unchanged pinned old draw source and1/1/3 radial path counts; source adapter only')
