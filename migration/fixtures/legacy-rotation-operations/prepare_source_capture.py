#!/usr/bin/env python3
"""Extract unchanged original functions, verify full source against the pin."""
from pathlib import Path
import hashlib,json,subprocess
base=Path(__file__).resolve().parent
pin='afa43fae3e920210146abed514f136fd49f671b5'
sources={};bodies=[];functions={}
def extract(path,names):
 p=base/'gimp'/path; blob=subprocess.check_output(['git','-C',str(base/'gimp'),'show',f'{pin}:{path}']);assert p.read_bytes()==blob,path
 sources[path]=hashlib.sha256(blob).hexdigest();text=blob.decode()
 for name in names:
  start=text.index('\n'+name+' (')+1 if '\n'+name+' (' in text else text.index('\n'+name+' ')+1
  start=text.rfind('\n',0,start-1)+1
  b=text.index('{',start);e=b+1;depth=1
  while depth:
   depth+=(text[e]=='{')-(text[e]=='}');e+=1
  body=text[start:e];bodies.append(body);functions[name]={'path':path,'start_line':text[:start].count('\n')+1,'end_line':text[:e].count('\n')+1,'sha256':hashlib.sha256(body.encode()).hexdigest()}
extract('app/display/gimpdisplayshell-tool-events.c',['gimp_display_shell_start_rotating','gimp_display_shell_rotate','gimp_display_shell_stop_rotating'])
extract('app/display/gimpstatusbar.c',['gimp_statusbar_rotate_left_clicked','gimp_statusbar_rotate_right_clicked','gimp_statusbar_flip_side_clicked','gimp_statusbar_reset_view_clicked'])
extract('app/display/gimpdisplayshell-rotate.c',['gimp_display_shell_set_cairo_rotate'])
header='''/* Original function bodies below unchanged. Synthetic structs and side-effect
 * counters are adapters, not a claim of GUI or device input. Cairo is real. */
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <cairo.h>
typedef double gdouble; typedef int gint;
typedef struct { unsigned state; } GdkEvent, GdkEventMotion;
typedef struct { int rotating, disp_width, disp_height, mirrored; double rotate_angle, rotate_start_angle; int grab, cursor, expose, draw, scale_requests; double scale; } GimpDisplayShell;
typedef struct { GimpDisplayShell *shell; } GimpStatusbar;
typedef struct { int active; } GtkWidget;
#define TRUE 1
#define FALSE 0
#define GDK_CONTROL_MASK 4
#define GDK_POINTER_MOTION_MASK 0
#define GDK_EXCHANGE 1
#define ROTATE_SNAP_UNIT 15.0
#define ROTATE_UNIT_ANGLE 30
#define GIMP_ZOOM_TO 0
#define GIMP_ZOOM_FOCUS_BEST_GUESS 0
#define g_return_if_fail(x) assert(x)
#define gimp_display_shell_pointer_grab(s,...) ((s)->grab=1)
#define gimp_display_shell_pointer_ungrab(s,...) ((s)->grab=0)
#define gimp_display_shell_set_override_cursor(s,c) ((s)->cursor=c)
#define gimp_display_shell_unset_override_cursor(s) ((s)->cursor=0)
#define gimp_display_shell_expose_full(s) ((s)->expose++)
#define GTK_WIDGET(s) (s)
#define GTK_TOGGLE_BUTTON(s) (s)
#define gtk_widget_queue_draw(s) ((s)->draw++)
#define gtk_toggle_button_get_active(w) ((w)->active)
#define gimp_display_shell_scale(s,t,v,f) ((s)->scale_requests++,(s)->scale=(v))
'''
(base/'source-capture.c').write_text(header+'\n\n'.join(bodies)+'\n'+(base/'source-driver.c').read_text())
(base/'source-provenance.json').write_text(json.dumps({'pin':pin,'sources':sources,'functions':functions,'scope':'Original function bodies, synthetic structs and GUI side-effect adapters; real Cairo transforms. Not a GUI/event/device capture.'},indent=2)+'\n')
