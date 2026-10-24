#!/usr/bin/env python3
import pathlib,json,hashlib,subprocess
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def check(p=P):
 j=json.loads((p/'source.json').read_text())
 assert j['revision']=='afa43fae3e920210146abed514f136fd49f671b5'
 for row in j['functions']:
  source=(p/(row['file']+'.source')).read_bytes();f=(p/row['file']).read_bytes()
  assert hashlib.sha256(source).hexdigest()==row['source_sha256']
  assert hashlib.sha256(f).hexdigest()==row['function_sha256']
  assert f in source
  assert source[:source.index(f)].count(b'\n')+1==row['line']
  assert hashlib.sha1(b'blob '+str(len(source)).encode()+b'\0'+source).hexdigest()==row['blob']
 for row in j['macro_sources']:assert hashlib.sha256((p/row['file']).read_bytes()).hexdigest()==row['sha256']
 shell=(p/'gimpdisplayshell.h.source').read_text();math=(p/'gimpmath.h.source').read_text()
 assert '#define  PROJ_ROUND(coord)   ((gint) RINT (coord))' in shell
 assert '#define  SCALEX(s,x)      PROJ_ROUND ((x) * (s)->scale_x)' in shell
 assert '#define  SCALEY(s,y)      PROJ_ROUND ((y) * (s)->scale_y)' in shell
 assert '#if defined (HAVE_RINT) && 0' in math and '#define RINT(x) floor ((x) + 0.5)' in math
 adapter=(p/'hit-oracle.inc').read_text()
 assert '#define SCALEX(s,x) ((gint) floor ((x) * (s)->scale_x + 0.5))' in adapter
 assert '#define SCALEY(s,y) ((gint) floor ((y) * (s)->scale_y + 0.5))' in adapter
 assert '#include "zoom.inc"' in adapter and '#include "on-handle.inc"' in adapter
 assert 'g_assert_cmpint (anchor, ==, GIMP_HANDLE_ANCHOR_CENTER); *nx = x; *ny = y;' in adapter
 print('PASS: pinned unchanged old zoom/hit functions and bounded circle-center adapter')
if __name__=='__main__':check()
