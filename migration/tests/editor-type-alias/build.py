import pathlib,json,subprocess,hashlib
R=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');O=pathlib.Path('/workspace/shared/editor-type-alias');N=pathlib.Path('/workspace/shared/named-editor-entry');B=pathlib.Path('/workspace/shared/gimp-native-preset-gui/build');P=O/'source/app/widgets';P.mkdir(parents=True,exist_ok=True)
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
j=json.loads((N/'build.json').read_text()); results=[]
for name in ['gimppaintermybrusheditor.cpp','gimppaintermybrusheditor.h','gimppaintermybrusheditor.hpp','gimpmypaintbrusheditor.h','gimpmypaintbrusheditor.hpp']:(P/name).write_bytes((R/'app/widgets'/name).read_bytes())
def compile(src,template,out,lang=None,expect=0):
 c=template[:];c[1:1]=['-I'+str(P.parent),'-I'+str(P)];c[c.index('-o')+1]=str(O/out);c[c.index('-MF')+1]=str(O/(out+'.d'));c[-1]=str(src)
 if lang:c=[x for x in c if not x.startswith('-std=')];c.insert(1,'-std='+lang)
 x=subprocess.run(c,cwd=B,capture_output=True,text=True);results.append(dict(command=c,returncode=x.returncode,stdout=x.stdout,stderr=x.stderr));assert (x.returncode==0)==(expect==0),x.stderr
 return str(O/out)
e=next(c for c in j['compile'] if c[-1].endswith('gimppaintermybrusheditor.cpp'));editor=compile(P/'gimppaintermybrusheditor.cpp',e,'editor.o')
ct=next(c for c in j['compile'] if c[-1].endswith('mypaint-brush-editor-actions.c'));cpp=next(c for c in j['compile'] if c[-1].endswith('standalone.cpp'))
checks='''g_assert_cmpuint(a, ==, b); g_assert_cmpstr(g_type_name(a), ==, "GimpPainterMybrushEditor"); g_assert_cmpstr(g_type_name(g_type_parent(a)), ==, "GimpEditor"); return 0;'''
c=O/'alias-first.c';c.write_text('#include "config.h"\n#include <gegl.h>\n#include "widgets/gimpmypaintbrusheditor.h"\nextern GType gimp_painter_mybrush_editor_get_type(void);\nint main(void){ GType a=gimp_mypaint_brush_editor_get_type(); GType b=gimp_painter_mybrush_editor_get_type();'+checks+'}\n')
cc=O/'native-first.cpp';cc.write_text('#include "config.h"\n#include <gegl.h>\n#include <type_traits>\n#include "widgets/gimpmypaintbrusheditor.hpp"\nstatic_assert(std::is_same<GimpMypaintBrushEditor,GimpPainterMybrushEditor>::value,"same owner");\nint main(){GType b=gimp_painter_mybrush_editor_get_type(); GType a=GimpPainter::TypeTraits<GimpMypaintBrushEditor>::type();'+checks+'}\n')
objs=[compile(c,ct,'alias-first.o','c11'),compile(cc,cpp,'native-first.o','c++14')]
oldeditor=next(x for x in j['link'] if x.endswith('app_widgets_gimppaintermybrusheditor.cpp.o'));oldmain=next(x for x in j['link'] if x.endswith('app_tests_standalone.cpp.o'))
links=[]
for name,main in [('alias-first',objs[0]),('native-first',objs[1]),('ownership',oldmain)]:
 l=[editor if x==oldeditor else main if x==oldmain else x for x in j['link']];l[l.index('-o')+1]=str(O/name);subprocess.run(l,cwd=B,check=True);links.append(l)
for name,body in [('opaque','int x=sizeof(GimpMypaintBrushEditor);'),('no-old-class','GimpMypaintBrushEditorClass *klass;')]:
 p=O/(name+'.c');p.write_text('#include "config.h"\n#include <gegl.h>\n#include "widgets/gimpmypaintbrusheditor.h"\n'+body+'\n');compile(p,ct,name+'.o','c11',expect=1)
syms=subprocess.check_output(['nm','-g',str(O/'alias-first')],text=True);assert sum(x.endswith(' T gimp_mypaint_brush_editor_get_type') for x in syms.splitlines())==1
(O/'build.json').write_text(json.dumps(dict(compile=results,link=links,source_sha256={p.name:h(p) for p in P.iterdir()},unmangled_symbol=True),indent=2)+'\n')
