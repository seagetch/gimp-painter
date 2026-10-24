#!/usr/bin/env python3
import pathlib,json,hashlib
P=pathlib.Path(__file__).resolve().parent
def check(p=P):
 j=json.loads((p/'source.json').read_text());s=(p/'smudge.c.source').read_bytes();f=(p/'paint-order.inc').read_bytes()
 assert j['revision']=='afa43fae3e920210146abed514f136fd49f671b5'
 assert j['blob']=='4d5cd380aa0bcc8f3d15482a21adfb2689cf7b6b'
 assert hashlib.sha1(b'blob '+str(len(s)).encode()+b'\0'+s).hexdigest()==j['blob']
 assert hashlib.sha256(s).hexdigest()==j['source_sha256']=='3f41729a4e3920d3e0abda1e1afb37eaf2d866cf184ffd168bb28381e817eff1'
 assert hashlib.sha256(f).hexdigest()==j['function_sha256']=='c86c698372d688f42d0e304dfab346214d51ed324f2bac89d186252d654b1fd1'
 assert f in s and s[:s.index(f)].count(b'\n')+1==j['line']==149
 assert f.index(b'gimp_smudge_start (')<f.index(b'gimp_smudge_motion (')
 start=s[s.index(b'static gboolean\ngimp_smudge_start ('):s.index(b'static void\ngimp_smudge_motion (')]
 assert b'if (! area)\n    return FALSE;' in start and b'CLAMP ((gint) coords->x' in start
 motion=s[s.index(b'static void\ngimp_smudge_motion ('):]
 assert motion.index(b'if (opacity == 0.0)')<motion.index(b'blend_region')
 print('PASS: unchanged old start-before-motion control flow, outside refusal and opacity-before-update source; no old runtime')
if __name__=='__main__':check()
