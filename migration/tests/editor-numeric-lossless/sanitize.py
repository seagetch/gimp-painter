from pathlib import Path
import json,subprocess,hashlib
O=Path('/workspace/shared/editor-controls-audit');N=Path('/workspace/shared/named-editor-entry');B=Path('/workspace/shared/gimp-native-preset-gui/build');j=json.loads((O/'green-build.json').read_text());old=json.loads((N/'asan-build.json').read_text());commands=[]
for c in j['compile']:
 c=c[:];obj=O/('asan-'+Path(c[c.index('-o')+1]).name);c[c.index('-o')+1]=str(obj);c[c.index('-MF')+1]=str(obj)+'.d';c[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-O1','-g0'];c=[x for x in c if x!='-O2'];subprocess.run(c,cwd=B,check=True);commands.append(c)
l=old['link'][:];l=[str(O/'asan-green-editor.o') if x.endswith('app_widgets_gimppaintermybrusheditor.cpp.o') else str(O/'asan-green-fixture.o') if x.endswith('app_tests_standalone.cpp.o') else x for x in l];l[l.index('-o')+1]=str(O/'green-asan');l.insert(1,'-Wl,--wrap=gimp_painter_mybrush_options_set_json');subprocess.run(l,cwd=B,check=True);(O/'asan-build.json').write_text(json.dumps({'compile':commands,'link':l,'scope':'2 freshly compiled TUs plus 5 unchanged predecessor instrumented TUs; native dependencies uninstrumented'},indent=2)+'\n')
