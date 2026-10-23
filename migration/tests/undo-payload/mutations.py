import pathlib,subprocess,json
S=pathlib.Path('<native>/source');B=S.parent/'build-port';E=pathlib.Path(__file__).parent
p=S/'app/paint/gimppaintcore.c';original=p.read_text();old='buffer = gegl_buffer_new (GEGL_RECTANGLE (0, 0, rect.width, rect.height),\n                                        gimp_drawable_get_format (iter->data));';new='buffer = gegl_buffer_new (GEGL_RECTANGLE (0, 0, rect.width, rect.height),\n                                        babl_format ("RGB float"));';assert original.count(old)==1
copyold='gimp_gegl_buffer_copy (undo_buffer,\n                                     &rect,\n                                     GEGL_ABYSS_NONE,\n                                     buffer,\n                                     GEGL_RECTANGLE (0, 0, 0, 0));';copynew=copyold.replace('gimp_gegl_buffer_copy (undo_buffer,','gimp_gegl_buffer_copy (gimp_drawable_get_buffer (iter->data),');assert original.count(copyold)==1
rows=[]
try:
 for name,a,b in [('narrow-native-undo-payload',old,new),('save-painted-instead-of-initial',copyold,copynew)]:
  p.write_text(original.replace(a,b));subprocess.run(['meson','compile','-C',str(B),'-j4','gimp-painter-surface'],check=True)
  r=subprocess.run(['python3',str(S/'tools/run-painter-xvfb.py'),'meson','test','-C',str(B),'--no-rebuild','--num-processes','1','--print-errorlogs','--logbase','undo-payload-'+name,'--test-args=-p /undo-payload/native-snapshot-bytes','gimp-painter-surface'],capture_output=True,text=True);(E/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode!=0 and 'not ok'in r.stdout+r.stderr;rows.append(dict(name=name,exit_code=r.returncode,rejected=True,old=a,new=b))
finally:
 p.write_text(original);subprocess.run(['meson','compile','-C',str(B),'-j4','gimp-painter-surface'],check=True)
(E/'mutations.json').write_text(json.dumps(rows,indent=2)+'\n');subprocess.run(['python3',str(S/'tools/run-painter-xvfb.py'),'meson','test','-C',str(B),'--no-rebuild','--num-processes','1','--print-errorlogs','--logbase','undo-payload-restored','--test-args=-p /undo-payload/native-snapshot-bytes','gimp-painter-surface'],check=True)
