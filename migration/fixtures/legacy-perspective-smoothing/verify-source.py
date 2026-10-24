#!/usr/bin/env python3
import hashlib,json,pathlib
P=pathlib.Path(__file__).resolve().parent
def check(p=P):
 j=json.loads((p/'source.json').read_text());s=(p/'paintcore.c.source').read_bytes();f=(p/'smooth-coords.inc').read_bytes()
 assert j['revision']=='afa43fae3e920210146abed514f136fd49f671b5'
 assert j['blob']=='d3b66dcc34444f63e45fe27c63b665cd2580b3c1'
 assert hashlib.sha1(b'blob '+str(len(s)).encode()+b'\0'+s).hexdigest()==j['blob']
 assert hashlib.sha256(s).hexdigest()==j['source_sha256']=='92a20fc87dec36622cf27ccbd92bc1b8124d69e99b28d20d1cf5c330eaaddec3'
 assert hashlib.sha256(f).hexdigest()==j['function_sha256']=='6c82979ee4835a8cb654c93db494ac67301b37acc5cb1ae1340eaf0cd779812c'
 assert f in s and s[:s.index(f)].count(b'\n')+1==j['line']==1111
 a=(p/'oracle.inc').read_text();assert '#define GimpPaintCore LegacySmoothingCore' in a and '#include "smooth-coords.inc"' in a
 print('PASS: unchanged pinned old Gaussian body, history-only core adapter; no old runtime')
if __name__=='__main__':check()
