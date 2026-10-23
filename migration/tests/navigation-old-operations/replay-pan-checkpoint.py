#!/usr/bin/env python3
"""Replay original bounded proof against immutable c49648 source, not today's tree."""
import importlib.util,json,pathlib,subprocess,tempfile
R=pathlib.Path(__file__).resolve().parents[3];E=R/'migration/tests/navigation-legacy-pan'
commit='c49648e13db656eb27c362c9277639dd162e04b8'
spec=importlib.util.spec_from_file_location('pan_checkpoint',E/'check-evidence.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
m=json.loads((E/'manifest.json').read_text());paths=set(m['source_sha256'])|{'migration/inventory/additional-c-header-routes.json'}
with tempfile.TemporaryDirectory(prefix='pan-checkpoint-') as d:
 root=pathlib.Path(d)
 for p in paths:
  out=root/p;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(subprocess.check_output(['git','show',commit+':'+p],cwd=R))
 result=module.check(root,E)
 print(json.dumps(dict(checkpoint=commit,historical_only=True,result=result),sort_keys=True))
