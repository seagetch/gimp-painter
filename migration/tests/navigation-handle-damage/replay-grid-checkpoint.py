#!/usr/bin/env python3
"""Replay immutable pre-item grid proof; no new runtime claim."""
import importlib.util,json,pathlib,subprocess,tempfile
R=pathlib.Path(__file__).resolve().parents[3];E=R/'migration/tests/navigation-grid-damage'
commit='167f2cb6364c00fbcb3fed365d0c4242846cc6de'
spec=importlib.util.spec_from_file_location('grid_checkpoint',E/'check-grid.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
m=json.loads((E/'manifest.json').read_text())
with tempfile.TemporaryDirectory(prefix='grid-checkpoint-') as d:
 root=pathlib.Path(d)
 for p in m['source_sha256']:
  out=root/p;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(subprocess.check_output(['git','show',commit+':'+p],cwd=R))
 print(json.dumps(dict(checkpoint=commit,historical_only=True,result=module.check(root,E)),sort_keys=True))
