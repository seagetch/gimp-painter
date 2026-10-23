#!/usr/bin/env python3
"""Replay immutable pre-grid Space proof; no new runtime claim."""
import importlib.util,json,pathlib,subprocess,tempfile
R=pathlib.Path(__file__).resolve().parents[3];E=R/'migration/tests/navigation-old-space'
commit='a16521caf3b6a75ff956bce7c6e20df5c4953fac'
spec=importlib.util.spec_from_file_location('space_checkpoint',E/'check-space.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
m=json.loads((E/'manifest.json').read_text())
with tempfile.TemporaryDirectory(prefix='space-checkpoint-') as d:
 root=pathlib.Path(d)
 for p in m['source_sha256']:
  out=root/p;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(subprocess.check_output(['git','show',commit+':'+p],cwd=R))
 print(json.dumps(dict(checkpoint=commit,historical_only=True,result=module.check(root,E)),sort_keys=True))
