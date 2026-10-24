#!/usr/bin/env python3
import pathlib,json,hashlib
P=pathlib.Path(__file__).resolve().parent
def check(p=P):
 j=json.loads((p/'source.json').read_text());s=(p/'tool-events.c.source').read_bytes()
 assert j['revision']=='afa43fae3e920210146abed514f136fd49f671b5'
 assert j['blob']=='b670161eb5906349f84a51da961ae339b7209574'
 assert j['source_sha256']=='7b81d99ab288247d1452ee47053731a4c71eb8c40fa54c734f87023d31757c84'
 assert hashlib.sha256(s).hexdigest()==j['source_sha256']
 assert hashlib.sha1(b'blob '+str(len(s)).encode()+b'\0'+s).hexdigest()==j['blob']
 assert len(j['functions'])==2
 for r in j['functions']:
  f=(p/r['file']).read_bytes();assert hashlib.sha256(f).hexdigest()==r['sha256'];assert f in s;assert s[:s.index(f)].count(b'\n')+1==r['line']
 a=(p/'oracle.inc').read_text();assert '#define DIST_THRESHOLD 32' in a
 assert '#include "snap-angle.inc"' in a and '#include "lazy-snap.inc"' in a
 assert '#define GimpDisplayShell LegacyStrokeShell' in a
 print('PASS: unchanged pinned old snap-angle/lazy-projection source,32px threshold')
if __name__=='__main__':check()
