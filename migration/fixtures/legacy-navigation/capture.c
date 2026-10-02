/* Extracted unchanged functions; GTK grab/cursor/expose are no-op stubs.
 * This is executable source-arithmetic evidence, not an actual GUI event run. */
#include <math.h>
#include <stdio.h>
#include <assert.h>
typedef double gdouble; typedef int gint;
typedef struct { unsigned state; } GdkEvent, GdkEventMotion;
typedef struct { int rotating, disp_width, disp_height, mirrored; double rotate_angle, rotate_start_angle; } GimpDisplayShell;
#define TRUE 1
#define GDK_CONTROL_MASK 4
#define GDK_POINTER_MOTION_MASK 0
#define GDK_EXCHANGE 0
#define ROTATE_SNAP_UNIT 15.0
#define g_return_if_fail(x) assert(x)
#define gimp_display_shell_pointer_grab(...) ((void)0)
#define gimp_display_shell_set_override_cursor(...) ((void)0)
#define gimp_display_shell_expose_full(...) ((void)0)
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
int main(void) {
 const double angles[]={0,7.5,22.5,337.5,352.5,359.999};
 const int points[][2]={{0,0},{40,0},{0,40},{-40,0},{0,-40},{23,37},{-23,37},{-23,-37},{23,-37}};
 const unsigned states[]={0,1,4,5,13,8};
 for(int w=100;w<=101;w++) for(int mirror=0;mirror<2;mirror++)
 for(unsigned a=0;a<6;a++) for(unsigned s=0;s<4;s++) {
  GimpDisplayShell shell={0,w,w+12,mirror,angles[a],0};
  int sx=w/2+points[s][0],sy=(w+12)/2+points[s][1];
  gimp_display_shell_start_rotating(&shell,NULL,sx,sy);
  for(unsigned p=0;p<9;p++) for(unsigned m=0;m<6;m++) {
   GdkEvent event={states[m]}; int x=w/2+points[p][0],y=(w+12)/2+points[p][1];
   gimp_display_shell_rotate(&shell,&event,x,y);
   printf("%d\t%d\t%d\t%.17g\t%d\t%d\t%d\t%d\t%u\t%.17g\n",w,w+12,mirror,angles[a],sx,sy,x,y,states[m],shell.rotate_angle);
  }
 }
 return 0;
}
