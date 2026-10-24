from pathlib import Path
import hashlib,json,gzip
E=Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
ref=json.loads((E/'reference.json').read_text());raw=gzip.decompress((E/'old-source.c.gz').read_bytes())
assert ref['commit']=='afa43fae3e920210146abed514f136fd49f671b5'
assert sha(raw)==ref['sha256']
assert hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()==ref['blob']
s=raw.decode()
for n,prefix in [('old-wrapper.inc','gboolean\n'),('old-compressor.inc','static GdkEvent *\n')]:
 f=(E/n).read_text();assert f.startswith(prefix);assert f[len(prefix):].strip() in s,n
r=json.loads((E/'results.json').read_text());assert not r['changed_after_run']
for n,h in r['inputs'].items():assert sha((E/n).read_bytes())==h,n
assert set(r['runs'])=={'normal','sanitizer'}
for row in r['runs'].values():
 assert row['compile']['exit_code']==0
 q=row['runtime'];assert q['exit_code']==0 and not q['stderr'];assert q['stdout'].startswith('PASS 4163 bounded')
assert '-fsanitize=address,undefined' in r['runs']['sanitizer']['compile']['command']
assert {m['name'] for m in r['mutations']}=={'state-boundary','device-boundary','window-boundary','terminal-dispatch','terminal-free','double-terminal-free'}
for m in r['mutations']:assert m['exit_code']!=0 and 'Assertion' in m['stderr'] and m['original_sha256']==r['inputs'][m['changed_file']]
print('PASS pinned source-only event foundation: 4163 normal + 4163 sanitizer scenes; 6 causal mutants; native order remains open')
