#!/usr/bin/env python3
"""Replay immutable predecessors; never new runtime acceptance."""
import importlib.util,json,pathlib,subprocess,tempfile
R=pathlib.Path(__file__).resolve().parents[3]
for name,checker,commit in [('navigation-item-geometry','check-parent.py','0f71f055a5fc3bb2237a56303738a41d5d503663'),('navigation-reset-action','check-reset.py','c8ff52500a')]:
 E=R/'migration/tests'/name;commit=subprocess.check_output(['git','rev-parse',commit],cwd=R).decode().strip();spec=importlib.util.spec_from_file_location(name,E/checker);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);m=json.loads((E/'manifest.json').read_text());paths=set(m['source_sha256'])
 if name=='navigation-item-geometry':
  for review in ['render-source-duty-review.json','primitive-source-duty-review.json','navigation-source-duty-review.json']:paths.update(json.loads((E/review).read_text())['current_sources_sha256'])
 else:paths.update(a['path'] for a in json.loads((E/'retrospective-runtime-dependencies.json').read_text())['assets'])
 with tempfile.TemporaryDirectory(prefix='navigation-predecessor-') as d:
  root=pathlib.Path(d)
  for p in paths:
   out=root/p;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(subprocess.check_output(['git','show',commit+':'+p],cwd=R))
  print(json.dumps(dict(checkpoint=commit,historical_only=True,result=module.check(root,E)),sort_keys=True))
