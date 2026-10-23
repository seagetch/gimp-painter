import pathlib,hashlib,json,tarfile,sys
W=pathlib.Path('/workspace/shared/gimp-native-preset-gui');S=W/'source';E=W/'tool-registration-evidence';prefix=sys.argv[1]
paths={'app/tools/gimp-tools.c','app/tools/gimp-tools.h','app/tests/test-tool-registration.c','app/tests/test-tool-registration.cpp','app/tests/tests.c','app/core/gimpcontainer.c','app/core/gimpcontainer.h','app/core/gimplist.c','app/core/gimptoolinfo.c','app/core/gimptoolinfo.h','app/core/gimptooloptions.c','app/core/gimptooloptions.h','app/core/gimpcontext.c','app/core/gimpcontext.h','app/core/gimp.c','app/core/gimpobject.c','app/core/gimpviewable.c','app/core/gimptoolitem.c','app/painter/object-ref.hpp','app/painter/boundary.hpp'}
if prefix=='sanitizer':paths |= set(json.loads((E/'asan-build.json').read_text())['sources_sha256'])|{'migration/tests/build_tool_registration_sanitizers.py'}
# Explicitly pin uninstrumented, authorized engine overlay and excluded UI providers.
for p in ['app/core/gimpimage-undo.c','app/core/gimpimage-undo.h','app/paint/painter-mypaint/engine.cpp','app/paint/painter-mypaint/engine.hpp','app/paint/painter-mypaint/legacy-brush.hpp','app/core/gimppaintermybrush.cpp','app/display/gimppaintercanvasui.cpp','app/widgets/gimpcellrendererpopup.cpp']:
 if(S/p).exists():paths.add(p)
seal={p:hashlib.sha256((S/p).read_bytes()).hexdigest() for p in sorted(paths)}
(E/(prefix+'-source-sha256.json')).write_text(json.dumps(seal,indent=2)+'\n')
with tarfile.open(E/(prefix+'-tested-sources.tar.gz'),'w:gz') as t:
 for p in sorted(paths):t.add(S/p,arcname=p)
print('sealed',prefix,len(paths),'explicit checkpoint sources, not full application')
