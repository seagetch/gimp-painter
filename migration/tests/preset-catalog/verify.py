#!/usr/bin/env python3
"""Portable bounded catalog evidence verifier; never reruns application callbacks."""
import argparse, hashlib, json, pathlib, tarfile
p=argparse.ArgumentParser();p.add_argument('--root',type=pathlib.Path,default=pathlib.Path(__file__).resolve().parent);p.add_argument('--check-owned-source',action='store_true');a=p.parse_args();E=a.root;R=pathlib.Path(__file__).resolve().parents[3]
sha=lambda b:hashlib.sha256(b).hexdigest()
m=json.loads((E/'manifest.json').read_text())
for n,h in m['artifacts'].items():assert sha((E/n).read_bytes())==h,n
with tarfile.open(E/'tested-inputs.tar.gz') as t:
 for n,h in m['owned_source_sha256'].items():assert sha(t.extractfile(n).read())==h,n
assert 'autosave' in (E/'cleanup-red.log').read_text() and 'SIGABRT' in (E/'cleanup-red.log').read_text()
normal=json.loads((E/'normal-results.json').read_text());assert len(normal)==3
counts={'preset-catalog':26,'preset-catalog-ui':5,'preset-gui-entries':6}
for r in normal:
 count=counts[r['name'].split('/')[-1].strip()]
 assert r['result']=='OK' and r['returncode']==0,r['name']
 assert len([l for l in r['stdout'].splitlines() if l.startswith('ok ')])==count
 assert '# SKIP' not in r['stdout']
san=json.loads((E/'sanitizer-results.json').read_text());assert len(san)==3
for r in san:
 count=counts[r['target']]
 assert r['returncode']==0,r['target']
 assert len([l for l in r['stdout'].splitlines() if l.startswith('ok ')])==count
 assert '# SKIP' not in r['stdout']
build=json.loads((E/'sanitizer-build.json').read_text());assert len(build['instrumented_sources'])==11;assert not build['changed_during_build'];assert not build['leak_detection'];assert not build['whole_application_instrumented']
assert json.loads((E/'sanitizer-source-before.json').read_text())==json.loads((E/'sanitizer-source-after.json').read_text())
assert (E/'selected-catalog-portrait.png').read_bytes()[:8]==b'\x89PNG\r\n\x1a\n'
assert '1080x1920x24' in (E/'portrait.log').read_text()
assert 'the_unit_gimp == NULL' in (E/'singleton-failure.json').read_text()
assert 'gimp_preset_catalog_register' in (E/'missing-entry.log').read_text()
assert json.loads((E/'link-inputs-before.json').read_text())==json.loads((E/'link-inputs-after.json').read_text())
mutants=json.loads((E/'mutants.json').read_text());assert len(mutants)==2
for r in mutants:assert r['returncode']!=0 and r['expected_assertion'] in r['stdout']+r['stderr']
held=json.loads((E/'held-provider-exclusion.json').read_text());assert held['held_provider_count']==len(held['paths'])==10
full=json.loads((E/'source-manifest.json').read_text())['complete_source_sha256']
for row in held['paths']:assert full[row['path']]==row['sha256']
regressions=json.loads((E/'regression-results.json').read_text());assert len(regressions)==6
assert all(r['result']=='OK' and r['returncode']==0 for r in regressions)
if a.check_owned_source:
 for n,h in m['owned_source_sha256'].items():assert sha((R/n).read_bytes())==h,n
print('PASS: catalog normal26+GUI5+legacy6; selected eleven-unit ASan37; immutable input archive; scoped ownership/provenance')
