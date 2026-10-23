/* Synthetic GDK inputs to unchanged original GUI event handler. */
#include "config.h"
#include <stdio.h>
#include <math.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-tool-events.h"
#include "display/gimpdisplayshell-rotate.h"
#include "display/gimpdisplayshell-transform.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static FILE *out;
static unsigned seq;
static void record(GimpDisplayShell *s,const char *scene,const char *step,int type,unsigned state,double x,double y,unsigned detail,int handled) {
 double dx=s->disp_width/2+40,dy=s->disp_height/2+20,ix=dx,iy=dy,bx,by;
 gimp_display_shell_device_to_image_coords(s,&ix,&iy); bx=ix;by=iy;
 gimp_display_shell_image_to_device_coords(s,&bx,&by);
 fprintf(out,"%u\t%s\t%s\t%d\t%u\t%.17g\t%.17g\t%u\t%d\t%d\t%d\t%d\t%.17g\t%.17g\t%d\t%d\t%d\t%d\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\n",++seq,scene,step,type,state,x,y,detail,handled,s->disp_width,s->disp_height,s->mirrored,s->rotate_angle,s->rotate_start_angle,s->rotating,s->scrolling,s->scaling,s->pointer_grabbed,dx,dy,ix,iy,bx,by); fflush(out);
}
static void event(GimpDisplayShell *s,const char *scene,const char *step,GdkEventType type,unsigned state,double x,double y,unsigned detail) {
 GdkEvent *e=gdk_event_new(type); e->any.window=g_object_ref(gtk_widget_get_window(s->canvas));e->any.send_event=TRUE;
 if(type==GDK_MOTION_NOTIFY){e->motion.time=GDK_CURRENT_TIME;e->motion.x=x;e->motion.y=y;e->motion.state=state;e->motion.device=gdk_display_get_core_pointer(gtk_widget_get_display(s->canvas));}
 else if(type==GDK_BUTTON_PRESS||type==GDK_BUTTON_RELEASE){e->button.time=GDK_CURRENT_TIME;e->button.x=x;e->button.y=y;e->button.state=state;e->button.button=detail;e->button.device=gdk_display_get_core_pointer(gtk_widget_get_display(s->canvas));}
 else {e->key.time=GDK_CURRENT_TIME;e->key.state=state;e->key.keyval=detail;}
 int handled=gimp_display_shell_canvas_tool_events(s->canvas,e,s);gdk_event_free(e);record(s,scene,step,type,state,x,y,detail,handled);
}
static GtkWidget *find_button(GtkWidget *w,const char *stock) {
 if(GTK_IS_BUTTON(w)) {GtkWidget *im=gtk_button_get_image(GTK_BUTTON(w));
  if(im && GTK_IS_IMAGE(im) && gtk_image_get_storage_type(GTK_IMAGE(im))==GTK_IMAGE_STOCK){gchar *id=NULL;GtkIconSize size;gtk_image_get_stock(GTK_IMAGE(im),&id,&size);if(id && !strcmp(id,stock))return w;}}
 if(GTK_IS_CONTAINER(w)){GList *children=gtk_container_get_children(GTK_CONTAINER(w));GtkWidget *found=NULL;for(GList *p=children;p&&!found;p=p->next)found=find_button(p->data,stock);g_list_free(children);return found;}return NULL;
}
int main(int argc,char **argv){
 Gimp *g;GimpDisplayShell *s;char *path;int mirror,i;double initial[]={0,7.499999,7.5,7.500001,-7.499999,-7.5,-7.500001,22.5,-22.5,352.5,359.9};
 gtk_init(&argc,&argv);gimp_test_utils_setup_menus_dir();g=gimp_init_for_gui_testing(TRUE);gimp_test_run_temp_mainloop(500);gimp_test_utils_create_image(g,640,480);gimp_test_run_temp_mainloop(500);
 s=gimp_display_get_shell(GIMP_DISPLAY(gimp_get_display_iter(g)->data));gtk_widget_grab_focus(s->canvas);gimp_context_set_display(gimp_get_user_context(g),s->display);gimp_test_run_temp_mainloop(100);
 path=g_build_filename(g_getenv("CAPTURE_DIR"),"rotation.tsv",NULL);out=fopen(path,"w");g_assert(out);g_free(path);
 fprintf(out,"seq\tscene\tstep\ttype\tstate\tx\ty\tdetail\thandled\twidth\theight\tmirror\tangle\tanchor\trotating\tscrolling\tscaling\tgrabbed\tdevice_x\tdevice_y\tunrotated_x\tunrotated_y\tback_x\tback_y\n");
 for(mirror=0;mirror<2;mirror++)for(i=0;i<11;i++){
 char scene[80];int cx=s->disp_width/2,cy=s->disp_height/2;snprintf(scene,sizeof(scene),"stationary-m%d-i%d",mirror,i);s->mirrored=mirror;s->rotate_angle=initial[i];record(s,scene,"setup",-1,0,0,0,0,0);
 event(s,scene,"shift-middle",GDK_BUTTON_PRESS,GDK_SHIFT_MASK,cx,cy+100,2);
 event(s,scene,"free",GDK_MOTION_NOTIFY,GDK_SHIFT_MASK|GDK_BUTTON2_MASK,cx,cy+100,0);
 event(s,scene,"ctrl-press",GDK_KEY_PRESS,GDK_SHIFT_MASK|GDK_BUTTON2_MASK,cx,cy+100,GDK_Control_L);
 event(s,scene,"snap",GDK_MOTION_NOTIFY,GDK_SHIFT_MASK|GDK_CONTROL_MASK|GDK_BUTTON2_MASK,cx,cy+100,0);
 event(s,scene,"shift-release",GDK_KEY_RELEASE,GDK_SHIFT_MASK|GDK_CONTROL_MASK|GDK_BUTTON2_MASK,cx,cy+100,GDK_Shift_L);
 event(s,scene,"snap-without-shift",GDK_MOTION_NOTIFY,GDK_CONTROL_MASK|GDK_BUTTON2_MASK,cx,cy+100,0);
 event(s,scene,"ctrl-release",GDK_KEY_RELEASE,GDK_CONTROL_MASK|GDK_BUTTON2_MASK,cx,cy+100,GDK_Control_L);
 event(s,scene,"restored",GDK_MOTION_NOTIFY,GDK_BUTTON2_MASK,cx,cy+100,0);
 event(s,scene,"middle-release",GDK_BUTTON_RELEASE,GDK_BUTTON2_MASK,cx,cy+100,2);
 }
 for(mirror=0;mirror<2;mirror++){
 char scene[80];int cx=s->disp_width/2,cy=s->disp_height/2;snprintf(scene,sizeof(scene),"quadrants-m%d",mirror);s->mirrored=mirror;s->rotate_angle=0;record(s,scene,"setup",-1,0,0,0,0,0);
 event(s,scene,"shift-ctrl-alt-middle",GDK_BUTTON_PRESS,GDK_SHIFT_MASK|GDK_CONTROL_MASK|GDK_MOD1_MASK,cx,cy+100,2);
 event(s,scene,"right",GDK_MOTION_NOTIFY,GDK_BUTTON2_MASK,cx+100,cy,0);
 event(s,scene,"top",GDK_MOTION_NOTIFY,GDK_BUTTON2_MASK,cx,cy-100,0);
 event(s,scene,"left",GDK_MOTION_NOTIFY,GDK_BUTTON2_MASK,cx-100,cy,0);
 event(s,scene,"fractional",GDK_MOTION_NOTIFY,GDK_BUTTON2_MASK,cx+26.9,cy+100.9,0);
 event(s,scene,"fractional-snap",GDK_MOTION_NOTIFY,GDK_BUTTON2_MASK|GDK_CONTROL_MASK,cx+26.9,cy+100.9,0);
 event(s,scene,"center",GDK_MOTION_NOTIFY,GDK_BUTTON2_MASK,cx,cy,0);
 event(s,scene,"middle-release",GDK_BUTTON_RELEASE,GDK_BUTTON2_MASK,cx,cy,2);
 }
 {
 GtkWidget *left=find_button(s->statusbar,GIMP_STOCK_ROTATE_270),*right=find_button(s->statusbar,GIMP_STOCK_ROTATE_90),*flip=find_button(s->statusbar,GIMP_STOCK_FLIP_HORIZONTAL),*reset=find_button(s->statusbar,GTK_STOCK_ZOOM_100);
 double values[]={0,7.5,-7.5,15,-15,14.999999,15.000001,-14.999999,-15.000001,345,359.9};
 g_assert(left && right && flip && reset);
 for(mirror=0;mirror<2;mirror++)for(i=0;i<11;i++) {
  char scene[80];snprintf(scene,sizeof(scene),"buttons-m%d-i%d",mirror,i);gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(flip),mirror);s->mirrored=mirror;s->rotate_angle=values[i];record(s,scene,"setup",-1,0,0,0,0,0);
  gtk_button_clicked(GTK_BUTTON(left));record(s,scene,"left",-2,0,0,0,0,1);
  s->rotate_angle=values[i];gtk_button_clicked(GTK_BUTTON(right));record(s,scene,"right",-2,0,0,0,0,1);
  s->rotate_angle=values[i];gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(flip),!mirror);record(s,scene,"flip",-2,0,0,0,0,1);
  gtk_button_clicked(GTK_BUTTON(right));record(s,scene,"right-after-flip",-2,0,0,0,0,1);
  gtk_button_clicked(GTK_BUTTON(reset));record(s,scene,"reset",-2,0,0,0,0,1);
 }
}
{
 GdkWindow *window=gtk_widget_get_window(s->canvas);gint w=gdk_window_get_width(window),h=gdk_window_get_height(window);GdkPixbuf *pix=gdk_pixbuf_get_from_drawable(NULL,window,NULL,0,0,0,0,w,h);
 char *filename=g_build_filename(g_getenv("CAPTURE_DIR"),"canvas.png",NULL);if(pix){gdk_pixbuf_save(pix,filename,"png",NULL,NULL);g_object_unref(pix);}g_free(filename);
}
fclose(out);g_print("ROTATION_CAPTURE_COMPLETE rows=%u\n",seq);gimp_exit(g,TRUE);return 0;
}
