#!/usr/bin/env python3
import importlib.util,pathlib,random,struct,subprocess,tempfile,shlex,json,hashlib,os,argparse
HERE=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('oracle',HERE/'run.py');oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
ap=argparse.ArgumentParser();ap.add_argument('--report',type=pathlib.Path,required=True);ap.add_argument('--sanitize',action='store_true');args=ap.parse_args()
random.seed(1301201)
with tempfile.TemporaryDirectory(prefix='painter-float-gegl-') as t:
 t=pathlib.Path(t);data=t/'vectors.bin'
 with data.open('wb') as f:
  for mode in range(9):
   for opacity in [0,.0001,.37,.5,1]:
    opacity=oracle.f32(opacity);a=[];b=[];m=[];expected=[]
    for n in range(64):
     aa=list(map(oracle.f32,[*[random.uniform(-4,4) for _ in range(3)],random.choice([0.,1.,random.random()])]))
     bb=list(map(oracle.f32,[*[random.uniform(-4,4) for _ in range(3)],random.choice([0.,1.,random.random()])]))
     mm=oracle.f32(random.choice([0.,1.,random.random()]));a+=aa;b+=bb;m.append(mm);expected+=oracle.reference(aa,bb,opacity,mm,mode)
    f.write(struct.pack('=if',mode,opacity));f.write(struct.pack('=832f',*a,*b,*m,*expected))
 flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gegl-0.4'],text=True))
 cmd=['cc','-O2','-I'+str(oracle.SRC.parent),str(oracle.SRC),str(HERE/'gegl-check.c'),*flags,'-lm','-o',str(t/'check')]
 if args.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 subprocess.run(cmd,check=True)
 run=subprocess.run([str(t/'check'),str(data)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1',GEGL_USE_OPENCL='no'),capture_output=True,text=True)
 args.report.write_text(json.dumps({'scope':'test-only GEGL point-composer adapter plus new isolated kernel; NOT production app layer-mode registration','graphs':45,'pixels_per_graph':64,'sanitizers':['address','undefined'] if args.sanitize else [],'source_sha256':{str(p.relative_to(oracle.ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [oracle.SRC,HERE/'gegl-check.c',HERE/'run.py',pathlib.Path(__file__)]},'exit':run.returncode,'stdout':run.stdout,'stderr':run.stderr},indent=2)+'\n');print(run.stdout,run.stderr);raise SystemExit(run.returncode)
