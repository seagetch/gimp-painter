#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,tarfile,struct,sys
D=Path(__file__).resolve().parent;R=D.parents[2];h=lambda b:hashlib.sha256(b).hexdigest()
for p,w in json.loads((D/'artifacts.json').read_text()).items():assert h((D/p).read_bytes())==w,p
sources=json.loads((D/'sources.json').read_text());b=json.loads((D/'build.json').read_text())
with tarfile.open(D/'sources.tar.gz')as t:
 assert {x.name for x in t if x.isfile()}==set(sources)
 for p,w in sources.items():assert h(t.extractfile(p).read())==w,p
for p,w in b['owned'].items():
 assert sources[p]==w
 if '--check-owned' in sys.argv:assert h((R/p).read_bytes())==w,p
for p,w in b['compiled_source'].items():assert sources[p]==w
assert len(b['compiled_source'])==5 and b['linked_inputs_unchanged'] and len(b['linked_inputs'])>=1300 and len(b['runtime_libraries'])==125
rows=json.loads((D/'results.json').read_text());assert list(rows)==['colorify_editor_create_pixels']
v=next(iter(rows.values()));assert v['exit_code']==0 and 'ok 1 /painter-layer-ui/colorify_editor_create_pixels' in v['stdout'] and 'oncanvas visible and config enabled;1080x1920' in v['stdout']
s=json.loads((D/'runtime-seal.json').read_text());assert s=={'binary_unchanged':True,'runtime_libraries_unchanged':True,'case_count':1}
for p in ['colorify-editor-before-oncanvas-portrait.png','colorify-editor-after-oncanvas-portrait.png']:
 data=(D/p).read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',data[16:24])==(1080,1920)
print('PASS: one actual oncanvas portrait case, visible dialog + independent pixel check; original seven+seven receipt separate')
