#!/usr/bin/env python3
"""Frozen clean-provider follow-up; does not rewrite predecessor evidence."""
import argparse,hashlib,json,re,tarfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--check-owned-source',action='store_true');args=p.parse_args()
d=Path(__file__).resolve().parent;r=d.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
m=json.loads((d/'source-manifest.json').read_text());held=json.loads((d/'held-source-seal.json').read_text());assert m['selected_commit']=='ff2116774a';assert held['baseline']==m['clean_base']=='81bc24545d';assert len(held['four_holds_eight_production_files'])==8
with tarfile.open(d/'tested-inputs.tar.gz') as t:
 for n,h in m['input_sha256'].items():assert sha(t.extractfile(n).read())==h,n
 for n,h in held['four_holds_eight_production_files'].items():assert m['input_sha256'][n]==h,n
 for n,h in m['owned_source_sha256'].items():
  assert m['input_sha256'][n]==h,n
  if args.check_owned_source:assert sha((r/n).read_bytes())==h,n
b=json.loads((d/'asan-build.json').read_text());assert len(b['instrumented_sources'])==6 and not b['changed_during_build'];assert all(m['input_sha256'][n]==h for n,h in b['sources_sha256'].items())
normal=json.loads((d/'normal.json').read_text());assert len(normal)==1 and normal[0]['result']=='OK';asan=json.loads((d/'asan-results.json').read_text())
for row in [normal[0],asan]:
 assert row['returncode']==0;assert re.search(r'^1\.\.15$',row['stdout'],re.M);assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==15
 assert not re.search(r'not ok|ERROR: AddressSanitizer|runtime error:',row['stdout']+(row['stderr'] or ''))
for n in ['normal-portrait.log','asan-portrait.log']:assert '1080x1920x24, authenticated accepted, empty authority rejected' in (d/n).read_text()
v=json.loads((d/'verified.json').read_text());assert v['inputs_unchanged'] and v['oncanvas_enabled_asserted_by_committed_test'] and not v['rendered_ui_claim']
print('PASS clean-provider pixel follow-up15 native/15 selected-six-unit sanitizer;8 committed held-provider exclusions; frozen prebuild sources; model-only')
