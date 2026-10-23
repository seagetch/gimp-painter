import pathlib,subprocess,json,os,shutil
R=pathlib.Path('<checkout>');E=pathlib.Path(__file__).parent;S=R/'app/paint/painter-mypaint-surface';D=E/'mutant-source';D.mkdir(exist_ok=True)
for p in S.glob('*.hpp'):shutil.copy2(p,D/p.name)
shutil.copy2(S/'gegl-surface.cpp',D/'gegl-surface.cpp')
p=D/'native-pixels.hpp';original=p.read_text();flags=subprocess.check_output(['pkg-config','--cflags','--libs','gegl-0.4','lcms2'],text=True).split();results=[]
mutants=[('accept-noncanonical','if(f!=babl_format_with_space((model+" "+name).c_str(),space)||babl_format_get_bytes_per_pixel(f)!=bytes)','if(false)','/precision-refusal/noncanonical-order'),('narrow-u32-through-float','case U32:return load<std::uint32_t>(p)/4294967295.;','case U32:return float(load<std::uint32_t>(p)/4294967295.);','/precision/quantization-native-lsb')]

for name,old,new,test in mutants:
 assert original.count(old)==1;p.write_text(original.replace(old,new));exe=E/('mutant-'+name)
 cmd=['c++','-std=c++14','-O1','-I'+str(R/'app'),'-I'+str(S),str(S/('tests/native-precision.cpp' if name=='narrow-u32-through-float' else 'tests/precision-refusal.cpp')),str(D/'gegl-surface.cpp'),*flags,'-o',str(exe)];subprocess.run(cmd,check=True)
 env=dict(os.environ,GEGL_SWAP='RAM',GEGL_USE_OPENCL='no');r=subprocess.run([str(exe),'--tap','-p',test],capture_output=True,text=True,env=env);(E/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode!=0 and 'not ok' in r.stdout+r.stderr
 results.append(dict(name=name,exit_code=r.returncode,test=test,rejected=True,old=old,new=new))
p.write_text(original);(E/'mutations.json').write_text(json.dumps(results,indent=2)+'\n')
