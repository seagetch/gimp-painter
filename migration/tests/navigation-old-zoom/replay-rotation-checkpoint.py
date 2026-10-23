#!/usr/bin/env python3
"""Replay the immutable rotation checkpoint, not current zoom source evidence."""
import importlib.util,json,pathlib,subprocess,tempfile
R=pathlib.Path(__file__).resolve().parents[3];E=R/'migration/tests/navigation-old-operations'
commit='0a10f021f4346cc8595c6072e56940d1321a8e3f'
spec=importlib.util.spec_from_file_location('rotation_checkpoint',E/'check-rotation.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
m=json.loads((E/'rotation-manifest.json').read_text());paths=set(m['source_sha256'])|{'migration/inventory/additional-c-header-routes.json'}
paths.update(str(p.relative_to(R)) for p in (R/'migration/fixtures/legacy-navigation-applied').glob('*') if p.is_file() and not p.name.startswith('zoom-'))
with tempfile.TemporaryDirectory(prefix='rotation-checkpoint-') as d:
 root=pathlib.Path(d)
 for p in paths:
  out=root/p;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(subprocess.check_output(['git','show',commit+':'+p],cwd=R))
 result=module.check(root,E)
 print(json.dumps(dict(checkpoint=commit,historical_only=True,result=result),sort_keys=True))
