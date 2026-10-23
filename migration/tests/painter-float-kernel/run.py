#!/usr/bin/env python3
"""Independent premultiplied double reference for the opt-in float kernel."""
import argparse, ctypes, hashlib, itertools, json, math, pathlib, random, struct, subprocess, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[3]
SRC=ROOT/'app/operations/layer-modes-legacy/gimppainterfloatkernel.c'
f32=lambda x:struct.unpack('f',struct.pack('f',x))[0]
LIMIT=f32(3.4028234663852886e38)
def unit(v):return max(0,min(1,v)) if math.isfinite(v) else 0.
def rgb(v):return v if math.isfinite(v) else 0.
def reference(a,b,o,m,mode):
 a=list(map(rgb,a[:3]))+[unit(a[3])];b=list(map(rgb,b[:3]))+[unit(b[3])];w=unit(o)*unit(m)
 if mode in (4,6):a,b=b,a
 if mode in (0,2):
  alpha=a[3]*(1-b[3]*w) if mode==0 else 1-(1-a[3])*(1-b[3]*w)
  colors=a[:3]
 else:
  if mode==8:b=[a[i]*b[i] for i in range(3)]+[min(a[3],b[3])]
  if mode in (3,4):b[3]*=a[3]
  if mode in (5,6):b[3]*=1-a[3]
  source_weight=w*b[3]
  destination_weight=a[3]*(1-source_weight) if mode in (7,8) else a[3]*(1-w)
  alpha=destination_weight+source_weight
  colors=[(a[i]*destination_weight+b[i]*source_weight)/alpha if alpha else a[i] for i in range(3)]
  if mode==8 and a[3]>0:alpha=a[3]
 return [f32(max(-LIMIT,min(LIMIT,c))) for c in colors]+[f32(unit(alpha))]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--report',type=pathlib.Path,required=True);ap.add_argument('--sanitize',action='store_true');args=ap.parse_args()
 random.seed(13012);cases=[]
 colors=[[-2,.1234567,4],[.00001,.50001,.99999],[LIMIT,-LIMIT,0],[float('nan'),float('inf'),-float('inf')]]
 for mode in range(9):
  for a,b,o,m in itertools.product([-1.,0.,.001,.37,1.,2.,float('nan'),float('inf'),-float('inf')],repeat=4):
   for c in range(4):cases.append((mode,[*colors[c],a],[*colors[(c+1)%4],b],o,m))
  for n in range(10000):cases.append((mode,[*[random.uniform(-5,5) for _ in range(3)],random.random()],[*[random.uniform(-5,5) for _ in range(3)],random.random()],random.random(),random.random()))
 with tempfile.TemporaryDirectory(prefix='painter-float-') as tmp:
  tmp=pathlib.Path(tmp);data=tmp/'vectors.bin';count=0
  with data.open('wb') as f:
   for mode,a,b,o,m in cases:
    a=list(map(f32,a));b=list(map(f32,b));o=f32(o);m=f32(m)
    f.write(struct.pack('=i14f',mode,*a,*b,o,m,*reference(a,b,o,m,mode)));count+=1
  harness=tmp/'check.c';harness.write_text(r'''
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "gimppainterfloatkernel.h"
int main(int argc,char **argv){FILE*f=fopen(argv[1],"rb");int mode;float v[14],out[4],alias[4];unsigned n=0;(void)argc;
while(fread(&mode,4,1,f)==1){if(fread(v,sizeof v,1,f)!=1)return 2;
if(!gimp_painter_float_composite(v,v+4,out,v[8],v[9],mode))return 3;
for(int k=0;k<4;k++)if(!isfinite(out[k]) || fabs((double)out[k]-v[10+k])>2e-6*fmax(1,fabs(v[10+k]))){fprintf(stderr,"case %u mode %d channel %d got %.9g expected %.9g\n",n,mode,k,out[k],v[10+k]);return 4;}
memcpy(alias,v,sizeof alias);gimp_painter_float_composite(alias,v+4,alias,v[8],v[9],mode);if(memcmp(alias,out,sizeof out))return 5;
memcpy(alias,v+4,sizeof alias);gimp_painter_float_composite(v,alias,alias,v[8],v[9],mode);if(memcmp(alias,out,sizeof out))return 6;n++;}
fclose(f);
{float a[4]={0,0,0,0},b[4]={.1234567f,-2,4,1},z[4]={9,8,7,6};
if(gimp_painter_float_composite(a,b,z,1,1,99)||z[0]!=9||z[3]!=6)return 7;
gimp_painter_float_composite(a,b,z,1,1,GIMP_PAINTER_FLOAT_NORMAL);
if(z[0]!=b[0]||z[1]!=-2||z[2]!=4||z[3]!=1)return 8;
if(fabs(z[0]*255-round(z[0]*255))<.001)return 9;}
printf("%u reference vectors and %u alias checks passed\n",n,2*n);return 0;}
''')
  cmd=['cc','-std=c11','-O2','-Wall','-Wextra','-I'+str(SRC.parent),str(SRC),str(harness),'-lm','-o',str(tmp/'check')]
  if args.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  subprocess.run(cmd,check=True)
  import os
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
  run=subprocess.run([str(tmp/'check'),str(data)],env=env,text=True,capture_output=True)
  report={'scope':'isolated continuous kernel, no native registration/UI/XCF claim','vectors':count,'alias_checks':2*count,'seed':13012,'sanitizers':['address','undefined'] if args.sanitize else [],'source_sha256':hashlib.sha256(SRC.read_bytes()).hexdigest(),'runner_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'exit':run.returncode,'stdout':run.stdout,'stderr':run.stderr}
  args.report.write_text(json.dumps(report,indent=2)+'\n');print(run.stdout,run.stderr);raise SystemExit(run.returncode)
if __name__=='__main__':main()
