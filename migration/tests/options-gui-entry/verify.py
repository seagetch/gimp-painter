#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,struct,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-owned-source',action='store_true');a=p.parse_args()
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest();read=lambda p:json.loads((E/p).read_text())
for phase in ['red','green']:
 m=read(phase+'/source-manifest.json')
 with tarfile.open(E/phase/'tested-inputs.tar.gz') as t:
  for path,h in m['input_sha256'].items():assert sha(t.extractfile(path).read())==h,path
 assert len(m['held_providers'])==8 and len(m['clone_providers'])==4
 assert m['base_for_held_providers']=='a84cd0b051e7b648ec9519ede82ae9541c44ea1c'
m=read('green/source-manifest.json')
if a.check_owned_source:
 for path,h in m['owned_source_sha256'].items():assert sha((R/path).read_bytes())==h,path
assert re.search('undefined reference.*gimp_mypaint_options_gui',(E/'red/build.log').read_text())
for row,count in [(read('green/normal.json')[0],10),(read('green/asan-results.json'),8)]:
 assert row['returncode']==0
 assert re.search(r'^1\.\.'+str(count)+'$',row['stdout'],re.M)
 assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==count
 assert not re.search(r'ERROR: AddressSanitizer|runtime error:',row['stdout']+(row['stderr'] or ''))
b=read('green/asan-build.json');assert len(b['instrumented_sources'])==5;assert not b['changed_during_build']
for path,h in b['sources_sha256'].items():assert h==m['input_sha256'][path]
assert read('green/post-test-inputs.json')['all_recorded_inputs_unchanged'] is True
for name in ['normal-portrait.log','asan-portrait.log']:
 assert '1080x1920x24, authenticated accepted, empty authority rejected' in (E/'green'/name).read_text()
for name in ['vertical','horizontal']:
 data=(E/'green/screenshots'/('original-options-'+name+'-portrait.png')).read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n';assert struct.unpack('>II',data[16:24])==(1080,1920)
for path,h in read('evidence-manifest.json').items():assert sha((E/path).read_bytes())==h,path
print('PASS: original GUI normal10 + selected5-unit sanitizer8; C missing-symbol red; portrait screenshots; selected source seals; no full semantic parity claim')
