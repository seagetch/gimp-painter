import pathlib,subprocess,json,os,time
S=pathlib.Path('<native>/source');B=S.parent/'build-port';E=pathlib.Path(__file__).parent;p=S/'app/paint/painter-mypaint-surface/paint-core.cpp';original=p.read_text();old='gegl_color_get_pixel(gimp_context_get_foreground(GIMP_CONTEXT(options.get())),GeglSurface::evaluation_format(gimp_drawable_get_buffer(d)),rgba);';new='gegl_color_get_pixel(gimp_context_get_foreground(GIMP_CONTEXT(options.get())),babl_format("R\'G\'B\'A double"),rgba);';assert original.count(old)==1
try:
 p.write_text(original.replace(old,new));subprocess.run(['meson','compile','-C',str(B),'-j4','painter-mypaint-precision'],check=True)
 r=subprocess.run(['python3',str(S/'tools/run-painter-xvfb.py'),'meson','test','-C',str(B),'--no-rebuild','--num-processes','1','--print-errorlogs','--logbase','color-placement-mutant','--test-args=-p /painter-precision/color-placement-ingress','painter-mypaint-precision'],capture_output=True,text=True);(E/'native-mutant.log').write_text(r.stdout+r.stderr);assert r.returncode!=0 and 'assertion failed' in r.stdout+r.stderr
 (E/'native-mutant.json').write_text(json.dumps(dict(name='missing-context-to-profile-ingress',exit_code=r.returncode,rejected=True,old=old,new=new),indent=2)+'\n')
finally:
 p.write_text(original);subprocess.run(['meson','compile','-C',str(B),'-j4','painter-mypaint-precision'],check=True)
subprocess.run(['python3',str(S/'tools/run-painter-xvfb.py'),'meson','test','-C',str(B),'--no-rebuild','--num-processes','1','--print-errorlogs','--logbase','color-placement-restored','--test-args=-p /painter-precision/color-placement-ingress','painter-mypaint-precision'],check=True)
