#!/usr/bin/env python3
import json,hashlib,pathlib,re,tarfile,argparse
p=argparse.ArgumentParser();p.add_argument('--check-owned-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];read=lambda p:json.loads((E/p).read_text());sha=lambda b:hashlib.sha256(b).hexdigest()
for phase in ['red','green']:
 m=read(phase+'/source-manifest.json')
 with tarfile.open(E/phase/'tested-inputs.tar.gz') as t:
  for p,h in m['input_sha256'].items():assert sha(t.extractfile(p).read())==h,p
m=read('green/source-manifest.json')
if a.check_owned_source:
 for p,h in m['owned_sha256'].items():assert sha((R/p).read_bytes())==h,p
r=read('red/results.json')[0];assert r['returncode']!=0 and 'entry->help_id' in r['stdout'] and '("gimp-dock" == "")' in r['stdout']
rows=read('green/results.json');assert len(rows)==2
for r in rows:
 n=18 if r['name'].endswith('layer-presets-ui') else 6;assert r['returncode']==0 and len(re.findall(r'^ok \d+',r['stdout'],re.M))==n
r=read('green/asan-results.json');assert r['returncode']==0 and len(re.findall(r'^ok \d+',r['stdout'],re.M))==18;assert not re.search('ERROR: AddressSanitizer|runtime error:',r['stdout']+(r['stderr'] or ''))
b=read('green/asan-build.json');assert len(b['instrumented_sources'])==2
for p,h in b['instrumented_sources'].items():assert m['input_sha256'][p]==h
assert read('green/asan-post.json')['dependency_objects_unchanged'];assert read('green/source-after.json')['complete_recorded_source_unchanged']
for p in ['green/portrait.log','green/asan-portrait.log']:assert '1080x1920x24, authenticated accepted, empty authority rejected' in (E/p).read_text()
o=read('legacy-source/oracle.json');assert o['dialog_help_id']=='' and o['action_help_id']=='gimp-layer-preset-dialog';assert sha((E/'legacy-source/layer-preset-gui.cpp').read_bytes())==o['sha256']
for p,h in read('evidence-manifest.json').items():assert sha((E/p).read_bytes())==h,p
print('PASS: one-field original dialog help; causal registered-entry red; normal24 and selected2-unit ASan18; no installed-help claim')
