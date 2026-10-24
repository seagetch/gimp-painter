/* Actual Paintbrush, source-only old Gaussian oracle, deterministic main-thread
 * painting. Wrappers observe and forward every production virtual call. */
#include "core/gimpdrawable.h"
#include "core/gimpimage-undo.h"
#include "core/gimpbrush.h"
#include "paint/paint-types.h"
#include "paint/gimppaintcore.h"
#include "paint/gimppaintoptions.h"
#include "tools/gimppainttool.h"
#include "../../migration/fixtures/legacy-perspective-stroke/oracle.inc"
#include "../../migration/fixtures/legacy-perspective-smoothing/oracle.inc"
static void (*smoothing_real_motion) (GimpTool *, const GimpCoords *, guint32, GdkModifierType, GimpDisplay *);
static void (*smoothing_real_interpolate) (GimpPaintCore *, GList *, GimpPaintOptions *, guint32);
static LegacySmoothingCore smoothing_old;
static GimpCoords smoothing_expected;
static guint smoothing_inputs, smoothing_outputs, smoothing_changed;
static gdouble smoothing_angle, smoothing_ox, smoothing_oy;
static gboolean smoothing_waiting;
static void smoothing_motion (GimpTool *tool, const GimpCoords *coords, guint32 time,
                              GdkModifierType state, GimpDisplay *display)
{
  GimpPaintTool *paint = GIMP_PAINT_TOOL (tool);
  LegacyStrokeShell old = { 0 }; GimpCoords constrained = *coords;
  old.snap_origin.x = smoothing_ox; old.snap_origin.y = smoothing_oy;
  old.snapped_angle = smoothing_angle;
  g_assert_true (legacy_stroke_lazy_snap (&old, image, &constrained));
  g_assert_cmpfloat_with_epsilon (coords->x, constrained.x, 1e-7);
  g_assert_cmpfloat_with_epsilon (coords->y, constrained.y, 1e-7);
  g_assert_false (smoothing_waiting);
  smoothing_expected = *coords;
  legacy_perspective_smooth_coords (&smoothing_old, GIMP_PAINT_TOOL_GET_OPTIONS (paint), &smoothing_expected);
  if (fabs (smoothing_expected.x-coords->x)+fabs(smoothing_expected.y-coords->y)>1e-6) smoothing_changed++;
  smoothing_inputs++; smoothing_waiting=TRUE;
  smoothing_real_motion (tool, coords, time, state, display);
  g_assert_false (smoothing_waiting);
}
static void smoothing_interpolate (GimpPaintCore *core, GList *drawables,
                                   GimpPaintOptions *options, guint32 time)
{
  g_assert_true (smoothing_waiting);
  g_assert_cmpfloat_with_epsilon (core->cur_coords.x, smoothing_expected.x, 1e-10);
  g_assert_cmpfloat_with_epsilon (core->cur_coords.y, smoothing_expected.y, 1e-10);
  g_assert_cmpfloat (core->cur_coords.pressure, ==, smoothing_expected.pressure);
  g_assert_cmpfloat (core->cur_coords.velocity, ==, smoothing_expected.velocity);
  smoothing_outputs++; smoothing_waiting=FALSE;
  smoothing_real_interpolate (core, drawables, options, time);
}
static GimpCoords smoothing_boundary (gdouble x, gdouble y)
{
  GimpCoords d={0},p={0};d.x=x*shell->scale_x-shell->offset_x;d.y=y*shell->scale_y-shell->offset_y;
  if(shell->rotate_transform)cairo_matrix_transform_point(shell->rotate_transform,&d.x,&d.y);
  gimp_display_shell_untransform_coords(shell,&d,&p);return p;
}
static void smoothing_real_paintbrush (void)
{
  guint strokes=0, total_inputs=0, total_changed=0;
  const gdouble angles[]={0,37,90}, points[][2]={{1000,1000},{-1000,1000},{128,-1000}};
  GimpToolClass *tool_class; GimpPaintCoreClass *core_class; GimpPaintTool *paint;
  GimpPaintOptions *options; GList *drawables; GimpDrawable *drawable; GeglBuffer *buffer;
  guchar before[256*256*4], after[256*256*4];
  create_image (); guide(3);gimp_painter_canvas_ui_set_visible(shell,TRUE);
  g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
  gimp_context_set_tool(gimp_get_user_context(gimp),gimp_get_tool_info(gimp,"gimp-paintbrush-tool"));
  paint=GIMP_PAINT_TOOL(active());options=GIMP_PAINT_TOOL_GET_OPTIONS(paint);
  g_assert_nonnull(gimp_context_get_brush(GIMP_CONTEXT(options)));
  g_object_set(options,"brush-size",9.0,"dynamics-enabled",FALSE,"use-smoothing",TRUE,
    "smoothing-quality",4,"smoothing-factor",50.0,NULL);
  gimp_tool_control_set_snap_to(active()->control,FALSE);
  drawables=gimp_image_get_selected_drawables(image);g_assert_cmpuint(g_list_length(drawables),==,1);
  drawable=drawables->data;buffer=gimp_drawable_get_buffer(drawable);g_list_free(drawables);
  gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,256,256),1,babl_format("R'G'B'A u8"),before,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  tool_class=GIMP_TOOL_GET_CLASS(paint);core_class=GIMP_PAINT_CORE_GET_CLASS(paint->core);
  smoothing_real_motion=tool_class->motion;smoothing_real_interpolate=core_class->interpolate;
  tool_class->motion=smoothing_motion;core_class->interpolate=smoothing_interpolate;
  gimp_display_shell_set_perspective_snap(shell,TRUE);
  for(guint a=0;a<3;a++)for(guint flip=0;flip<4;flip++)for(guint ray=0;ray<3;ray++)
    {
      LegacyStrokeShell old={0};GimpCoords crossing;
      gdouble ox=120.125,oy=120.375,dx=points[ray][0]-ox,dy=points[ray][1]-oy,unit;
      gimp_image_set_resolution(image,(flip&1)?144:72,72);
      gimp_display_shell_scale_set_dot_for_dot(shell,!(flip&1));
      gimp_display_shell_scale(shell,GIMP_ZOOM_TO,1.25,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
      gimp_display_shell_rotate_to(shell,angles[a]);gimp_display_shell_flip(shell,flip&1,flip&2);
      gimp_display_shell_scroll_set_offset(shell,(gint)(128*shell->scale_x)-shell->disp_width/2,
        (gint)(128*shell->scale_y)-shell->disp_height/2);
      g_assert_true(gimp_display_shell_painter_pan_begin(shell,shell->disp_width/2,shell->disp_height/2));
      gimp_display_shell_painter_pan_motion(shell,shell->disp_width/2+.375,shell->disp_height/2-.625);
      unit=sqrt(dx*dx*shell->scale_x*shell->scale_x+dy*dy*shell->scale_y*shell->scale_y);dx/=unit;dy/=unit;
      smoothing_old.stroke_buffer=g_array_new(FALSE,FALSE,sizeof(GimpCoords));
      smoothing_inputs=smoothing_outputs=smoothing_changed=0;smoothing_waiting=FALSE;
      editor_event(GDK_MOTION_NOTIFY,0,ox,oy,0);editor_event(GDK_BUTTON_PRESS,0,ox,oy,0);
      g_assert_true(shell->perspective_pending);old.snap_origin=shell->perspective_origin;
      old.scale_x=shell->scale_x;old.scale_y=shell->scale_y;old.snapping=TRUE;
      crossing=smoothing_boundary(ox+dx*40,oy+dy*40);g_assert_true(legacy_stroke_lazy_snap(&old,image,&crossing));
      smoothing_angle=old.snapped_angle;smoothing_ox=old.snap_origin.x;smoothing_oy=old.snap_origin.y;
      editor_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,ox+dx*40,oy+dy*40,0);
      g_assert_true(shell->perspective_locked);g_assert_true(gimp_tool_control_is_active(active()->control));
      g_assert_cmpuint(smoothing_inputs,==,0);
      for(guint sample=0;sample<6;sample++)
        {
          gdouble distance=48+sample*9, jitter=(sample%2)?5:-4;
          editor_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,ox+dx*distance-dy*jitter,oy+dy*distance+dx*jitter,0);
        }
      gimp_motion_buffer_end_stroke(shell->motion_buffer);
      editor_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,ox+dx*93,oy+dy*93,0);assert_reset();
      g_assert_cmpuint(smoothing_inputs,>,1);g_assert_cmpuint(smoothing_outputs,==,smoothing_inputs);
      g_assert_cmpuint(smoothing_changed,>,0);total_inputs+=smoothing_inputs;total_changed+=smoothing_changed;
      g_array_free(smoothing_old.stroke_buffer,TRUE);smoothing_old.stroke_buffer=NULL;strokes++;
    }
  tool_class->motion=smoothing_real_motion;core_class->interpolate=smoothing_real_interpolate;
  gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,256,256),1,babl_format("R'G'B'A u8"),after,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpint(memcmp(before,after,sizeof(before)),!=,0);
  guint ink=0;for(guint i=0;i<256*256;i++)if(after[i*4+3])ink++;
  g_assert_cmpuint(ink,>,100);g_assert_cmpuint(strokes,==,36);
  g_test_message("actual Paintbrush: %u three-point transformed strokes, %u constrained-before-Gaussian samples, %u smoothed samples, %u alpha pixels; non-threaded synthetic master pointer",strokes,total_inputs,total_changed,ink);
  lifecycle_capture("paintbrush-smoothed-three-directions");close_image();
}
