#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,tarfile,sys,struct
D=Path(__file__).resolve().parent;R=D.parents[2];h=lambda b:hashlib.sha256(b).hexdigest()
for p,w in json.loads((D/'artifacts.json').read_text()).items():assert h((D/p).read_bytes())==w,p
selected=json.loads((D/'selected-sources.json').read_text())
with tarfile.open(D/'sources.tar.gz')as t:
 assert {x.name for x in t if x.isfile()}==set(selected)
 for p,w in selected.items():assert h(t.extractfile(p).read())==w,p
owned=json.loads((D/'owned-sha256.json').read_text())
for p,w in owned.items():
 assert selected[p]==w
 if '--check-owned' in sys.argv:assert h((R/p).read_bytes())==w,p
red=json.loads((D/'red/results.json').read_text());v=next(iter(red.values()));assert len(red)==1 and v['exit_code']!=0 and 'gtk_combo_box_set_active_id' in v['stdout']+v['stderr'];assert 'Actual per-fixture GimpDisplay' in v['stdout']
for mode in ['green','sanitizer']:
 b=json.loads((D/mode/'build.json').read_text());rows=json.loads((D/mode/'results.json').read_text());seal=json.loads((D/mode/'runtime-seal.json').read_text())
 assert len(rows)==7 and seal['case_count']==7 and seal['binary_unchanged'] and seal['runtime_libraries_unchanged']
 assert b['linked_inputs_unchanged'] and len(b['linked_inputs'])>=1300 and len(b['runtime_libraries'])==(127 if mode=='sanitizer' else 125)
 assert b['owned']==owned
 for v in rows.values():assert v['exit_code']==0 and 'ok 1 ' in v['stdout'] and 'Actual per-fixture GimpDisplay' in v['stdout']
 for p,w in b['compiled_source'].items():assert selected[p]==w,p
san=json.loads((D/'sanitizer/build.json').read_text());assert len(san['compiled_source'])==11
assert len([c for c in san['compile']if '-fsanitize=address,undefined' in c])==8
for p in ['colorify-editor-before-oncanvas-portrait.png','colorify-editor-after-oncanvas-portrait.png']:
 data=(D/p).read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',data[16:24])==(1080,1920)
c=json.loads((D/'contract.json').read_text());assert c['actual_per_case_display'] and c['oncanvas_explicit_set_and_assert'] and c['before_editor_occluded'] and not c['old_edge_executed'] and not c['old_runtime_executed']
print('PASS Colorify existing editor:causal missing-page red,7 actual-oncanvas normal+7 scoped8-unit sanitizer,exact models/Undo/reentry; canvas pictures,occluded editor limitation explicit')
