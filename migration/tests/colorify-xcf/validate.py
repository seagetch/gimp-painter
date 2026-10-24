#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,gzip,tarfile,sys
p=Path(__file__).resolve().parent;r=p.parents[2]
h=lambda b:hashlib.sha256(b).hexdigest()
for f,w in json.loads((p/'artifacts.json').read_text()).items():assert h((p/f).read_bytes())==w,f
before=json.loads(gzip.decompress((p/'source-before.json.gz').read_bytes()));after=json.loads(gzip.decompress((p/'source-after.json.gz').read_bytes()));assert before['sha256']==after['sha256'] and not after['changed_after_run']
selected=json.loads((p/'selected-sources.json').read_text())
with tarfile.open(p/'sources.tar.gz')as t:
 assert {x.name for x in t if x.isfile()}==set(selected)
 for f,w in selected.items():assert h(t.extractfile(f).read())==w==after['sha256'][f],f
owned=json.loads((p/'owned-sha256.json').read_text())
for f,w in owned.items():
 assert selected[f]==w
 if '--check-owned' in sys.argv:assert h((r/f).read_bytes())==w,f
contract=json.loads((p/'contract.json').read_text());native=json.loads((p/'native.json').read_text());san=json.loads((p/'sanitizer.json').read_text())
assert len(native)==4 and all(v['exit_code']==0 and 'ok 1 ' in v['stdout']for v in native.values())
assert san['exit_code']==0 and not san['changed_after_run'] and len(san['sources'])==11
assert '1..2' in san['stdout'] and len([x for x in san['stdout'].splitlines()if x.startswith('ok ')])==2
assert not san['leak_detection'] and not san['instrumented_cpp_rtti']
for f,w in san['source_sha256'].items():assert after['sha256'][f]==w,f
assert contract['save_open_rounds']==24 and not contract['production_changes'] and not contract['old_runtime_executed'] and not contract['ui_verified']
assert len([x for x in native['colorify_typed_xcf_roundtrips']['stdout'].splitlines()if 'Colorify typed scene' in x])==4
assert len([x for x in native['colorify_unsupported_typed_xcf_roundtrips']['stdout'].splitlines()if 'Colorify unsupported tuple' in x])==8
print('PASS Colorify typed XCF:24 rounds,normal4/scoped11-unitASan2,exact nullable/unknown tuple bytes/cache,pixels and sealed test-only source')
