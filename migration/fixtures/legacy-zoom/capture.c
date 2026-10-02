/* Extracted unchanged functions; GTK grab/cursor/expose are no-op stubs.
 * This is executable source-arithmetic evidence, not an actual GUI event run. */
#include <math.h>
#include <stdio.h>
#include <assert.h>
typedef double gdouble; typedef int gint;
typedef struct { unsigned state; } GdkEvent, GdkEventMotion;
typedef struct { int scaling, disp_width, disp_height, mirrored; double scale_x, scale_y, scaling_start_distance, result; } GimpDisplayShell;
#define TRUE 1
#define GDK_CONTROL_MASK 4
#define GDK_POINTER_MOTION_MASK 0
#define GDK_EXCHANGE 0
#define ZOOM_UNIT_DISTANCE 300.0
#define GDK_SIZING 0
#define GIMP_ZOOM_TO 0
#define GIMP_ZOOM_FOCUS_RETAIN_CENTERING_ELSE_BEST_GUESS 0
#define MAX(a,b) ((a)>(b)?(a):(b))
#define gimp_display_shell_scale(shell,mode,value,focus) ((shell)->result=(value))
#define g_return_if_fail(x) assert(x)
#define gimp_display_shell_pointer_grab(...) ((void)0)
#define gimp_display_shell_set_override_cursor(...) ((void)0)
#define gimp_display_shell_expose_full(...) ((void)0)
static void
gimp_display_shell_start_scaling   (GimpDisplayShell *shell,
                                    const GdkEvent   *event,
                                    gint              x,
                                    gint              y)
{
  gdouble cx, cy;
  gdouble rx, ry;
  g_return_if_fail (! shell->scaling);

  gimp_display_shell_pointer_grab (shell, event, GDK_POINTER_MOTION_MASK);

  shell->scaling      = TRUE;

  cx = shell->disp_width  / 2;
  cy = shell->disp_height / 2;
  rx = x - cx;
  ry = y - cy;

  if (shell->mirrored)
    rx = -rx;

  shell->scaling_start_distance = MAX(shell->scale_x, shell->scale_y) * ZOOM_UNIT_DISTANCE - 
                                  sqrt(rx * rx + ry * ry); 

  gimp_display_shell_set_override_cursor (shell, GDK_SIZING);
}

static void
gimp_display_shell_do_scaling    (GimpDisplayShell *shell,
                                  gint              x,
                                  gint              y)
{
  gdouble cx, cy;
  gdouble rx, ry;
  gdouble distance;
  gdouble distance_diff;
  g_return_if_fail (shell->scaling);

  cx = shell->disp_width  / 2;
  cy = shell->disp_height / 2;
  rx = x - cx;
  ry = y - cy;

  distance      = sqrt(rx * rx + ry * ry);
  distance_diff = distance + shell->scaling_start_distance;

  gimp_display_shell_scale(shell, GIMP_ZOOM_TO, distance_diff / ZOOM_UNIT_DISTANCE, 
                           GIMP_ZOOM_FOCUS_RETAIN_CENTERING_ELSE_BEST_GUESS);
  gimp_display_shell_expose_full(shell);
}
int main(void) {
 const double scales[]={0.1,1.0/3.0,1.0,1.25,2,16};
 const int points[][2]={{0,0},{40,0},{0,40},{-40,0},{0,-40},{23,37},{-23,37},{-23,-37},{23,-37},{400,0}};
 for(int w=100;w<=101;w++) for(int mirror=0;mirror<2;mirror++)
 for(unsigned a=0;a<6;a++) for(unsigned s=0;s<10;s++) {
  GimpDisplayShell shell={0,w,w+12,mirror,scales[a],scales[(a+1)%6],0,0};
  int sx=w/2+points[s][0],sy=(w+12)/2+points[s][1];
  gimp_display_shell_start_scaling(&shell,NULL,sx,sy);
  for(unsigned p=0;p<10;p++) {
   int x=w/2+points[p][0],y=(w+12)/2+points[p][1];
   gimp_display_shell_do_scaling(&shell,x,y);
   printf("%d\t%d\t%d\t%.17g\t%.17g\t%d\t%d\t%d\t%d\t%.17g\n",w,w+12,mirror,shell.scale_x,shell.scale_y,sx,sy,x,y,shell.result);
  }
 }
 return 0;
}
