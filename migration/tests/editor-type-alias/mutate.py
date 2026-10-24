import pathlib,json,subprocess
O=pathlib.Path('/workspace/shared/editor-type-alias');B=pathlib.Path('/workspace/shared/gimp-native-preset-gui/build');j=json.loads((O/'build.json').read_text());p=O/'source/app/widgets/gimppaintermybrusheditor.cpp';s=p.read_text();old='GType gimp_mypaint_brush_editor_get_type (void)\n{\n  return gimp_painter_mybrush_editor_get_type ();\n}'
assert s.count(old)==1;m=O/'wrong-type.cpp';m.write_text(s.replace(old,old.replace('return gimp_painter_mybrush_editor_get_type ();','return GIMP_TYPE_EDITOR;')))
c=j['compile'][0]['command'][:];c[-1]=str(m);c[c.index('-o')+1]=str(O/'wrong-type.o');c[c.index('-MF')+1]=str(O/'wrong-type.d');subprocess.run(c,cwd=B,check=True)
l=j['link'][0][:];l=[str(O/'wrong-type.o') if x==str(O/'editor.o') else x for x in l];l[l.index('-o')+1]=str(O/'wrong-type');subprocess.run(l,cwd=B,check=True)
x=subprocess.run([str(O/'wrong-type')],cwd=O,capture_output=True,text=True);assert x.returncode!=0;assert 'a == b' in x.stderr or 'a == b' in x.stdout;(O/'mutant.json').write_text(json.dumps(dict(compile=c,link=l,returncode=x.returncode,stdout=x.stdout,stderr=x.stderr),indent=2)+'\n')
