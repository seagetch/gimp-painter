import pathlib,json,hashlib,subprocess,shlex,os
R=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter'); W=pathlib.Path('/workspace/shared/gimp-native-preset-gui'); B=W/'build'; S=W/'source'; O=pathlib.Path('/workspace/shared/named-editor-entry'); P=O/'source';P.mkdir(exist_ok=True)
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for row in json.loads((R/'migration/tests/nonincremental-display-21013/excluded-provider-seal.json').read_text()): assert h(S/row['path'])==row['tested'],row['path']
paths=(O/'owned-paths.txt').read_text().splitlines()
for p in paths:
 q=P/p;q.parent.mkdir(exist_ok=True,parents=True);q.write_bytes((R/p).read_bytes())
for name in ['painter','paint']:
 q=P/'app'/name
 if not q.exists() and not q.is_symlink():q.symlink_to(S/'app'/name,target_is_directory=True)
# Runtime assets from unchanged predecessor, with the one added GMenu.
for name in ['data','etc','icons','themes']:
 q=P/name
 if not q.exists() and not q.is_symlink():q.symlink_to(S/name,target_is_directory=True)
for q in (S/'menus').iterdir():
 if q.is_file() and not(P/'menus'/q.name).exists():(P/'menus'/q.name).symlink_to(q)
text=(R/'app/tests/test-painter-mypaint-editor.cpp').read_text();text=text[:text.index('static void constructors_and_shared_model()')]
text+='\n#include "test-painter-editor-entry.inc"\nint main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);add_editor_case("/painter-editor/named-entry/constructor",named_editor_constructor);add_editor_case("/painter-editor/named-entry/menu-save",named_editor_menu_save);add_editor_case("/painter-editor/named-entry/menu-lifetime",named_editor_menu_lifetime);g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(gimp->app);g_clear_object(&gimp->app);g_free(original_search);return result;}\n'
(P/'app/tests/standalone.cpp').write_text(text)
entries=json.loads((B/'compile_commands.json').read_text());commands=[];objects=[]
def compile(path,template=None):
 e=next(x for x in entries if x['file'].endswith('/'+(template or path)) and (path!='app/tests/tests.c' or 'painter-mypaint-editor.p' in x['output']))
 c=shlex.split(e['command']); obj=O/(path.replace('/','_')+'.o'); dep=pathlib.Path(str(obj)+'.d')
 c[c.index('-o')+1]=str(obj);c[c.index('-MF')+1]=str(dep);c[-1]=str(P/path if (P/path).exists() else S/path)
 c[1:1]=['-I'+str(P/'app'),'-I'+str(P/'app/widgets'),'-I'+str(P/'app/tests'),'-I'+str(S/'app/widgets')]
 commands.append(c);subprocess.run(c,cwd=B,check=True);objects.append(str(obj));return e
for p,t in [('app/widgets/gimppaintermybrusheditor.cpp',None),('app/actions/actions.c',None),('app/actions/mypaint-brush-editor-actions.c','app/actions/brush-editor-actions.c'),('app/menus/menus.c',None),('app/dialogs/dialogs-constructors.c',None)]:compile(p,t)
fixture=compile('app/tests/standalone.cpp','app/tests/test-painter-mypaint-editor.cpp'); tc=compile('app/tests/tests.c')
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/painter-mypaint-editor'],cwd=B,text=True).splitlines()[-1]);link[link.index('-o')+1]=str(O/'named-editor');link=[x for x in link if x not in [fixture['output'],tc['output']]];link[1:1]=objects
# Every linked thin-archive member and dynamic library is frozen before/after.
inputs={}
for a in link:
 p=B/a
 if p.is_file() and p.suffix in ['.a','.o'] or (p.is_file() and '.so' in p.name):
  inputs[str(p.resolve())]=h(p)
  if p.suffix=='.a':
   for m in subprocess.check_output(['ar','t',str(p)],cwd=B,text=True).splitlines():
    q=pathlib.Path(m);q=q if q.is_absolute() else p.parent/q
    if not q.exists():q=B/m
    if q.is_file():inputs[str(q.resolve())]=h(q)
(O/'linked-before.json').write_text(json.dumps(inputs,indent=2)+'\n')
subprocess.run(link,cwd=B,check=True)
assert all(h(pathlib.Path(p))==v for p,v in inputs.items())
(O/'build.json').write_text(json.dumps({'compile':commands,'link':link,'executable_sha256':h(O/'named-editor'),'source_sha256':{str(p.relative_to(P)):h(p) for p in P.rglob('*') if p.is_file() and not p.is_symlink()},'linked_inputs':inputs},indent=2)+'\n')
