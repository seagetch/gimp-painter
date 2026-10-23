#!/usr/bin/env python3
"""Extract unchanged pinned applied-zoom functions; no old application execution."""
from pathlib import Path
import hashlib,json,re,subprocess
ROOT=Path(__file__).resolve().parents[3];OUT=Path(__file__).resolve().parent;REV='afa43fae3e920210146abed514f136fd49f671b5'
GROUPS={
'app/display/gimpdisplayshell-scale.c':['gimp_display_shell_scale','gimp_display_shell_scale_to','gimp_display_shell_scale_get_zoom_focus','gimp_display_shell_scale_get_image_center_viewport','gimp_display_shell_scale_image_is_within_viewport','gimp_display_shell_scale_image_starts_to_fit','gimp_display_shell_scale_image_stops_to_fit','gimp_display_shell_scale_viewport_coord_almost_centered','gimp_display_shell_calculate_scale_x_and_y','gimp_display_shell_scale_by_values','gimp_display_shell_scale_resize','gimp_display_shell_get_screen_resolution'],
'app/display/gimpdisplayshell-scroll.c':['gimp_display_shell_scroll_center_image','gimp_display_shell_scroll_set_offset','gimp_display_shell_scroll_clamp_offsets','gimp_display_shell_scroll_clamp_and_update'],
'app/display/gimpdisplayshell-draw.c':['gimp_display_shell_draw_get_scaled_image_size','gimp_display_shell_draw_get_scaled_image_size_for_scale'],
'app/display/gimpdisplayshell-transform.c':['gimp_display_shell_untransform_xy_f'],
'app/display/gimpdisplayshell-rotate.c':['gimp_display_shell_set_cairo_rotate'],
'app/display/gimpdisplayshell.c':['gimp_display_shell_scale_changed'],
'libgimpwidgets/gimpzoommodel.c':['gimp_zoom_model_zoom','gimp_zoom_model_get_factor','gimp_zoom_model_zoom_step','gimp_zoom_model_set_property']}
rows=[];texts=[];prototypes=[]
for path,names in GROUPS.items():
 raw=subprocess.check_output(['git','show',REV+':'+path],cwd=ROOT);text=raw.decode()
 for name in names:
  m=re.search(r'^(?:static )?(?:void|gboolean|gdouble|gint)\n'+name+r'\s*\(',text,re.M);assert m,name
  brace=text.index('{',m.start());i=brace+1;depth=1
  while depth:
   if text[i]=='{':depth+=1
   elif text[i]=='}':depth-=1
   i+=1
  function=text[m.start():i];file='zoom-'+name+'.txt';(OUT/file).write_text(function+'\n');texts.append(function);prototypes.append(function[:function.index('{')].rstrip()+';')
  rows.append(dict(name=name,source_path=path,source_blob=subprocess.check_output(['git','rev-parse',REV+':'+path],cwd=ROOT,text=True).strip(),source_sha256=hashlib.sha256(raw).hexdigest(),first_line=text[:m.start()].count('\n')+1,last_line=text[:i].count('\n')+1,file=file,sha256=hashlib.sha256((function+'\n').encode()).hexdigest()))
(OUT/'zoom-functions.inc').write_text('/* Unchanged pinned function bodies; aliases/stubs live outside this file. */\n'+'\n\n'.join(texts)+'\n')
(OUT/'zoom-prototypes.inc').write_text('\n'.join(prototypes)+'\n')
(OUT/'zoom-source-manifest.json').write_text(json.dumps(dict(revision=REV,scope='Source-only unchanged extracted applied-zoom logic; no old runtime execution',functions=rows),indent=2)+'\n')
print('Extracted',len(rows),'unchanged pinned functions')
