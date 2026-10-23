import pathlib,subprocess,json,os,time
S=pathlib.Path('<native>/source');B=S.parent/'build-port';E=pathlib.Path(__file__).parent;p=S/'app/paint/painter-mypaint-surface/buffer-read.hpp';original=p.read_text();old='if (right <= x || bottom <= y)';new='if (right <= x + 1 || bottom <= y)';assert original.count(old)==1

try:
 p.write_text(original.replace(old,new));subprocess.run(['meson','compile','-C',str(B),'-j4','gimp-painter-surface'],check=True)
 r=subprocess.run(['python3',str(S/'tools/run-painter-xvfb.py'),'meson','test','-C',str(B),'--no-rebuild','--num-processes','1','--print-errorlogs','--logbase','small-boundaries-mutant','--test-args=-p /small-boundaries/native-tiny-edges','gimp-painter-surface'],capture_output=True,text=True);(E/'native-mutant.log').write_text(r.stdout+r.stderr);assert r.returncode!=0 and 'not ok' in r.stdout+r.stderr
 (E/'native-mutant.json').write_text(json.dumps(dict(name='off-by-one-one-pixel-extent',exit_code=r.returncode,rejected=True,old=old,new=new),indent=2)+'\n')
finally:
 p.write_text(original);subprocess.run(['meson','compile','-C',str(B),'-j4','gimp-painter-surface'],check=True)
subprocess.run(['python3',str(S/'tools/run-painter-xvfb.py'),'meson','test','-C',str(B),'--no-rebuild','--num-processes','1','--print-errorlogs','--logbase','small-boundaries-restored','--test-args=-p /small-boundaries/native-tiny-edges','gimp-painter-surface'],check=True)
