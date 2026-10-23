#!/usr/bin/env python3
import importlib.util,json,pathlib,tempfile,shutil,hashlib
E=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('check_rotation',E/'check-rotation.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
controls=[]
def trial(label,edit,artifact=None):
 with tempfile.TemporaryDirectory(prefix='rotation-negative-') as d:
  e=pathlib.Path(d);shutil.copytree(E,e,dirs_exist_ok=True)
  m=json.loads((e/'rotation-manifest.json').read_text());edit(e,m)
  if artifact:m['artifact_sha256'][artifact]=hashlib.sha256((e/artifact).read_bytes()).hexdigest()
  (e/'rotation-manifest.json').write_text(json.dumps(m))
  try:module.check(e=e)
  except (ValueError,FileNotFoundError):controls.append(dict(name=label,result='rejected'));return
  raise AssertionError(label+' accepted')
def change_receipt(e,name,edit):
 p=e/name;o=json.loads(p.read_text());edit(o);p.write_text(json.dumps(o))
trial('missing source',lambda e,m:m['source_sha256'].__setitem__('missing-source.c','0'))
trial('wrong source hash',lambda e,m:m['source_sha256'].__setitem__('app/display/gimpdisplayshell.h','0'))
trial('wrong pair count',lambda e,m:m.__setitem__('point_pairs',1007))
trial('oncanvas disabled',lambda e,m:m.__setitem__('oncanvas',False))
trial('landscape',lambda e,m:m['xvfb'].__setitem__('width',1920))
trial('assertions disabled',lambda e,m:m['build_policy'].__setitem__('b_ndebug',True))
trial('failed sanitizer',lambda e,m:change_receipt(e,'rotation-sanitizer.json',lambda o:o.__setitem__('result','FAIL')),'rotation-sanitizer.json')
trial('missing new render case',lambda e,m:change_receipt(e,'rotation-native.json',lambda o:o.__setitem__('stdout',o['stdout'].replace('/old-operations/rotation-render','/removed'))),'rotation-native.json')
trial('missing old pair',lambda e,m:change_receipt(e,'rotation-native.json',lambda o:o.__setitem__('stdout',o['stdout'].replace('# old applied rotation ','# missing ',1))),'rotation-native.json')
trial('raw environment',lambda e,m:change_receipt(e,'rotation-native.json',lambda o:o.__setitem__('env',{})),'rotation-native.json')
trial('false expected red',lambda e,m:change_receipt(e,'rotation-expected-red.json',lambda o:o.__setitem__('returncode',0)),'rotation-expected-red.json')
trial('changed pre-fix source',lambda e,m:(e/'rotation-red-source/gimpdisplayshell-rotate.c.txt').write_text('changed'))
positive=module.check();out=dict(positive=positive,negative_controls=controls)
(E/'rotation-negative-controls.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
