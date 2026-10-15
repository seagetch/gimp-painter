/* Original function bodies below unchanged. Synthetic structs and side-effect
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
static void
gimp_display_shell_start_rotating (GimpDisplayShell *shell,
                                    const GdkEvent   *event,
                                    gint              x,
                                    gint              y)
{
  gdouble cx, cy;
  gdouble rx, ry;
  g_return_if_fail (! shell->rotating);

  gimp_display_shell_pointer_grab (shell, event, GDK_POINTER_MOTION_MASK);

  shell->rotating      = TRUE;

  cx = shell->disp_width  / 2;
  cy = shell->disp_height / 2;
  rx = x - cx;
  ry = y - cy;

  if (shell->mirrored)
    rx = -rx;

  shell->rotate_start_angle = shell->rotate_angle + 
    fmod(atan2(rx, ry) / M_PI * 180.0 + 360.0, 360.0); 

  gimp_display_shell_set_override_cursor (shell, GDK_EXCHANGE);
}

static void
gimp_display_shell_rotate        (GimpDisplayShell *shell,
                                  const GdkEvent*         event,
                                  gint              x,
                                  gint              y)
{
  gdouble cx, cy;
  gdouble rx, ry;
  gdouble angle;
  GdkEventMotion* mevent = (GdkEventMotion*)event;
  g_return_if_fail (shell->rotating);

  cx = shell->disp_width  / 2;
  cy = shell->disp_height / 2;
  rx = x - cx;
  ry = y - cy;

  if (shell->mirrored)
    rx = -rx;

  angle               = shell->rotate_start_angle - 
                        fmod(atan2(rx, ry) / M_PI * 180.0 + 360.0, 360.0);
  angle = fmod(angle + 360.0, 360.0);
  if (mevent->state & GDK_CONTROL_MASK) {
    angle = (gint)((angle + ROTATE_SNAP_UNIT / 2) / ROTATE_SNAP_UNIT) * ROTATE_SNAP_UNIT;
    angle = fmod(angle + 360.0, 360.0);
  }
  shell->rotate_angle = angle;
  gimp_display_shell_expose_full(shell);
}

static void
gimp_display_shell_stop_rotating (GimpDisplayShell *shell,
                                   const GdkEvent   *event)
{
  g_return_if_fail (shell->rotating);

  gimp_display_shell_unset_override_cursor (shell);

  shell->rotating           = FALSE;
  shell->rotate_start_angle = 0;

  gimp_display_shell_pointer_ungrab (shell, event);
}

static void
gimp_statusbar_rotate_left_clicked (GtkWidget     *widget,
                                    GimpStatusbar *statusbar)
{
  GimpDisplayShell *shell  = statusbar->shell;
  gdouble direction = shell->mirrored ? -1: 1;
  g_return_if_fail (shell != NULL);

  shell->rotate_angle = fmod(round((shell->rotate_angle- direction * ROTATE_UNIT_ANGLE) /
                                   ROTATE_UNIT_ANGLE) * ROTATE_UNIT_ANGLE, 360);
  gtk_widget_queue_draw(GTK_WIDGET(shell));
}

static void
gimp_statusbar_rotate_right_clicked (GtkWidget     *widget,
                                    GimpStatusbar *statusbar)
{
  GimpDisplayShell *shell  = statusbar->shell;
  gdouble direction = shell->mirrored ? -1: 1;
  g_return_if_fail (shell != NULL);

  shell->rotate_angle = fmod(round((shell->rotate_angle + direction * ROTATE_UNIT_ANGLE) /
                                   ROTATE_UNIT_ANGLE) * ROTATE_UNIT_ANGLE, 360);
  gtk_widget_queue_draw(GTK_WIDGET(shell));
}

static void
gimp_statusbar_flip_side_clicked (GtkWidget     *widget,
                                  GimpStatusbar *statusbar)
{
  GimpDisplayShell *shell  = statusbar->shell;
  g_return_if_fail (shell != NULL);

  shell->mirrored = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  gtk_widget_queue_draw(GTK_WIDGET(shell));
}

static void
gimp_statusbar_reset_view_clicked (GtkWidget     *widget,
                                   GimpStatusbar *statusbar)
{
  GimpDisplayShell *shell  = statusbar->shell;
  g_return_if_fail (shell != NULL);

  shell->mirrored = FALSE;
  shell->rotate_angle = 0;
  gimp_display_shell_scale(shell, GIMP_ZOOM_TO, 1.0, GIMP_ZOOM_FOCUS_BEST_GUESS);
  gtk_widget_queue_draw(GTK_WIDGET(shell));
}

void
gimp_display_shell_set_cairo_rotate (const GimpDisplayShell *shell,
                                     cairo_t          *cr)
{
  gdouble cx, cy;

  g_return_if_fail (shell != NULL);
  g_return_if_fail (cr != NULL);
  
  cx = shell->disp_width / 2;
  cy = shell->disp_height / 2;

  cairo_translate (cr, cx, cy);
  if (shell->mirrored)
    cairo_scale(cr, -1, 1);
  cairo_rotate (cr, shell->rotate_angle / 180.0 * M_PI);
  cairo_translate (cr, -cx, -cy);
}
static int seq;
static void emit(GimpDisplayShell *s,const char *scene,const char *step,unsigned state,double x,double y) {
 cairo_surface_t *surf=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);cairo_t *cr=cairo_create(surf);cairo_matrix_t m;
 double px=s->disp_width/2+40,py=s->disp_height/2+20,bx,by;
 gimp_display_shell_set_cairo_rotate(s,cr);cairo_get_matrix(cr,&m);cairo_user_to_device(cr,&px,&py);bx=px;by=py;cairo_device_to_user(cr,&bx,&by);
 printf("%d\t%s\t%s\t%u\t%.17g\t%.17g\t%d\t%d\t%d\t%.17g\t%.17g\t%d\t%d\t%d\t%d\t%d\t%d\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\n",++seq,scene,step,state,x,y,s->disp_width,s->disp_height,s->mirrored,s->rotate_angle,s->rotate_start_angle,s->rotating,s->grab,s->cursor,s->expose,s->draw,s->scale_requests,s->scale,m.xx,m.yx,m.xy,m.yy,m.x0,m.y0,px,py,bx,by);
 cairo_destroy(cr);cairo_surface_destroy(surf);
}
int main(void){
 const double initial[]={0,7.499999,7.5,7.500001,-7.499999,-7.5,-7.500001,15,22.5,-22.5,352.5,359.999,15-0.000001,15.000001,-15,-14.999999,-15.000001,345};
 puts("seq\tscene\tstep\tstate\tx\ty\twidth\theight\tmirror\tangle\tanchor\trotating\tgrab\tcursor\texpose\tdraw\tscale_requests\tscale\txx\tyx\txy\tyy\tx0\ty0\tpoint_x\tpoint_y\tback_x\tback_y");
 for(int w=100;w<=101;w++)for(int mirror=0;mirror<2;mirror++)for(int a=0;a<18;a++){
 char scene[80];GimpDisplayShell s={0,w,w+12,mirror,initial[a],0};GimpStatusbar bar={&s};GtkWidget toggle={1-mirror};GdkEvent e={0};int x=w/2,y=(w+12)/2+40;
 snprintf(scene,sizeof(scene),"stationary-w%d-m%d-a%d",w,mirror,a);emit(&s,scene,"setup",0,x,y);
 gimp_display_shell_start_rotating(&s,NULL,x,y);emit(&s,scene,"start",1,x,y);
 unsigned states[]={1,5,4,0,4,12,8,0};const char *names[]={"free","ctrl-add","shift-remove","ctrl-remove","ctrl-readd","alt-add","ctrl-remove-alt","alt-remove"};
 for(int i=0;i<8;i++){e.state=states[i];gimp_display_shell_rotate(&s,&e,x,y);emit(&s,scene,names[i],e.state,x,y);}
 gimp_display_shell_stop_rotating(&s,NULL);emit(&s,scene,"stop",0,x,y);
 s.rotate_angle=initial[a];gimp_statusbar_rotate_left_clicked(NULL,&bar);emit(&s,scene,"left",0,0,0);
 s.rotate_angle=initial[a];gimp_statusbar_rotate_right_clicked(NULL,&bar);emit(&s,scene,"right",0,0,0);
 s.rotate_angle=initial[a];gimp_statusbar_flip_side_clicked(&toggle,&bar);emit(&s,scene,"flip",0,0,0);
 gimp_statusbar_rotate_right_clicked(NULL,&bar);emit(&s,scene,"right-after-flip",0,0,0);
 gimp_statusbar_reset_view_clicked(NULL,&bar);emit(&s,scene,"reset",0,0,0);
 }
 const double points[][2]={{40,0},{0,-40},{-40,0},{26.9,100.9},{-26.9,100.9},{0,0},{40,40},{-40,-40},{0,40}};
 for(int w=100;w<=101;w++)for(int mirror=0;mirror<2;mirror++){
 char scene[80];GimpDisplayShell s={0,w,w+12,mirror,0,0};GdkEvent e={0};int cx=w/2,cy=(w+12)/2;
 snprintf(scene,sizeof(scene),"motion-w%d-m%d",w,mirror);gimp_display_shell_start_rotating(&s,NULL,cx,cy+40);emit(&s,scene,"start",13,cx,cy+40);
 for(int p=0;p<9;p++)for(int ctrl=0;ctrl<2;ctrl++){char step[50];double x=cx+points[p][0],y=cy+points[p][1];e.state=ctrl?4:0;snprintf(step,sizeof(step),"p%d-%s",p,ctrl?"snap":"free");gimp_display_shell_rotate(&s,&e,x,y);emit(&s,scene,step,e.state,x,y);}
 gimp_display_shell_stop_rotating(&s,NULL);emit(&s,scene,"stop",0,cx,cy+40);
 }
 return 0;
}
