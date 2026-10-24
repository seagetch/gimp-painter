#!/usr/bin/env python3
import hashlib,json,pathlib,tarfile
p=pathlib.Path(__file__).resolve().parent;a=p/'evidence.tar.gz'
assert hashlib.sha256(a.read_bytes()).hexdigest()=='a2438fbbde3b8f994bd5e10f44d86bb5f844713d58b6e9fbeda962dcfcc4daab'
with tarfile.open(a) as t:
 read=lambda n:json.loads(t.extractfile(n).read())
 source=read('source-manifest.json');assert source['commit']=='5d1c22bc7bdd01e2ef7d36cebed41a52c3a8b15c' and len(source['files'])==16474 and len(source['refreshed_paths'])==97
 assert read('source-after.json')=={'checked':16474,'changed':[]}
 assert not any(v['linked'] for n,v in source['files'].items() if n.startswith('app/'))
 assert t.extractfile('adapter.status').read().strip()==b'0' and t.extractfile('persistence.status').read().strip()==b'0'
 assert b'PASS: 2535 sources; 720 reviewed data sites; 4 dormant legacy findings' in t.extractfile('adapter.log').read()
 r=read('persistence.json');assert r['result']=='PASS' and (r['counts']['contract_records'],r['counts']['syntax_candidates'],r['counts']['routes'])==(1379,873,16)
print('PASS: exact committed5d1c source-only adapter/persistence gates; later source not covered')
