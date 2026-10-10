/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <math.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "display/display-types.h"
#include "tools/tools-types.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimptooloptions.h"
#include "tools/tool_manager.h"
#include "widgets/gimpwidgets-utils.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpimage-perspective-guide.h"
#include "core/gimptoolinfo.h"
#include "core/gimptoolgroup.h"
#include "core/gimplist.h"
#include "tools/gimp-tools.h"
#include "core/gimpundostack.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-callbacks.h"
#include "display/gimpdisplayshell-rotate.h"
#include "display/gimpdisplayshell-scale.h"
#include "display/gimpdisplayshell-transform.h"
#include "display/gimpcanvasitem.h"
#include "tools/gimpperspectiveguidetool.h"
#include "tools/gimptoolcontrol.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
static Gimp *gimp;
static GimpDisplayShell *shell;
static GimpImage *image;
static GimpTool *tool;
static GimpToolClass *klass;
static void create_image(void)
{
  GimpToolInfo *info;
  gimp_set_focused_once(gimp);gimp_test_utils_create_image(gimp,256,256);gimp_test_run_mainloop_until_idle();
  g_assert_cmpuint(g_list_length(gimp_get_display_iter(gimp)),==,1);
  shell=gimp_display_get_shell(gimp_get_display_iter(gimp)->data);image=gimp_display_get_image(shell->display);
  info=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,"gimp-perspective-guide-tool"));
  g_assert_nonnull(info);tool=g_object_new(GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL,"tool-info",info,NULL);klass=GIMP_TOOL_GET_CLASS(tool);
}
static void close_image(void)
{
  GimpDisplay *display=shell->display;
  gimp_tool_control(tool,GIMP_TOOL_ACTION_HALT,display);g_object_unref(tool);
  g_object_unref(image);gimp_display_close(display);gimp_test_run_mainloop_until_idle();
}
static void click(gdouble x,gdouble y,GdkModifierType state)
{
  GimpCoords coords={0};coords.x=x;coords.y=y;
  klass->oper_update(tool,&coords,state,TRUE,shell->display);
  klass->button_press(tool,&coords,0,state,GIMP_BUTTON_PRESS_NORMAL,shell->display);
  klass->button_release(tool,&coords,0,state,GIMP_BUTTON_RELEASE_NORMAL,shell->display);
  g_assert_false(gimp_tool_control_is_active(tool->control));
}
static void check_point(gint index,gdouble ex,gdouble ey)
{
  gdouble x,y;GimpPerspectiveGuide *guide=gimp_image_get_perspective_guide(image);
  g_assert_nonnull(guide);g_assert_true(gimp_perspective_guide_get_vanish_points(guide,index,&x,&y));
  g_assert_cmpfloat(x,==,ex);g_assert_cmpfloat(y,==,ey);
}
static void add_move_remove_undo(void)
{
  GimpCoords coords={0};gint depth;
  create_image();click(10,20,0);g_assert_null(gimp_image_get_perspective_guide(image));
  klass->modifier_key(tool,GDK_SHIFT_MASK,TRUE,GDK_SHIFT_MASK,shell->display);
  click(10,20,GDK_SHIFT_MASK);click(70,90,GDK_SHIFT_MASK);click(200,240,GDK_SHIFT_MASK);click(250,250,GDK_SHIFT_MASK);
  g_assert_cmpint(gimp_perspective_guide_get_vanish_point_length(gimp_image_get_perspective_guide(image)),==,3);
  check_point(0,10,20);check_point(1,70,90);check_point(2,200,240);
  klass->modifier_key(tool,GDK_SHIFT_MASK,FALSE,0,shell->display);
  depth=gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image));
  coords.x=70;coords.y=90;klass->button_press(tool,&coords,0,0,GIMP_BUTTON_PRESS_NORMAL,shell->display);
  for(gint i=0;i<10;++i){coords.x=80+i;coords.y=100+i;klass->motion(tool,&coords,0,GDK_BUTTON1_MASK,shell->display);}
  klass->button_release(tool,&coords,0,0,GIMP_BUTTON_RELEASE_NORMAL,shell->display);check_point(1,89,109);
  g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,depth+1);
  g_assert_true(gimp_image_undo(image));check_point(1,70,90);g_assert_true(gimp_image_redo(image));check_point(1,89,109);
  /* Hover updates after undo/redo must adopt the current model. */
  klass->oper_update(tool,&coords,0,TRUE,shell->display);
  klass->modifier_key(tool,GDK_CONTROL_MASK,TRUE,GDK_CONTROL_MASK,shell->display);click(89,109,GDK_CONTROL_MASK);
  g_assert_cmpint(gimp_perspective_guide_get_vanish_point_length(gimp_image_get_perspective_guide(image)),==,2);check_point(1,200,240);
  g_assert_true(gimp_image_undo(image));check_point(1,89,109);check_point(2,200,240);
  close_image();
}
static void mark_frame_painted (GdkFrameClock *clock, gboolean *painted)
{
  *painted = TRUE;
}
static void close_with_pending_resize (void)
{
  GtkAllocation allocation;
  GdkFrameClock *clock;
  gboolean painted = FALSE;
  gulong handler;
  gint64 deadline;

  create_image ();
  klass->modifier_key (tool, GDK_SHIFT_MASK, TRUE, GDK_SHIFT_MASK, shell->display);
  click (24, 36, GDK_SHIFT_MASK);
  check_point (0, 24, 36);
  gtk_widget_get_allocation (shell->canvas, &allocation);
  g_assert_cmpint (allocation.width, >, 64);
  clock = gtk_widget_get_frame_clock (shell->canvas);
  g_assert_nonnull (clock);
  handler = g_signal_connect (clock, "after-paint",
                              G_CALLBACK (mark_frame_painted), &painted);

  /* Queue the same deferred resize work as a window configure event, then
   * close the last image before the frame clock is allowed to run it. */
  shell->zoom_on_resize = FALSE;
  shell->size_allocate_from_configure_event = TRUE;
  shell->size_allocate_center_image = TRUE;
  shell->disp_width = allocation.width - 1;
  gimp_display_shell_canvas_size_allocate (shell->canvas, &allocation, shell);
  close_image ();
  g_assert_null (gimp_display_get_image (shell->display));
  g_assert_null (gimp_canvas_item_get_extents (shell->perspective_guide));
  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_PAINT);
  deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  while (! painted && g_get_monotonic_time () < deadline)
    gimp_test_run_temp_mainloop (10);
  g_assert_true (painted);
  g_assert_false (shell->size_allocate_from_configure_event);
  g_assert_false (shell->size_allocate_center_image);
  g_signal_handler_disconnect (clock, handler);

  /* Reusing the retained empty display must still accept a new image. */
  create_image ();
  klass->modifier_key (tool, GDK_SHIFT_MASK, TRUE, GDK_SHIFT_MASK, shell->display);
  click (32, 48, GDK_SHIFT_MASK);
  check_point (0, 32, 48);
  close_image ();
}
static void cancel_and_hit_transforms(void)
{
  GimpCoords coords={0};GimpPerspectiveGuide *guide;gdouble zooms[]={0.25,1,4};
  create_image();guide=gimp_perspective_guide_new(0);gimp_perspective_guide_add_vanish_points(guide,80,90);gimp_image_set_perspective_guide(image,guide);g_object_unref(guide);
  for(guint z=0;z<G_N_ELEMENTS(zooms);++z)for(guint flip=0;flip<4;++flip)
    {
      gimp_display_shell_scale(shell,GIMP_ZOOM_TO,zooms[z],GIMP_ZOOM_FOCUS_IMAGE_CENTER);
      gimp_display_shell_rotate_to(shell,37);gimp_display_shell_flip(shell,flip&1,flip&2);
      coords.x=80+20/shell->scale_x;coords.y=90;
      klass->oper_update(tool,&coords,0,TRUE,shell->display);klass->button_press(tool,&coords,0,0,GIMP_BUTTON_PRESS_NORMAL,shell->display);
      coords.x=150;coords.y=160;klass->motion(tool,&coords,0,0,shell->display);klass->button_release(tool,&coords,0,0,GIMP_BUTTON_RELEASE_NORMAL,shell->display);check_point(0,80,90);
      coords.x=80;coords.y=90;klass->button_press(tool,&coords,0,0,GIMP_BUTTON_PRESS_NORMAL,shell->display);
      coords.x=150;coords.y=160;klass->motion(tool,&coords,0,0,shell->display);check_point(0,150,160);
      klass->button_release(tool,&coords,0,0,GIMP_BUTTON_RELEASE_CANCEL,shell->display);check_point(0,80,90);
      g_assert_cmpint(GIMP_DRAW_TOOL(tool)->paused_count,==,0);
    }
  close_image();
}
static void overlay_extents_and_replace(void)
{
  GimpPerspectiveGuide *guide;const gdouble points[][2]={{10,20},{500,220},{100,-150}};
  create_image();g_assert_nonnull(shell->perspective_guide);
  g_assert_null(gimp_canvas_item_get_extents(shell->perspective_guide));
  guide=gimp_perspective_guide_new(0);gimp_image_set_perspective_guide(image,guide);
  for(gint n=0;n<3;++n)
    {
      gimp_perspective_guide_add_vanish_points(guide,points[n][0],points[n][1]);
      for(guint flip=0;flip<4;++flip)
        {
          cairo_region_t *region;gimp_display_shell_rotate_to(shell,63);gimp_display_shell_flip(shell,flip&1,flip&2);
          region=gimp_canvas_item_get_extents(shell->perspective_guide);g_assert_nonnull(region);
          for(gint i=0;i<=n;++i){gdouble x,y;gimp_canvas_item_transform_xy_f(shell->perspective_guide,points[i][0],points[i][1],&x,&y);g_assert_true(cairo_region_contains_point(region,(gint)floor(x),(gint)floor(y)));}
          cairo_region_destroy(region);
        }
    }
  gimp_image_set_perspective_guide(image,NULL);g_assert_null(gimp_canvas_item_get_extents(shell->perspective_guide));
  /* A disconnected old guide cannot restore the overlay. */
  gimp_perspective_guide_set_vanish_points(guide,0,999,999);g_assert_null(gimp_canvas_item_get_extents(shell->perspective_guide));
  g_object_unref(guide);close_image();
}
static void overlay_pixels_and_image_switch(void)
{
  GimpPerspectiveGuide *guide,*other_guide;
  GimpImage *other;
  const gdouble zooms[]={0.25,1,4};
  create_image();guide=gimp_perspective_guide_new(0);
  gimp_perspective_guide_add_vanish_points(guide,128,128);gimp_image_set_perspective_guide(image,guide);
  for(guint z=0;z<G_N_ELEMENTS(zooms);++z)for(guint flip=0;flip<4;++flip)
    {
      cairo_surface_t *surface;cairo_t *cr;gdouble x,y;gboolean painted=FALSE;
      gimp_display_shell_scale(shell,GIMP_ZOOM_TO,zooms[z],GIMP_ZOOM_FOCUS_IMAGE_CENTER);
      gimp_display_shell_rotate_to(shell,37);gimp_display_shell_flip(shell,flip&1,flip&2);
      surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,shell->disp_width,shell->disp_height);cr=cairo_create(surface);
      if(shell->rotate_transform)cairo_transform(cr,shell->rotate_transform);
      gimp_canvas_item_draw(shell->perspective_guide,cr);cairo_destroy(cr);cairo_surface_flush(surface);
      gimp_display_shell_transform_xy_f(shell,128,128,&x,&y);
      for(gint py=MAX(0,(gint)y-12);py<MIN(shell->disp_height,(gint)y+13);++py)
        for(gint px=MAX(0,(gint)x-12);px<MIN(shell->disp_width,(gint)x+13);++px)
          if(((guint32*)(cairo_image_surface_get_data(surface)+py*cairo_image_surface_get_stride(surface)))[px])painted=TRUE;
      g_assert_true(painted);g_assert_cmpint(cairo_surface_status(surface),==,CAIRO_STATUS_SUCCESS);cairo_surface_destroy(surface);
    }
  other=gimp_image_new(gimp,256,256,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  other_guide=gimp_perspective_guide_new(0);gimp_perspective_guide_add_vanish_points(other_guide,64,96);gimp_image_set_perspective_guide(other,other_guide);
  gimp_display_set_image(shell->display,other);
  {
    cairo_region_t *before=gimp_canvas_item_get_extents(shell->perspective_guide),*after;
    g_assert_nonnull(before);gimp_perspective_guide_set_vanish_points(guide,0,9999,9999);
    after=gimp_canvas_item_get_extents(shell->perspective_guide);g_assert_true(cairo_region_equal(before,after));cairo_region_destroy(before);cairo_region_destroy(after);
  }
  gimp_image_set_perspective_guide(other,NULL);g_assert_null(gimp_canvas_item_get_extents(shell->perspective_guide));
  gimp_display_set_image(shell->display,image);{cairo_region_t *region=gimp_canvas_item_get_extents(shell->perspective_guide);g_assert_nonnull(region);cairo_region_destroy(region);}
  g_object_unref(other_guide);g_object_unref(other);g_object_unref(guide);close_image();
}
static void registration_and_external_replace(void)
{
  GimpToolInfo *info;guint guide_key,gradient_key;GdkModifierType guide_mod,gradient_mod;
  GimpCoords coords={0};GimpPerspectiveGuide *replacement;
  create_image();
  info=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,"gimp-perspective-guide-tool"));gtk_accelerator_parse(info->menu_accel,&guide_key,&guide_mod);
  info=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,"gimp-gradient-tool"));gtk_accelerator_parse(info->menu_accel,&gradient_key,&gradient_mod);
  g_assert_cmpuint(guide_key,==,GDK_KEY_g);g_assert_cmpuint(gradient_key,==,GDK_KEY_l);g_assert_cmpuint(guide_mod,==,0);g_assert_cmpuint(gradient_mod,==,0);
  for(gint i=0;i<gimp_container_get_n_children(gimp->tool_info_list);++i){guint key;GdkModifierType mod;info=GIMP_TOOL_INFO(gimp_container_get_child_by_index(gimp->tool_info_list,i));gtk_accelerator_parse(info->menu_accel ? info->menu_accel : "",&key,&mod);if(key==guide_key && mod==guide_mod)g_assert_cmpstr(gimp_object_get_name(info),==,"gimp-perspective-guide-tool");if(key==gradient_key && mod==gradient_mod)g_assert_cmpstr(gimp_object_get_name(info),==,"gimp-gradient-tool");}
  klass->modifier_key(tool,GDK_SHIFT_MASK,TRUE,GDK_SHIFT_MASK,shell->display);click(10,20,GDK_SHIFT_MASK);klass->modifier_key(tool,GDK_SHIFT_MASK,FALSE,0,shell->display);
  coords.x=10;coords.y=20;klass->button_press(tool,&coords,0,0,GIMP_BUTTON_PRESS_NORMAL,shell->display);coords.x=30;coords.y=40;klass->motion(tool,&coords,0,0,shell->display);
  replacement=gimp_perspective_guide_new(0);gimp_perspective_guide_add_vanish_points(replacement,100,110);gimp_image_set_perspective_guide(image,replacement);
  klass->button_release(tool,&coords,0,0,GIMP_BUTTON_RELEASE_CANCEL,shell->display);g_assert_true(gimp_image_get_perspective_guide(image)==replacement);check_point(0,100,110);
  g_object_unref(replacement);close_image();
}
static void old_toolrc_preserves_groups(void)
{
  gchar *text=NULL,*path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration","fixtures","legacy-perspective","gimp3-before-ruler-toolrc",NULL);
  GError *error=NULL;GScanner *scanner;GimpContainer *items=gimp->tool_item_list;GimpObject *first;GimpContainer *children;
  create_image();
  g_assert_true(g_file_get_contents(path,&text,NULL,&error));g_assert_no_error(error);g_free(path);
  scanner=gimp_scanner_new_string(text,-1,&error);g_assert_no_error(error);
  /* Move the live tool infos only after detaching their existing groups, as reset does. */
  gimp_container_clear(items);
  g_assert_true(gimp_tools_deserialize(gimp,items,scanner));g_assert_no_error(error);
  first=gimp_container_get_child_by_index(items,0);g_assert_true(GIMP_IS_TOOL_GROUP(first));
  g_assert_cmpstr(gimp_tool_group_get_active_tool(GIMP_TOOL_GROUP(first)),==,"gimp-move-tool");
  children=gimp_viewable_get_children(GIMP_VIEWABLE(first));g_assert_cmpint(gimp_container_get_n_children(children),==,2);
  g_assert_cmpstr(gimp_object_get_name(gimp_container_get_child_by_index(children,1)),==,"gimp-align-tool");
  {
    const gchar *new_tools[] = {
      "gimp-perspective-guide-tool", "gimp-bucket-fill-brush-tool",
      "gimp-painter-mypaint-tool", "gimp-painter-smudge-tool"
    };
    GimpObject *second = gimp_container_get_child_by_index (items, 1);
    GimpObject *last_old = gimp_container_get_child_by_name (items, "gimp-gegl-tool");
    g_assert_nonnull (last_old);
    g_assert_true (GIMP_IS_TOOL_GROUP (second));
    g_assert_cmpstr (gimp_tool_group_get_active_tool (GIMP_TOOL_GROUP (second)),
                     ==, "gimp-rect-select-tool");
    for (guint i = 0; i < G_N_ELEMENTS (new_tools); i++)
      {
        GimpObject *added = gimp_container_get_child_by_name (items, new_tools[i]);
        g_assert_null (g_strstr_len (text, -1, new_tools[i]));
        g_assert_nonnull (added);
        g_assert_cmpint (gimp_container_get_child_index (items, added), >,
                         gimp_container_get_child_index (items, last_old));
      }
  }
  gimp_scanner_unref(scanner);g_free(text);close_image();
}
typedef struct { guint calls; gboolean dispose; } InterruptState;
static void interrupt_edit (GObject *emitter, InterruptState *state)
{
  if (state->calls++) return;
  if (state->dispose) g_object_run_dispose (G_OBJECT (tool));
  else gimp_tool_control (tool, GIMP_TOOL_ACTION_HALT, shell->display);
}
static void interrupted_add (gconstpointer data)
{
  GimpCoords coords = { 0 };
  InterruptState state = { 0, GPOINTER_TO_INT (data) };
  gulong handler;
  create_image ();
  coords.x = 24; coords.y = 36;
  klass->oper_update (tool, &coords, 0, TRUE, shell->display);
  klass->modifier_key (tool, GDK_SHIFT_MASK, TRUE, GDK_SHIFT_MASK, shell->display);
  handler = g_signal_connect (image, "perspective-guide-changed", G_CALLBACK (interrupt_edit), &state);
  klass->button_press (tool, &coords, 0, GDK_SHIFT_MASK, GIMP_BUTTON_PRESS_NORMAL, shell->display);
  g_signal_handler_disconnect (image, handler);
  g_assert_cmpuint (state.calls, >, 0);
  g_assert_false (gimp_tool_control_is_active (tool->control));
  g_assert_null (gimp_image_get_perspective_guide (image));
  close_image ();
}
static void interrupted_remove (gconstpointer data)
{
  GimpCoords coords = { 0 };
  InterruptState state = { 0, GPOINTER_TO_INT (data) };
  GimpPerspectiveGuide *guide;
  gulong handler;
  create_image ();
  guide = gimp_perspective_guide_new (0);
  gimp_perspective_guide_add_vanish_points (guide, 24, 36);
  gimp_image_set_perspective_guide (image, guide);
  coords.x = 24; coords.y = 36;
  klass->oper_update (tool, &coords, 0, TRUE, shell->display);
  klass->modifier_key (tool, GDK_CONTROL_MASK, TRUE, GDK_CONTROL_MASK, shell->display);
  handler = g_signal_connect (guide, "changed", G_CALLBACK (interrupt_edit), &state);
  klass->button_press (tool, &coords, 0, GDK_CONTROL_MASK, GIMP_BUTTON_PRESS_NORMAL, shell->display);
  g_signal_handler_disconnect (guide, handler);
  g_assert_cmpuint (state.calls, ==, 1);
  g_assert_false (gimp_tool_control_is_active (tool->control));
  check_point (0, 24, 36);
  g_object_unref (guide);
  close_image ();
}
static void proximity_status (void)
{
  GimpCoords coords = { 0 };
  create_image ();
  klass->oper_update (tool, &coords, 0, TRUE, shell->display);
  g_assert_nonnull (g_list_find (tool->status_displays, shell->display));
  klass->oper_update (tool, &coords, 0, FALSE, shell->display);
  g_assert_null (g_list_find (tool->status_displays, shell->display));
  close_image ();
}
static void mark_finalized (gpointer data)
{ *(gboolean *) data = TRUE; }
static void registered_type_options (void)
{
  GimpToolInfo *info;
  GimpTool *active;
  GimpPerspectiveGuideToolClass *native_class;
  GTypeQuery query;
  GtkWidget *gui;
  gboolean visible = FALSE;
  create_image ();
  info = gimp_get_tool_info (gimp, "gimp-perspective-guide-tool");
  g_assert_cmpuint (info->tool_type, ==, GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL);
  g_assert_cmpuint (info->tool_options_type, ==, GIMP_TYPE_TOOL_OPTIONS);
  g_assert_cmpuint (info->context_props, ==, 0);
  g_assert_false (info->hidden);
  g_object_get (info, "visible", &visible, NULL);
  g_assert_true (visible);
  /* GIMP3 restores visibility from toolrc, not the old private qdata. */
  gimp_tool_item_set_visible (GIMP_TOOL_ITEM (info), FALSE);
  {
    /* Test this checkout's shipped defaults, independent of the dependency
     * prefix's installed GIMP configuration. */
    gchar *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "etc", "toolrc", NULL);
    gchar *text = NULL;
    GError *error = NULL;
    GScanner *scanner;
    g_assert_true (g_file_get_contents (path, &text, NULL, &error)); g_assert_no_error (error);
    scanner = gimp_scanner_new_string (text, -1, &error); g_assert_no_error (error);
    gimp_container_clear (gimp->tool_item_list);
    g_assert_true (gimp_tools_deserialize (gimp, gimp->tool_item_list, scanner));
    gimp_scanner_unref (scanner); g_free (text); g_free (path);
  }
  g_assert_true (gimp_tool_item_get_visible (GIMP_TOOL_ITEM (info)));
  gimp_context_set_tool (gimp_get_user_context (gimp), info);
  active = g_object_ref (tool_manager_get_active (gimp));
  g_assert_true (GIMP_IS_PERSPECTIVE_GUIDE_TOOL (active));
  g_assert_true (GIMP_TOOL (GIMP_PERSPECTIVE_GUIDE_TOOL (active)) == active);
  g_assert_true (active->tool_info == info);
  g_assert_true (gimp_tool_get_options (active) == info->tool_options);
  g_assert_cmpuint (G_OBJECT_TYPE (info->tool_options), ==, GIMP_TYPE_TOOL_OPTIONS);
  g_assert_cmpuint (g_type_parent (G_OBJECT_TYPE (active)), ==, GIMP_TYPE_DRAW_TOOL);
  native_class = GIMP_PERSPECTIVE_GUIDE_TOOL_GET_CLASS (active);
  g_assert_true (GIMP_IS_PERSPECTIVE_GUIDE_TOOL_CLASS (native_class));
  g_assert_true (GIMP_PERSPECTIVE_GUIDE_TOOL_CLASS (G_OBJECT_GET_CLASS (active)) == native_class);
  g_type_query (G_OBJECT_TYPE (active), &query);
  g_assert_cmpuint (query.instance_size, ==, sizeof (GimpPerspectiveGuideTool));
  g_assert_cmpuint (query.class_size, ==, sizeof (GimpPerspectiveGuideToolClass));
  gui = gimp_tools_get_tool_options_gui (info->tool_options);
  g_assert_true (GTK_IS_BOX (gui));
  g_assert_true (gui == gimp_tools_get_tool_options_gui (info->tool_options));
  gimp_context_set_tool (gimp_get_user_context (gimp), gimp_get_tool_info (gimp, "gimp-paintbrush-tool"));
  g_object_unref (active);
  close_image ();
}
static void standalone_lifetime (void)
{
  for (guint phase = 0; phase < 3; phase++)
    {
      gboolean finalized = FALSE, control_finalized = FALSE;
      GimpCoords coords = { 0 };
      create_image ();
      g_object_set_data_full (G_OBJECT (tool), "lifetime-probe", &finalized, mark_finalized);
      g_object_set_data_full (G_OBJECT (tool->control), "lifetime-probe", &control_finalized, mark_finalized);
      coords.x = 24; coords.y = 36;
      klass->oper_update (tool, &coords, 0, TRUE, shell->display);
      if (phase)
        {
          klass->modifier_key (tool, GDK_SHIFT_MASK, TRUE, GDK_SHIFT_MASK, shell->display);
          klass->button_press (tool, &coords, 0, GDK_SHIFT_MASK, GIMP_BUTTON_PRESS_NORMAL, shell->display);
          if (phase == 2) klass->button_release (tool, &coords, 0, 0, GIMP_BUTTON_RELEASE_NORMAL, shell->display);
        }
      g_object_run_dispose (G_OBJECT (tool));
      g_object_run_dispose (G_OBJECT (tool));
      g_assert_false (finalized);
      g_assert_false (control_finalized);
      g_assert_false (gimp_tool_control_is_active (tool->control));
      g_assert_false (gimp_draw_tool_is_active (GIMP_DRAW_TOOL (tool)));
      g_object_unref (tool); tool = NULL;
      g_assert_true (finalized); g_assert_true (control_finalized);
      if (phase == 2)
        {
          check_point (0, 24, 36);
          g_assert_true (gimp_image_undo (image));
          g_assert_null (gimp_image_get_perspective_guide (image));
          g_assert_true (gimp_image_redo (image)); check_point (0, 24, 36);
        }
      else g_assert_null (gimp_image_get_perspective_guide (image));
      g_object_unref (image); gimp_display_close (shell->display); gimp_test_run_mainloop_until_idle ();
    }
}
static void interrupted_motion (void)
{
  GimpCoords coords = { 0 };
  InterruptState state = { 0 };
  GimpPerspectiveGuide *guide;
  gulong handler;
  create_image ();
  guide = gimp_perspective_guide_new (0);
  gimp_perspective_guide_add_vanish_points (guide, 24, 36);
  gimp_image_set_perspective_guide (image, guide);
  coords.x = 24; coords.y = 36;
  klass->oper_update (tool, &coords, 0, TRUE, shell->display);
  klass->button_press (tool, &coords, 0, 0, GIMP_BUTTON_PRESS_NORMAL, shell->display);
  handler = g_signal_connect (guide, "changed", G_CALLBACK (interrupt_edit), &state);
  coords.x = 40; coords.y = 50;
  klass->motion (tool, &coords, 0, 0, shell->display);
  g_signal_handler_disconnect (guide, handler);
  g_assert_cmpuint (state.calls, ==, 1);
  g_assert_false (gimp_tool_control_is_active (tool->control)); check_point (0, 24, 36);
  g_object_unref (guide); close_image ();
}
static void recursive_cancel (void)
{
  GimpCoords coords = { 0 };
  InterruptState state = { 0 };
  gulong handler;
  create_image ();
  coords.x = 24; coords.y = 36;
  klass->oper_update (tool, &coords, 0, TRUE, shell->display);
  klass->modifier_key (tool, GDK_SHIFT_MASK, TRUE, GDK_SHIFT_MASK, shell->display);
  klass->button_press (tool, &coords, 0, GDK_SHIFT_MASK, GIMP_BUTTON_PRESS_NORMAL, shell->display);
  handler = g_signal_connect (image, "perspective-guide-changed", G_CALLBACK (interrupt_edit), &state);
  klass->button_release (tool, &coords, 0, 0, GIMP_BUTTON_RELEASE_CANCEL, shell->display);
  g_signal_handler_disconnect (image, handler);
  g_assert_cmpuint (state.calls, ==, 1);
  g_assert_null (gimp_image_get_perspective_guide (image));
  g_assert_false (gimp_tool_control_is_active (tool->control)); close_image ();
}
static void guide_finalized_disposes_tool (gpointer data, GObject *old_guide)
{
  *(gboolean *) data = TRUE;
  g_object_run_dispose (G_OBJECT (tool));
}
static void switch_finalizer_disposes_tool (void)
{
  GimpCoords coords = { 0 };
  GimpImage *other;
  gboolean callback = FALSE;
  create_image ();
  coords.x = 24; coords.y = 36;
  klass->oper_update (tool, &coords, 0, TRUE, shell->display);
  klass->modifier_key (tool, GDK_SHIFT_MASK, TRUE, GDK_SHIFT_MASK, shell->display);
  klass->button_press (tool, &coords, 0, GDK_SHIFT_MASK, GIMP_BUTTON_PRESS_NORMAL, shell->display);
  g_object_weak_ref (G_OBJECT (gimp_image_get_perspective_guide (image)), guide_finalized_disposes_tool, &callback);
  other = gimp_image_new (gimp, 256, 256, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  gimp_display_set_image (shell->display, other);
  klass->button_press (tool, &coords, 0, 0, GIMP_BUTTON_PRESS_NORMAL, shell->display);
  g_assert_true (callback);
  g_assert_false (gimp_draw_tool_is_active (GIMP_DRAW_TOOL (tool)));
  g_assert_false (gimp_tool_control_is_active (tool->control));
  g_assert_null (gimp_image_get_perspective_guide (image));
  g_assert_null (gimp_image_get_perspective_guide (other));
  gimp_display_set_image (shell->display, image); g_object_unref (other);
  close_image ();
}
static void release_tool_owner (GObject *emitter, guint *calls)
{
  if ((*calls)++ == 0) { GimpTool *released = tool; tool = NULL; g_object_unref (released); }
}
static void notification_last_owner (void)
{
  GimpCoords coords = { 0 };
  gboolean finalized = FALSE;
  guint calls = 0;
  gulong handler;
  create_image ();
  g_object_set_data_full (G_OBJECT (tool), "lifetime-probe", &finalized, mark_finalized);
  coords.x = 24; coords.y = 36;
  klass->oper_update (tool, &coords, 0, TRUE, shell->display);
  klass->modifier_key (tool, GDK_SHIFT_MASK, TRUE, GDK_SHIFT_MASK, shell->display);
  handler = g_signal_connect (image, "perspective-guide-changed", G_CALLBACK (release_tool_owner), &calls);
  klass->button_press (tool, &coords, 0, GDK_SHIFT_MASK, GIMP_BUTTON_PRESS_NORMAL, shell->display);
  g_signal_handler_disconnect (image, handler);
  g_assert_true (finalized); g_assert_null (tool);
  g_assert_null (gimp_image_get_perspective_guide (image));
  g_object_unref (image); gimp_display_close (shell->display); gimp_test_run_mainloop_until_idle ();
}
static void guide_property_ownership (void)
{
  GimpTool *temporary;
  GimpPerspectiveGuide *guide, *returned = NULL;
  GParamSpec *spec;
  gboolean finalized = FALSE;
  create_image ();
  spec = g_object_class_find_property (G_OBJECT_GET_CLASS (tool), "guide");
  g_assert_nonnull (spec);
  g_assert_true (G_IS_PARAM_SPEC_OBJECT (spec));
  g_assert_cmpuint (G_PARAM_SPEC_VALUE_TYPE (spec), ==, GIMP_TYPE_PERSPECTIVE_GUIDE);
  g_assert_cmpuint (spec->flags & (G_PARAM_READWRITE | G_PARAM_CONSTRUCT), ==, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_assert_false (spec->flags & GIMP_CONFIG_PARAM_SERIALIZE);
  guide = gimp_perspective_guide_new (42);
  g_object_set_data_full (G_OBJECT (guide), "lifetime-probe", &finalized, mark_finalized);
  temporary = g_object_new (GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL, "tool-info", tool->tool_info, "guide", guide, NULL);
  g_object_unref (guide);
  g_assert_false (finalized);
  g_object_get (temporary, "guide", &returned, NULL);
  g_assert_true (returned == guide);
  g_object_run_dispose (G_OBJECT (temporary));
  g_object_run_dispose (G_OBJECT (temporary));
  g_assert_false (finalized);
  g_object_unref (returned); returned = NULL;
  g_assert_true (finalized);
  g_object_get (temporary, "guide", &returned, NULL); g_assert_null (returned);
  g_object_unref (temporary);
  /* Runtime set/get retains a borrowed input without changing image ownership. */
  guide = gimp_perspective_guide_new (43);
  g_object_set (tool, "guide", guide, NULL); g_object_unref (guide);
  g_object_get (tool, "guide", &returned, NULL); g_assert_true (returned == guide);
  g_assert_null (gimp_image_get_perspective_guide (image));
  g_object_set (tool, "guide", NULL, NULL); g_object_unref (returned);
  close_image ();
}
static void property_finalizer_disposes_tool (void)
{
  GimpCoords coords = { 0 };
  GimpPerspectiveGuide *guide;
  gboolean callback = FALSE;
  create_image ();
  guide = gimp_perspective_guide_new (44);
  g_object_weak_ref (G_OBJECT (guide), guide_finalized_disposes_tool, &callback);
  g_object_set (tool, "guide", guide, NULL); g_object_unref (guide);
  klass->oper_update (tool, &coords, 0, TRUE, shell->display);
  g_assert_true (callback);
  g_assert_false (gimp_draw_tool_is_active (GIMP_DRAW_TOOL (tool)));
  g_assert_false (gimp_tool_control_is_active (tool->control));
  g_assert_null (g_list_find (tool->status_displays, shell->display));
  close_image ();
}
static void guide_finalized_replaces_image_model (gpointer data, GObject *old_guide)
{ gimp_image_set_perspective_guide (image, data); }
static void property_finalizer_replaces_model (void)
{
  GimpCoords coords = { 0 };
  GimpPerspectiveGuide *guide, *replacement;
  create_image ();
  guide = gimp_perspective_guide_new (45);
  replacement = gimp_perspective_guide_new (46);
  gimp_perspective_guide_add_vanish_points (replacement, 100, 110);
  g_object_weak_ref (G_OBJECT (guide), guide_finalized_replaces_image_model, replacement);
  g_object_set (tool, "guide", guide, NULL); g_object_unref (guide);
  klass->modifier_key (tool, GDK_SHIFT_MASK, TRUE, GDK_SHIFT_MASK, shell->display);
  coords.x = 24; coords.y = 36;
  klass->button_press (tool, &coords, 0, GDK_SHIFT_MASK, GIMP_BUTTON_PRESS_NORMAL, shell->display);
  g_assert_true (gimp_image_get_perspective_guide (image) == replacement);
  g_assert_false (gimp_tool_control_is_active (tool->control));
  check_point (0, 100, 110);
  g_object_unref (replacement); close_image ();
}
int main(int argc,char **argv)
{
  gint result;g_test_init(&argc,&argv,NULL);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  g_test_add_func("/perspective-ui/add-move-remove-undo",add_move_remove_undo);
  g_test_add_func("/perspective-ui/close-pending-resize",close_with_pending_resize);
  g_test_add_func("/perspective-ui/cancel-hit-transforms",cancel_and_hit_transforms);
  g_test_add_func("/perspective-ui/overlay-extents-replace",overlay_extents_and_replace);
  g_test_add_func("/perspective-ui/overlay-pixels-image-switch",overlay_pixels_and_image_switch);
  g_test_add_func("/perspective-ui/registration-external-replace",registration_and_external_replace);
  g_test_add_func("/perspective-ui/old-toolrc-preserves-groups",old_toolrc_preserves_groups);
  g_test_add_data_func("/perspective-ui/interrupted-add",NULL,interrupted_add);
  g_test_add_data_func("/perspective-ui/interrupted-add-dispose",GINT_TO_POINTER(1),interrupted_add);
  g_test_add_data_func("/perspective-ui/interrupted-remove",NULL,interrupted_remove);
  g_test_add_data_func("/perspective-ui/interrupted-remove-dispose",GINT_TO_POINTER(1),interrupted_remove);
  g_test_add_func("/perspective-ui/proximity-status",proximity_status);
  g_test_add_func("/perspective-ui/registered-type-options",registered_type_options);
  g_test_add_func("/perspective-ui/standalone-lifetime",standalone_lifetime);
  g_test_add_func("/perspective-ui/interrupted-motion",interrupted_motion);
  g_test_add_func("/perspective-ui/recursive-cancel",recursive_cancel);
  g_test_add_func("/perspective-ui/switch-finalizer-disposes-tool",switch_finalizer_disposes_tool);
  g_test_add_func("/perspective-ui/notification-last-owner",notification_last_owner);
  g_test_add_func("/perspective-ui/guide-property-ownership",guide_property_ownership);
  g_test_add_func("/perspective-ui/property-finalizer-disposes-tool",property_finalizer_disposes_tool);
  g_test_add_func("/perspective-ui/property-finalizer-replaces-model",property_finalizer_replaces_model);
  g_application_run(gimp->app,0,NULL);result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
