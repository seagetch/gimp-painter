#!/usr/bin/env python3
"""Under caller shared build lock, mutate actual production sources then restore/rebuild."""
import argparse,json,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument("--source",type=pathlib.Path,required=True);p.add_argument("--build",type=pathlib.Path,required=True);p.add_argument("--output",type=pathlib.Path,required=True);args=p.parse_args()
src=args.source.resolve();build=args.build.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
rows=[]
cases=[
 ('replace-native-union','app/paint/painter-mypaint-surface/paint-core.cpp','else {c->x1=std::min(c->x1,rect.x);c->y1=std::min(c->y1,rect.y);c->x2=std::max(c->x2,rect.x+rect.width);c->y2=std::max(c->y2,rect.y+rect.height);}', 'else {c->x1=rect.x;c->y1=rect.y;c->x2=rect.x+rect.width;c->y2=rect.y+rect.height;}', '/dirty/native-update-undo-extent'),
 ('drop-drawable-update','app/paint/painter-mypaint-surface/paint-core.cpp','gimp_drawable_update(target.get(),rect.x,rect.y,rect.width,rect.height);','/* mutation: omit native drawable damage */','/dirty/native-update-undo-extent')]


for name,path,before,after,test in cases:
 p=src/path;original=p.read_bytes();text=original.decode();assert text.count(before)==1
 try:
  p.write_text(text.replace(before,after))
  c=subprocess.run(['meson','compile','-C',str(build),'-j4','gimp-painter-surface'],capture_output=True,text=True,timeout=180)
  assert c.returncode==0,c.stdout+c.stderr
  r=subprocess.run(['python3',str(src/'tools/run-painter-xvfb.py'),'meson','test','-C',str(build),'--no-rebuild','--print-errorlogs','--logbase','dirty-'+name,'--test-args=-p '+test,'gimp-painter-surface'],capture_output=True,text=True,timeout=90)
  raw=[json.loads(x) for x in (build/'meson-logs'/('dirty-'+name+'.json')).read_text().splitlines() if x.strip()];assert r.returncode!=0 and len(raw)==1 and raw[0]['result']=='FAIL',(r.stdout,r.stderr,raw)
  rows.append(dict(mutation=name,path=path,before=before,after=after,selected_test=test,compile_exit_code=c.returncode,test_exit_code=r.returncode,result=raw[0]['result'],stdout=raw[0]['stdout'],stderr=raw[0]['stderr']))
 finally:
  p.write_bytes(original)
  c=subprocess.run(['meson','compile','-C',str(build),'-j4','gimp-painter-surface'],capture_output=True,text=True,timeout=180);assert c.returncode==0,c.stdout+c.stderr
(out/'mutation-results.json').write_text(json.dumps(rows,indent=2)+'\n')
