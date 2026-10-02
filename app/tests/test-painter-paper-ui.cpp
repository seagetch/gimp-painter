/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <sys/resource.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "display/display-types.h"
#include "tools/tools-types.h"
#include "core/gimp.h"
#include "core/gimpbrush.h"
#include "core/gimpbrush-private.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpdynamics.h"
#include "core/gimppattern.h"
#include "core/gimppatternclipboard.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpimage-convert-precision.h"
#include "core/gimplayer.h"
#include "core/gimptempbuf.h"
#include "core/gimptoolinfo.h"
#include "core/gimppaintinfo.h"
#include "core/gimpundostack.h"
#include "display/gimpdisplay.h"
#include "display/gimpcanvasitem.h"
#include "core/gimpprojection.h"
#include "core/gimppickable.h"
#include "core/gimpprogress.h"
#include "core/gimpparamspecs.h"
#include "pdb/gimppdb.h"
#include "plug-in/gimppluginprocedure.h"
#include "file/file-save.h"
#include "xcf/xcf.h"
#include "display/gimpdisplayshell.h"
#include "paint/gimppaintersmudge.h"
#include "tools/gimp-tools.h"
#include "widgets/gimpwidgets-utils.h"
#include "widgets/gimpaction.h"
#include "widgets/gimpactiongroup.h"
#include "widgets/gimpuimanager.h"
#include "tools/gimppaintersmudgetool.h"
#include "tools/gimpfillbrushtool.h"
#include "tools/gimppaintbrushtool.h"
#include "tools/gimppaintoptions-gui.h"
#include "tools/gimptoolcontrol.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
}
static Gimp *gimp;
struct Scene {
  GimpDisplay *display;
  GimpImage *image;
  GimpDrawable *drawable;
  GimpTool *tool;
  GimpToolClass *klass;
  GimpPaintOptions *options;
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  int side;
  bool fill;
  std::vector<guchar> initial;
  explicit Scene (bool is_fill=false, int size=128) : side(size), fill(is_fill), initial (size*size*4, 80) {
    gimp_set_focused_once (gimp);
    gimp_test_utils_create_image (gimp, side, side);
    gimp_test_run_mainloop_until_idle ();
    display = GIMP_DISPLAY (gimp_get_display_iter (gimp)->data);
    image = gimp_display_get_image (display);
    auto *list = gimp_image_get_selected_drawables (image); g_assert_nonnull (list);
    drawable = GIMP_DRAWABLE (list->data); g_object_ref (drawable); g_list_free (list);
    for (gsize n=3; n<initial.size (); n+=4) initial[n]=255;
    gegl_buffer_set (gimp_drawable_get_buffer (drawable), GEGL_RECTANGLE (0,0,side,side), 0,
                     babl_format ("R'G'B'A u8"), initial.data (), GEGL_AUTO_ROWSTRIDE);
    auto *info = GIMP_TOOL_INFO (gimp_container_get_child_by_name (gimp->tool_info_list, fill ? "gimp-bucket-fill-brush-tool" : "gimp-painter-smudge-tool"));
    g_assert_nonnull (info);
    tool = GIMP_TOOL (g_object_new (fill ? GIMP_TYPE_FILL_BRUSH_TOOL : GIMP_TYPE_PAINTER_SMUDGE_TOOL, "tool-info", info, nullptr));
    klass = GIMP_TOOL_GET_CLASS (tool); options = GIMP_PAINT_TOOL_GET_OPTIONS (tool);
    g_object_set (options, "brush-size", 15.0, "rate", 50.0, nullptr);
    if (!fill) g_object_set(options,"use-color-blending",TRUE,nullptr);
    auto *brush = GIMP_BRUSH (g_object_new (GIMP_TYPE_BRUSH, "name", "smudge-ui-brush", nullptr));
    brush->priv->mask = gimp_temp_buf_new (15,15,babl_format ("Y u8"));
    std::memset (gimp_temp_buf_get_data (brush->priv->mask), 255, 225);
    gimp_context_set_brush (GIMP_CONTEXT (options), brush); g_object_unref (brush);
    auto *dynamics = GIMP_DYNAMICS (g_object_new (GIMP_TYPE_DYNAMICS, "name", "smudge-ui-dynamics", nullptr));
    gimp_context_set_dynamics (GIMP_CONTEXT (options), dynamics); g_object_unref (dynamics);
    color ("red"); gimp_context_set_opacity (GIMP_CONTEXT (options), 1);
    gimp_context_set_paint_mode (GIMP_CONTEXT (options), GIMP_LAYER_MODE_PAINTER_NORMAL);
    coords.x = 32; coords.y = 32; coords.pressure = 1;
    gimp_image_undo_free (image);
  }
  ~Scene () {
    if (tool) { gimp_tool_control (tool, GIMP_TOOL_ACTION_HALT, display); g_object_unref (tool); }
    g_object_unref (drawable); g_object_unref (image);
    gimp_display_close (display); gimp_test_run_mainloop_until_idle ();
  }
  void color (const char *name) { auto *c=gegl_color_new (name); gimp_context_set_foreground (GIMP_CONTEXT (options), c); g_object_unref(c); }
  void press () { klass->oper_update (tool,&coords,GdkModifierType (0),TRUE,display); klass->button_press (tool,&coords,0,GdkModifierType (0),GIMP_BUTTON_PRESS_NORMAL,display); }
  void release (GimpButtonReleaseType type=GIMP_BUTTON_RELEASE_NORMAL) { klass->button_release (tool,&coords,0,GdkModifierType (0),type,display); }
  void motion (double x,double y) { coords.x=x;coords.y=y;klass->motion (tool,&coords,0,GDK_BUTTON1_MASK,display); }
  bool pending () { return tool && (fill ? gimp_fill_brush_tool_is_pending(GIMP_FILL_BRUSH_TOOL(tool)) : gimp_painter_smudge_tool_is_pending (GIMP_PAINTER_SMUDGE_TOOL (tool))); }
  void drain () {
    const gint64 deadline=g_get_monotonic_time ()+10000000;
    while (pending () && g_get_monotonic_time ()<deadline) g_main_context_iteration (nullptr,FALSE);
    g_assert_false (pending ());
    gimp_test_run_mainloop_until_idle ();
  }
  std::vector<guchar> pixels () { std::vector<guchar> p(initial.size());gegl_buffer_get(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,side,side),1,babl_format("R'G'B'A u8"),p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return p; }
  void await_paint () {
    const gint64 deadline=g_get_monotonic_time()+10000000;
    while (pixels()==initial && g_get_monotonic_time()<deadline) g_main_context_iteration(nullptr,FALSE);
    g_assert (pixels()!=initial);
  }
  gint depth () { return gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)); }
};

static GimpPattern *paper (unsigned char value, bool clipboard=false) {
  auto *pattern=clipboard?GIMP_PATTERN(gimp_pattern_clipboard_new(gimp)):GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","queued paper",nullptr));
  g_clear_pointer(&pattern->mask,gimp_temp_buf_unref);pattern->mask=gimp_temp_buf_new(3,5,babl_format("Y u8"));
  std::memset(gimp_temp_buf_get_data(pattern->mask),value,15);return pattern;
}
static GtkWidget *texture_toggle(GtkWidget *widget) {
  if(GTK_IS_TOGGLE_BUTTON(widget)){
    auto *name=static_cast<const char*>(g_object_get_data(G_OBJECT(widget),"gimp-widget-property-name"));
    if(name && !std::strcmp(name,"use-texture"))return widget;
  }
  if(GTK_IS_FRAME(widget)){auto *label=gtk_frame_get_label_widget(GTK_FRAME(widget));if(label){auto *found=texture_toggle(label);if(found)return found;}}
  if(GTK_IS_CONTAINER(widget)){
    GList *children=gtk_container_get_children(GTK_CONTAINER(widget));GtkWidget *found=nullptr;
    for(auto *iter=children;iter&&!found;iter=iter->next)found=texture_toggle(GTK_WIDGET(iter->data));
    g_list_free(children);return found;
  }
  return nullptr;
}
static void options_controls () {
  const char *ids[]={"gimp-paintbrush-tool","gimp-painter-smudge-tool","gimp-bucket-fill-brush-tool"};
  for(const auto *id:ids){
    auto *info=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,id));g_assert_nonnull(info);
    auto *options=GIMP_PAINT_OPTIONS(info->tool_options);auto *gui=gimp_tools_get_tool_options_gui(GIMP_TOOL_OPTIONS(options));g_assert_nonnull(gui);
    auto *toggle=texture_toggle(gui);g_assert_nonnull(toggle);g_object_set(options,"use-texture",FALSE,nullptr);g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),TRUE);g_assert_true(options->use_texture);
    auto *copy=GIMP_PAINT_OPTIONS(gimp_config_duplicate(GIMP_CONFIG(options)));g_assert_true(copy->use_texture);g_object_unref(copy);
    g_object_set(options,"use-texture",FALSE,nullptr);g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)));
  }
}
static void queued_snapshot(bool fill, bool clipboard=false){
  Scene s(fill);auto *pattern=paper(255,clipboard);gimp_context_set_pattern(GIMP_CONTEXT(s.options),pattern);g_object_set(s.options,"use-texture",TRUE,nullptr);
  s.press();s.motion(50,32);s.release();g_assert_true(s.pending());
  std::memset(gimp_temp_buf_get_data(pattern->mask),0,15);gimp_data_dirty(GIMP_DATA(pattern));
  s.drain();auto painted=s.pixels();g_assert(painted!=s.initial);g_assert_cmpint(s.depth(),==,1);
  g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==s.initial);
  s.coords.x=32;s.coords.y=32;s.press();s.motion(50,32);s.release();s.drain();g_assert(s.pixels()==s.initial);
  g_object_unref(pattern);
}
static void smudge_snapshot(){queued_snapshot(false);}
static void fill_snapshot(){queued_snapshot(true);}
static void smudge_clipboard_snapshot(){queued_snapshot(false,true);}
static void fill_clipboard_snapshot(){queued_snapshot(true,true);}
static void paper_cancel(bool fill){
  Scene s(fill);auto *pattern=paper(fill?255:128);gimp_context_set_pattern(GIMP_CONTEXT(s.options),pattern);g_object_set(s.options,"use-texture",TRUE,nullptr);g_object_unref(pattern);
  s.press();s.motion(50,32);s.await_paint();gimp_tool_control(s.tool,GIMP_TOOL_ACTION_HALT,s.display);s.drain();g_assert(s.pixels()==s.initial);g_assert_cmpint(s.depth(),==,0);
  s.coords.x=32;s.coords.y=32;s.press();s.motion(50,32);s.release();s.drain();g_assert(s.pixels()!=s.initial);g_assert_cmpint(s.depth(),==,1);
}
static void smudge_cancel(){paper_cancel(false);}
static void fill_cancel(){paper_cancel(true);}
int main(int argc,char **argv){
  g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  g_test_add_func("/paper-ui/three-options-controls",options_controls);
  g_test_add_func("/paper-ui/smudge-pattern-snapshot",smudge_snapshot);
  g_test_add_func("/paper-ui/fill-pattern-snapshot",fill_snapshot);
  g_test_add_func("/paper-ui/smudge-cancel-reuse",smudge_cancel);
  g_test_add_func("/paper-ui/fill-cancel-reuse",fill_cancel);
  g_test_add_func("/paper-ui/smudge-clipboard-snapshot",smudge_clipboard_snapshot);
  g_test_add_func("/paper-ui/fill-clipboard-snapshot",fill_clipboard_snapshot);
  g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
