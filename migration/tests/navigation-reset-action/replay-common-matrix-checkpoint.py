#!/usr/bin/env python3
"""Replay committed original25.009 proof, not a new runtime claim."""
import importlib.util,json,pathlib,subprocess,tempfile
R=pathlib.Path(__file__).resolve().parents[3];E=R/'migration/tests/navigation-item-geometry';commit='0f71f055a5fc3bb2237a56303738a41d5d503663'
spec=importlib.util.spec_from_file_location('matrix_checkpoint',E/'check-parent.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
m=json.loads((E/'manifest.json').read_text());paths=set(m['source_sha256'])
for name in ['render-source-duty-review.json','primitive-source-duty-review.json','navigation-source-duty-review.json']:paths.update(json.loads((E/name).read_text())['current_sources_sha256'])
with tempfile.TemporaryDirectory(prefix='matrix-checkpoint-') as d:
 root=pathlib.Path(d)
 for p in paths:
  out=root/p;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(subprocess.check_output(['git','show',commit+':'+p],cwd=R))
 print(json.dumps(dict(checkpoint=commit,historical_only=True,result=module.check(root,E)),sort_keys=True))
