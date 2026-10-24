# Run only after explicit aggregate release. No clean mutations.
import pathlib,json,hashlib,subprocess,shlex,sys
O=pathlib.Path(__file__).resolve().parent;A=pathlib.Path('/workspace/shared/gimp-clean-aggregate');B=A/'build';S=A/'source';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert len(sys.argv)==3,'expected exact released host SHA and mode'
mode=sys.argv[2];assert mode in ['sanitizer','reverse-mutant','boundary-mutant','equal-mutant']
sanitizer=mode=='sanitizer'
provider=O/(mode+'-provider.cpp');data=(S/'app/tools/gimppaintermybrushtool.cpp').read_text()
needle='elapsed<=std::numeric_limits<gint32>::max()'
if mode=='reverse-mutant':assert data.count(needle)==1;data=data.replace(needle,'true')
if mode=='boundary-mutant':assert data.count(needle)==1;data=data.replace(needle,'elapsed<std::numeric_limits<gint32>::max()')
if mode=='equal-mutant':
 needle2='if(paint&&!boundary<gboolean>'
 assert data.count(needle2)==1;data=data.replace(needle2,'if(paint&&state.seconds==0)return;\n  '+needle2)
provider.write_text(data)
flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer']if sanitizer else []
assert (A/'source-commit.txt').read_text().strip()==sys.argv[1]
rows=json.loads((B/'compile_commands.json').read_text());row=next(r for r in rows if r['file'].endswith('/test-painter-mypaint-tool.cpp'));c=shlex.split(row['command']);obj=O/(mode+'-fixture.o');c[c.index('-c')+1]=str(O/'test-time-sequences.cpp')
for flag in ['-o','-MQ','-MF']:c[c.index(flag)+1]=str(obj)+('.d' if flag=='-MF'else'')
c+=['-I'+str(S/'app/tests')]+flags
sources={str(p):sha(p)for p in [O/'test-time-sequences.cpp',S/'app/tests/test-painter-mypaint-tool.cpp',S/'app/tests/test-painter-layer-mask.inc',S/'app/tests/test-painter-layer-mask-visual.inc',S/'app/tests/test-painter-projection-notify.inc',S/'app/tools/gimppaintermybrushtool.cpp',S/'app/paint/painter-mypaint/legacy-brush.hpp']}
subprocess.run(c,cwd=B,check=True)
prow=next(r for r in rows if r['file'].endswith('/gimppaintermybrushtool.cpp'));pc=shlex.split(prow['command']);pobj=O/(mode+'-provider.o');pc[pc.index('-c')+1]=str(provider)
for flag in ['-o','-MQ','-MF']:pc[pc.index(flag)+1]=str(pobj)+('.d'if flag=='-MF'else'')
pc+=['-I'+str(S/'app/tools')]+flags;subprocess.run(pc,cwd=B,check=True);sources[str(provider)]=sha(provider)
commands=subprocess.check_output(['ninja','-t','commands','app/tests/painter-mypaint-tool'],cwd=B,text=True).splitlines();line=next(line for line in reversed(commands)if '-o app/tests/painter-mypaint-tool ' in line);link=shlex.split(line);old=next(a for a in link if a.endswith('test-painter-mypaint-tool.cpp.o'));link=[str(obj)if a==old else a for a in link];binary=O/('timestamp-'+mode);link[link.index('-o')+1]=str(binary);link.insert(3,str(pobj));link+=['-Wl,--wrap=gimp_painter_session_stroke_to']+flags
inputs={}
for a in link:
 p=pathlib.Path(a);p=p if p.is_absolute()else B/p
 if not p.is_file()or p==binary:continue
 inputs[str(p.resolve())]=sha(p)
 if p.suffix=='.a'and p.read_bytes()[:8]==b'!<thin>\n':
  for name in subprocess.check_output(['ar','t',str(p)],text=True).splitlines():
   q=pathlib.Path(name);inputs[str(q.resolve())]=sha(q)
subprocess.run(link,cwd=B,check=True);assert all(sha(pathlib.Path(p))==h for p,h in inputs.items());assert all(sha(pathlib.Path(p))==h for p,h in sources.items())
(O/(mode+'-build.json')).write_text(json.dumps({'host':sys.argv[1],'compile':c,'provider_compile':pc,'mode':mode,'link':link,'source_inputs':sources,'linked_inputs':inputs,'unchanged':True,'binary_sha256':sha(binary)},indent=2)+'\n')
