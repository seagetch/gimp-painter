#!/usr/bin/env python3
import pathlib,json,hashlib
P=pathlib.Path(__file__).resolve().parent
def sha(b):return hashlib.sha256(b).hexdigest()
def verify():
 m=json.loads((P/'source.json').read_text());b=(P/'paint-funcs-generic.h.source').read_bytes();assert sha(b)==m['sha256'];assert hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()==m['git_blob']
 for n in ['paint-funcs.c.source','paint-funcs-utils.h','old-brushcore.c.source']:
  b=(P/n).read_bytes();assert sha(b)==m[n]['sha256'];assert hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()==m[n]['git_blob']
 for n,full,key in [('blend-pixels.inc','paint-funcs-generic.h.source','function_sha256'),('replace-pixels.inc','paint-funcs-generic.h.source','replace_function_sha256'),('opaque-compose.inc','paint-funcs.c.source','opaque_function_sha256')]:
  b=(P/n).read_bytes();assert sha(b)==m[key];assert b in (P/full).read_bytes()
 old=(P/'old-brushcore.c.source').read_text();assert '1, 0, 0, &empty);' in old and '*d++ = (*m++) ? OPAQUE_OPACITY : TRANSPARENT_OPACITY;' in old
 assert '#define HAS_ALPHA(bytes) (~bytes & 1)' in (P/'paint-funcs-utils.h').read_text()
 print('PASS: pinned unchanged old blend/replace/opaque composition, exact macros and old byte solidify source; no old runtime')
if __name__=='__main__':verify()
