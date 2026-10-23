import json,pathlib,subprocess,tempfile,shutil,os,shlex
root=pathlib.Path('<checkout>');out=pathlib.Path('<evidence>');sub=pathlib.Path('app/paint/painter-mypaint-surface');rows=[]
with tempfile.TemporaryDirectory(prefix='dirty-union-mutation-') as d:
 p=pathlib.Path(d);shutil.copytree(root/sub,p/sub);(p/'app/painter').symlink_to(root/'app/painter',target_is_directory=True);(p/'app/paint/painter-mypaint').symlink_to(root/'app/paint/painter-mypaint',target_is_directory=True)
 file=p/sub/'gegl-surface.cpp';s=file.read_text();old='gegl_rectangle_bounding_box(&impl_->dirty,&impl_->dirty,&rect)';new='impl_->dirty=rect';assert s.count(old)==2;s=s.replace(old,new);file.write_text(s)
 flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gegl-0.4'],text=True));exe=p/'probe';cmd=['c++','-std=c++14','-O1','-g','-I'+str(p/'app'),str(file),str(root/sub/'tests/coordinate-boundaries.cpp'),*flags,'-o',str(exe)];subprocess.run(cmd,check=True,timeout=120)
 r=subprocess.run([str(exe),'-p','/coordinates/overlapping-adjacent-dirty-union'],capture_output=True,text=True,timeout=60,env=dict(os.environ,GEGL_SWAP='RAM',GEGL_USE_OPENCL='no'));assert r.returncode!=0,(r.stdout,r.stderr)
 rows.append(dict(mutation='replace-surface-union',path=str(sub/'gegl-surface.cpp'),before=old,after=new,selected_test='/coordinates/overlapping-adjacent-dirty-union',test_exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr.replace(d,'<temporary>'),scope='Private copy of actual production Surface; real GEGL; unchanged existing coordinate fixture'))
(out/'surface-mutation.json').write_text(json.dumps(rows,indent=2)+'\n');print('Actual production Surface union mutation rejected')
