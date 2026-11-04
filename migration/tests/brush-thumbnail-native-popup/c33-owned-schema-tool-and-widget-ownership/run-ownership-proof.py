#!/usr/bin/env python3
import pathlib, resource, subprocess, sys, tempfile
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
root=pathlib.Path(sys.argv[1]).resolve()
template_dir=pathlib.Path(__file__).resolve().parent
workspace=tempfile.TemporaryDirectory(prefix='gimp-widget-ownership-')
out=pathlib.Path(workspace.name)

def extract(rel,name):
 s=(root/rel).read_text();start=s.index('\n'+name+' (')+1
 # Include function return type (static gint is on preceding line).
 start=s.rfind('\n',0,start-1)+1
 opening=s.index('{',start);depth=1;i=opening+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
tree=extract('app/widgets/gimpcontainertreeview.c','gimp_container_tree_view_get_selected')
scale=extract('app/display/gimpscalecombobox.c','gimp_scale_combo_box_set_scale')
assert tree.startswith('static gint') and scale.startswith('void')
template=(template_dir/'ownership-harness.c.in').read_text()
for variant in ['fixed','missing-path-free','missing-label-free']:
 t=tree;s=scale
 if variant=='missing-path-free':t=t.replace('g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);','')
 if variant=='missing-label-free':s=s.replace('g_free (label);','')
 c=out/(variant+'.c');c.write_text(template.replace('@TREE@',t).replace('@SCALE@',s))
 exe=out/variant
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(c),'-o',str(exe),'-l:libgtk-3.so.0','-l:libgobject-2.0.so.0','-l:libglib-2.0.so.0','-lm'],check=True)
 r=subprocess.run([str(exe)],capture_output=True,text=True)
 print(variant, 'exit='+str(r.returncode),r.stdout,r.stderr,flush=True)
 assert (r.returncode==0)==(variant=='fixed')
 if variant=='missing-path-free': assert 'path_allocs==path_frees' in r.stderr
 if variant=='missing-label-free': assert 'label_allocs==label_frees' in r.stderr

workspace.cleanup()
