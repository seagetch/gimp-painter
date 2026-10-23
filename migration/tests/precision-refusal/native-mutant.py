import pathlib,subprocess,json,os,time
S=pathlib.Path('<native>/source');B=S.parent/'build-port';E=pathlib.Path(__file__).parent;p=S/'app/core/gimpimage-color-profile.c';original=p.read_text();old='if (dest_profile &&\n      ! gimp_image_validate_color_profile (image, dest_profile, NULL, error))\n    return FALSE;';new=old.replace('return FALSE','return TRUE');assert original.count(old)==1

try:
 p.write_text(original.replace(old,new));subprocess.run(['meson','compile','-C',str(B),'-j4','painter-mypaint-precision'],check=True)
 r=subprocess.run(['python3',str(S/'tools/run-painter-xvfb.py'),'meson','test','-C',str(B),'--no-rebuild','--num-processes','1','--print-errorlogs','--logbase','precision-refusal-mutant','--test-args=-p /painter-precision/unsupported-profile-refusal','painter-mypaint-precision'],capture_output=True,text=True);(E/'native-mutant.log').write_text(r.stdout+r.stderr);assert r.returncode!=0 and 'not ok' in r.stdout+r.stderr
 (E/'native-mutant.json').write_text(json.dumps(dict(name='false-success-on-unsupported-profile',exit_code=r.returncode,rejected=True,old=old,new=new),indent=2)+'\n')
finally:
 p.write_text(original);subprocess.run(['meson','compile','-C',str(B),'-j4','painter-mypaint-precision'],check=True)
subprocess.run(['python3',str(S/'tools/run-painter-xvfb.py'),'meson','test','-C',str(B),'--no-rebuild','--num-processes','1','--print-errorlogs','--logbase','precision-refusal-restored','--test-args=-p /painter-precision/unsupported-profile-refusal','painter-mypaint-precision'],check=True)
