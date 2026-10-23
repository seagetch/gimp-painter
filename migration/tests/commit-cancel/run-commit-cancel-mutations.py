#!/usr/bin/env python3
"""Under caller shared build lock, mutate actual production sources then restore/rebuild."""
import argparse,json,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument("--source",type=pathlib.Path,required=True);p.add_argument("--build",type=pathlib.Path,required=True);p.add_argument("--output",type=pathlib.Path,required=True);args=p.parse_args()
src=args.source.resolve();build=args.build.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
rows=[]
cases=[('cancel-keeps-pixels','app/paint/painter-mypaint-surface/paint-core.cpp','else gimp_paint_core_cancel(c,&drawables);','else gimp_paint_core_finish(c,&drawables,FALSE);','/commit-cancel/native-pixels-and-history'), ('commit-rolls-back','app/paint/painter-mypaint-surface/paint-core.cpp','if(commit&&gimp_item_is_attached(GIMP_ITEM(target.get())))','if(commit&&g_strcmp0(c->undo_desc,"Commit cancel fixture")!=0&&gimp_item_is_attached(GIMP_ITEM(target.get())))','/commit-cancel/native-pixels-and-history'), ('external-finish-commits-batch','app/paint/painter-mypaint-surface/paint-core.cpp','if(batch){batch_aborted=true;commit=false;}','if(batch){batch_aborted=true;}','/commit-cancel/native-pixels-and-history')]


for name,path,before,after,test in cases:
 p=src/path;original=p.read_bytes();text=original.decode();assert text.count(before)==1
 try:
  p.write_text(text.replace(before,after))
  c=subprocess.run(['meson','compile','-C',str(build),'-j4','gimp-painter-surface'],capture_output=True,text=True,timeout=180)
  assert c.returncode==0,c.stdout+c.stderr
  r=subprocess.run(['python3',str(src/'tools/run-painter-xvfb.py'),'meson','test','-C',str(build),'--no-rebuild','--print-errorlogs','--logbase','commit-cancel-'+name,'--test-args=-p '+test,'gimp-painter-surface'],capture_output=True,text=True,timeout=90)
  raw=[json.loads(x) for x in (build/'meson-logs'/('commit-cancel-'+name+'.json')).read_text().splitlines() if x.strip()];assert r.returncode!=0 and len(raw)==1 and raw[0]['result']=='FAIL',(r.stdout,r.stderr,raw)
  rows.append(dict(mutation=name,path=path,before=before,after=after,selected_test=test,compile_exit_code=c.returncode,test_exit_code=r.returncode,result=raw[0]['result'],stdout=raw[0]['stdout'],stderr=raw[0]['stderr']))
 finally:
  p.write_bytes(original)
  c=subprocess.run(['meson','compile','-C',str(build),'-j4','gimp-painter-surface'],capture_output=True,text=True,timeout=180);assert c.returncode==0,c.stdout+c.stderr
(out/'mutation-results.json').write_text(json.dumps(rows,indent=2)+'\n')
