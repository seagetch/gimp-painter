#!/usr/bin/env python3
"""Save selected test fields only, excluding Meson's raw environment."""
import hashlib,json,pathlib,shutil,sys
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
D=pathlib.Path('/workspace/shared/gimp-native-secondary')
sha=lambda b:hashlib.sha256(b).hexdigest()
paths=['app/display/'+n for n in ['gimpdisplayshell.h','gimpdisplayshell-scroll.h','gimpdisplayshell-scroll.c','gimpdisplayshell-rotate.c','gimpdisplayshell-tool-events.c','gimpdisplayshell-scale.c','gimpdisplayshell.c','gimpdisplayshell-callbacks.c','gimpdisplayshell-title.c']]+['app/tests/'+n for n in ['test-painter-navigation-events.c','test-painter-navigation-legacy-pan.inc','test-painter-navigation-old-operations.inc']]
paths += ['migration/fixtures/legacy-navigation-applied/'+n for n in ['manifest.json','rotation-oracle.inc','gimp_display_shell_start_rotating.txt','gimp_display_shell_rotate.txt','gimp_display_shell_stop_rotating.txt']]
for p in paths: assert (R/p).read_bytes()==(D/'source'/p).read_bytes(),p
for build,name in [('build','rotation-native.json'),('build-asan','rotation-sanitizer.json')]:
 if '--preserved' in sys.argv:
  row=json.loads((E/name).read_text())
 else:
  rows=[json.loads(x) for x in (D/build/'meson-logs/testlog.json').read_text().splitlines()]
  rows=[x for x in rows if x['name']=='gimp:app / painter-navigation-events'];assert len(rows)==1
  row=rows[0]
 assert row['result']=='OK' and row['returncode']==0
 assert sum(x.startswith('ok ') for x in row['stdout'].splitlines())==37
 assert row['stdout'].count('# old applied rotation ')==1008
 (E/name).write_text(json.dumps({k:row[k] for k in ['name','result','duration','returncode','stdout','stderr'] if k in row},indent=2)+'\n')
for n in ['portrait-oncanvas-rotation-m0.png','portrait-oncanvas-rotation-m1.png']:
 shutil.copyfile(D/'pan-screenshots'/n,E/n)
shutil.copyfile(D/'source/migration/inventory/additional-c-header-routes.json',E/'rotation-build-header-routes.json')
names=['rotation-native.json','rotation-sanitizer.json','rotation-expected-red.json','rotation-red-source.json','rotation-build-header-routes.json','portrait-oncanvas-rotation-m0.png','portrait-oncanvas-rotation-m1.png']
names += ['rotation-budget-'+b+'-'+g+'.json' for b in ['build','build-asan'] for g in ['original','added']]
m={'base_commit':'ce40ac4cffa5b6dba48cd9100303c3c94653a5bc','previous_checkpoint':'c49648e13db656eb27c362c9277639dd162e04b8','scope':'Default rotation origin only, original25.009 DOING; applied zoom not yet accepted','native_cases':37,'new_cases':2,'old_rotation_scenes':24,'reentries':2,'motions_per_entry':7,'point_pairs':1008,'oncanvas':True,'xvfb':{'width':1080,'height':1920,'depth':24,'authenticated':True},'source_sha256':{p:sha((R/p).read_bytes()) for p in paths},'artifact_sha256':{p:sha((E/p).read_bytes()) for p in names},'build_policy':{'optimization':'2','debug':False,'b_ndebug':False,'asan':True,'ubsan':True,'lsan':False,'dependency_instrumentation':False}}
(E/'rotation-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(json.dumps({'cases':37,'point_pairs':1008}))
