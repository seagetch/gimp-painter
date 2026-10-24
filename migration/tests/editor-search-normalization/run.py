from pathlib import Path
import subprocess,json,shlex,hashlib,os,sys
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');A=Path('/workspace/shared/editor-switch-default');numeric=False;candidate=True;W=Path('/workspace/shared/editor-list-history');W.mkdir(exist_ok=True);B=Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();C=json.loads((B/'compile_commands.json').read_text());row=next(x for x in C if x['file']=='../source/app/tests/test-painter-mypaint-editor.cpp')
head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip()
owned={}
for path,actual in [('app/widgets/gimppaintermybrusheditor.cpp',W/'green/editor.cpp'),('app/paint/painter-mypaint-surface/gimp-painter-options.cpp',S/'app/paint/painter-mypaint-surface/gimp-painter-options.cpp'),('app/paint/painter-mypaint/resource.cpp',S/'app/paint/painter-mypaint/resource.cpp')]:
 committed=subprocess.check_output(['git','show',head+':'+path],cwd=R);assert actual.read_bytes()==(R/path).read_bytes(),path
 if path!='app/widgets/gimppaintermybrusheditor.cpp':assert actual.read_bytes()==committed,path
 owned[path]=sha(actual)
(W/'owned-source.json').write_text(json.dumps({'commit':head,'owned_sha256':owned,'dependency_baseline':(B.parent/'source-commit.txt').read_text().strip(),'scope':'exact committed0a dependencies plus explicit candidate editor search-only normalization; not whole HEAD', 'editor_predecessor_sha256':sha(W/'before-editor.cpp')},indent=2)+'\n')
erow=next(x for x in C if x['file']=='../source/app/widgets/gimppaintermybrusheditor.cpp');ec=shlex.split(erow['command']);ec[1:1]=['-iquote',str(S/'app/widgets')];ec[ec.index('-o')+1]=str(W/'green/editor.o');ec[ec.index('-MF')+1]=str(W/'green/editor.o.d');ec[-1]=str(W/'green/editor.cpp');subprocess.run(ec,cwd=B,check=True)
text=(W/'fixture.cpp').read_text()
(W/'fixture.cpp').write_text(text);c=shlex.split(row['command']);c[1:1]=['-iquote',str(S/'app/tests')];c[c.index('-o')+1]=str(W/'fixture.o');c[c.index('-MF')+1]=str(W/'fixture.o.d');c[-1]=str(W/'fixture.cpp');subprocess.run(c,cwd=B,check=True)
l=shlex.split(next(x for x in reversed(subprocess.check_output(['ninja','-C',str(B),'-t','commands','app/tests/painter-mypaint-editor'],text=True).splitlines())if ' -o app/tests/painter-mypaint-editor 'in x));l[l.index('-o')+1]=str(W/'test');l=[str(W/'fixture.o')if x==row['output']else x for x in l]
if numeric:l.insert(1,'-Wl,--wrap=gimp_painter_mybrush_options_set_json')
l.insert(l.index('-Wl,--start-group')+1,str(W/'green/editor.o'))
inputs={}
for a in l:
 p=Path(a);p=p if p.is_absolute()else B/p
 if not p.is_file()or p==W/'test':continue
 inputs[str(p.resolve())]=sha(p)
 if p.suffix=='.a'and p.read_bytes()[:8]==b'!<thin>\n':
  for n in subprocess.check_output(['ar','t',str(p)],text=True).splitlines():
   n=Path(n);n=n if n.is_absolute()else p.parent/n;inputs[str(n.resolve())]=sha(n)
(W/'linked-before.json').write_text(json.dumps(inputs,indent=2)+'\n');(W/'commands.json').write_text(json.dumps({'compile':c,'editor_compile':ec,'link':l},indent=2)+'\n');subprocess.run(l,cwd=B,check=True);assert inputs=={p:sha(Path(p))for p in inputs};(W/'linked-after.json').write_text(json.dumps(inputs,indent=2)+'\n')
t=next(t for t in json.loads((B/'meson-info/intro-tests.json').read_text())if t['name']=='painter-editor-named-entry');env=os.environ.copy();ld=env.get('LD_LIBRARY_PATH','');env.update(t['env']);env['LD_LIBRARY_PATH']=env.get('LD_LIBRARY_PATH','')+':'+ld;env['GIMP_TEST_LIST_SCREENSHOT']=str(W/'matrix-portrait.png');env['GIMP_TEST_CONTROLS_SCREENSHOT']=str(W/'numeric-portrait.png');env.pop('GIMP_TEST_EDITOR_NAMED_ENTRY',None)
for key,d in [('HOME','home'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_STATE_HOME','state'),('XDG_RUNTIME_DIR','runtime'),('TMPDIR','tmp')]:
 p=W/'profile'/d;p.mkdir(parents=True,exist_ok=True);env[key]=str(p)
run=['python3',str(S/'tools/run-painter-xvfb.py'),str(W/'test')];p=subprocess.run(run,cwd=B,env=env,capture_output=True,text=True,timeout=180);(W/'run.log').write_text(p.stdout+p.stderr);(W/'result.json').write_text(json.dumps({'exit':p.returncode,'command':run,'binary_sha256':sha(W/'test'),'fixture_sha256':sha(W/'fixture.cpp')},indent=2)+'\n');print(p.returncode);print(p.stdout[-3000:]);print(p.stderr[-1000:])

after={p:sha(Path(p))for p in inputs};assert after==inputs;(W/'runtime-inputs-after.json').write_text(json.dumps(after,indent=2)+'\n')
