import pathlib,subprocess,json,os,shutil
R=pathlib.Path('<checkout>');E=pathlib.Path(__file__).parent;D=E/'stride-mutant';S=D/'app/paint/painter-mypaint-surface';S.mkdir(parents=True,exist_ok=True)
for p in (R/'app/paint/painter-mypaint-surface').iterdir():
 if p.suffix in ['.hpp','.cpp']:shutil.copyfile(p,S/p.name)
p=S/'buffer-read.hpp';old='gint (rowstride), GEGL_ABYSS_NONE);';new='gint (row), GEGL_ABYSS_NONE);';s=p.read_text();assert s.count(old)==1;p.write_text(s.replace(old,new))
flags=subprocess.check_output(['pkg-config','--cflags','--libs','gegl-0.4'],text=True).split();exe=E/'stride-mutant-test'
subprocess.run(['c++','-std=c++14','-O1','-I'+str(D/'app'),'-I'+str(R/'app'),'-I'+str(R/'app/paint/painter-mypaint-surface'),str(R/'app/paint/painter-mypaint-surface/tests/buffer-read.cpp'),str(S/'gegl-surface.cpp'),*flags,'-o',str(exe)],check=True)
r=subprocess.run([str(exe),'--tap','-p','/buffer-read/native-formats-and-partial-regions'],capture_output=True,text=True,env=dict(os.environ,GEGL_SWAP='RAM',GEGL_USE_OPENCL='no'));(E/'stride-mutant.log').write_text(r.stdout+r.stderr);assert r.returncode!=0 and 'not ok'in r.stdout+r.stderr;(E/'stride-mutant.json').write_text(json.dumps(dict(name='replace-padded-output-stride-with-packed-row',exit_code=r.returncode,rejected=True,old=old,new=new),indent=2)+'\n')
