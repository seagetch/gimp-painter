# Run only after explicit aggregate release. No clean mutations.
import pathlib,json,hashlib,subprocess,shlex,sys
O=pathlib.Path(__file__).resolve().parent;A=pathlib.Path('/workspace/shared/gimp-clean-aggregate');B=A/'build';S=A/'source';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert len(sys.argv)==2,'expected exact released host SHA argument'
assert (A/'source-commit.txt').read_text().strip()==sys.argv[1]
rows=json.loads((B/'compile_commands.json').read_text());row=next(r for r in rows if r['file'].endswith('/test-painter-mypaint-tool.cpp'));c=shlex.split(row['command']);obj=O/'fixture.o';c[c.index('-c')+1]=str(O/'test-time-sequences.cpp')
for flag in ['-o','-MQ','-MF']:c[c.index(flag)+1]=str(obj)+('.d' if flag=='-MF'else'')
c+=['-I'+str(S/'app/tests')]
sources={str(p):sha(p)for p in [O/'test-time-sequences.cpp',S/'app/tests/test-painter-mypaint-tool.cpp',S/'app/tests/test-painter-layer-mask.inc',S/'app/tests/test-painter-layer-mask-visual.inc',S/'app/tests/test-painter-projection-notify.inc',S/'app/tools/gimppaintermybrushtool.cpp',S/'app/paint/painter-mypaint/legacy-brush.hpp']}
subprocess.run(c,cwd=B,check=True)
commands=subprocess.check_output(['ninja','-t','commands','app/tests/painter-mypaint-tool'],cwd=B,text=True).splitlines();line=next(line for line in reversed(commands)if '-o app/tests/painter-mypaint-tool ' in line);link=shlex.split(line);old=next(a for a in link if a.endswith('test-painter-mypaint-tool.cpp.o'));link=[str(obj)if a==old else a for a in link];binary=O/'timestamp-sequences';link[link.index('-o')+1]=str(binary);link+=['-Wl,--wrap=gimp_painter_session_stroke_to']
inputs={}
for a in link:
 p=pathlib.Path(a);p=p if p.is_absolute()else B/p
 if not p.is_file()or p==binary:continue
 inputs[str(p.resolve())]=sha(p)
 if p.suffix=='.a'and p.read_bytes()[:8]==b'!<thin>\n':
  for name in subprocess.check_output(['ar','t',str(p)],text=True).splitlines():
   q=pathlib.Path(name);inputs[str(q.resolve())]=sha(q)
subprocess.run(link,cwd=B,check=True);assert all(sha(pathlib.Path(p))==h for p,h in inputs.items());assert all(sha(pathlib.Path(p))==h for p,h in sources.items())
(O/'build.json').write_text(json.dumps({'host':sys.argv[1],'compile':c,'link':link,'source_inputs':sources,'linked_inputs':inputs,'unchanged':True,'binary_sha256':sha(binary)},indent=2)+'\n')
