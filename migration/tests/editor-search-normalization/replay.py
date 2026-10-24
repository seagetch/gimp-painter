from pathlib import Path
import subprocess,os
ROOT=Path('/workspace/shared/editor-list-history');base=(ROOT/'run.py').read_text()
fixtures={'numeric':Path('/workspace/shared/editor-switch-default/numeric-candidate/fixture.cpp'),'curve':Path('/workspace/scratch/0015fc5d1a03/gimp-painter/migration/tests/editor-curve-matrix/fixture.cpp'),'selector':Path('/workspace/scratch/0015fc5d1a03/gimp-painter/migration/tests/editor-selector-surface/fixture.cpp')}
for name,source in fixtures.items():
 W=ROOT/('replay-'+name);W.mkdir(exist_ok=True);(W/'fixture.cpp').write_bytes(source.read_bytes())
 s=base.replace("W=Path('/workspace/shared/editor-list-history')", "ROOT=Path('/workspace/shared/editor-list-history');W=ROOT/'replay-%s'"%name).replace("W/'green/", "ROOT/'green/").replace("W/'before-editor.cpp'", "ROOT/'before-editor.cpp'").replace("W/'test'", "ROOT/'test'")
 if name=='numeric':s=s.replace('numeric=False','numeric=True')
 s=s.replace("env['GIMP_TEST_LIST_SCREENSHOT']=str(W/'matrix-portrait.png')", "env['GIMP_TEST_LIST_SCREENSHOT']=str(W/'matrix-portrait.png');env['GIMP_TEST_CURVE_SCREENSHOT']=str(W/'matrix-portrait.png');env['GIMP_TEST_SELECTOR_SCREENSHOT']=str(W/'matrix-portrait.png')")
 (W/'run.py').write_text(s)
 with (W/'driver.log').open('w')as log:subprocess.run(['python3',str(W/'run.py')],stdout=log,stderr=subprocess.STDOUT,check=True)
 print(name,(W/'result.json').read_text(),flush=True)
