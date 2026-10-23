#!/usr/bin/env python3
"""Replay the immutable pre-Space zoom proof; not a fresh current-source build."""
import importlib.util,json,pathlib,subprocess,tempfile
R=pathlib.Path(__file__).resolve().parents[3];E=R/'migration/tests/navigation-old-zoom'
commit='4ec50d3626668534f5121400f38bdb0af4960db0'
spec=importlib.util.spec_from_file_location('zoom_checkpoint',E/'check-zoom.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
m=json.loads((E/'manifest.json').read_text());paths=set(m['source_sha256'])|{'migration/inventory/additional-c-header-routes.json'}
with tempfile.TemporaryDirectory(prefix='zoom-checkpoint-') as d:
 root=pathlib.Path(d)
 for p in paths:
  out=root/p;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(subprocess.check_output(['git','show',commit+':'+p],cwd=R))
 print(json.dumps(dict(checkpoint=commit,historical_only=True,result=module.check(root,E)),sort_keys=True))
