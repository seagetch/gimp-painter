#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-owned-source',action='store_true');a=p.parse_args()
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest();read=lambda p:json.loads((E/p).read_text())
for phase in ['red','green']:
 m=read(phase+'/source-manifest.json')
 with tarfile.open(E/phase/'tested-inputs.tar.gz') as t:
  for path,h in m['input_sha256'].items():assert sha(t.extractfile(path).read())==h,path
 assert len(m['held_providers'])==8 and len(m['clone_providers'])==4
 assert m['base_for_held_providers']=='20b2204b11df079c306a680b41ac623f3be75c76'
m=read('green/source-manifest.json')
if a.check_owned_source:
 for path,h in m['owned_source_sha256'].items():assert sha((R/path).read_bytes())==h,path
assert re.search('undefined reference.*gimp_mypaint_options_get_brush_mode',(E/'red/build.log').read_text())
for row,count in [(read('green/normal.json')[0],6),(read('green/asan-results.json'),6)]:
 assert row['returncode']==0
 assert re.search(r'^1\.\.'+str(count)+'$',row['stdout'],re.M)
 assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==count
 assert not re.search(r'ERROR: AddressSanitizer|runtime error:',row['stdout']+(row['stderr'] or ''))
b=read('green/asan-build.json');assert len(b['instrumented_sources'])==4;assert not b['changed_during_build']
for path,h in b['sources_sha256'].items():assert h==m['input_sha256'][path]
assert read('green/post-test-inputs.json')['all_recorded_inputs_unchanged'] is True
h=read('green/headers.json');assert h['status']=='PASS' and h['routes']>=42 and h['probes']>=88
for name in ['normal-portrait.log','asan-portrait.log']:
 assert '1080x1920x24, authenticated accepted, empty authority rejected' in (E/'green'/name).read_text()
for path,h in read('evidence-manifest.json').items():assert sha((E/path).read_bytes())==h,path
print('PASS: constant original mode normal6 + selected4-unit sanitizer6; C missing-symbol red; model-only portrait/oncanvas source seals')
