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
  std::vector<guchar> initial;
  explicit Scene (int size=128) : side(size), initial (size*size*4, 80) {
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
    auto *info = GIMP_TOOL_INFO (gimp_container_get_child_by_name (gimp->tool_info_list, "gimp-painter-smudge-tool"));
    g_assert_nonnull (info);
    tool = GIMP_TOOL (g_object_new (GIMP_TYPE_PAINTER_SMUDGE_TOOL, "tool-info", info, nullptr));
    klass = GIMP_TOOL_GET_CLASS (tool); options = GIMP_PAINT_TOOL_GET_OPTIONS (tool);
    g_object_set (options, "brush-size", 15.0, "use-color-blending", TRUE, "rate", 50.0, nullptr);
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
  bool pending () { return tool && gimp_painter_smudge_tool_is_pending (GIMP_PAINTER_SMUDGE_TOOL (tool)); }
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
static void registration_options () {
  Scene s; auto *info=s.tool->tool_info;
  // Standard modern Smudge retains its old ID and S accelerator; legacy
  // profile imports will explicitly map Painter-origin references separately.
  auto* standard=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,"gimp-smudge-tool"));
  g_assert_nonnull(standard);g_assert_cmpstr(standard->menu_accel,==,"S");
  g_assert_cmpuint(info->paint_info->paint_type,==,GIMP_TYPE_PAINTER_SMUDGE);
  g_assert_true(GIMP_IS_PAINTER_SMUDGE(GIMP_PAINT_TOOL(s.tool)->core));
  g_assert_true(GIMP_PAINT_TOOL(s.tool)->pick_colors);
  g_assert_cmpint(GIMP_COLOR_TOOL(s.tool)->pick_target,==,GIMP_COLOR_PICK_TARGET_FOREGROUND);
  GtkWidget *gui=gimp_tools_get_tool_options_gui(GIMP_TOOL_OPTIONS(s.options));g_assert_nonnull(gui);
  g_object_set(s.options,"rate",27.0,"use-color-blending",TRUE,nullptr);
  auto *copy=G_OBJECT(gimp_config_duplicate(GIMP_CONFIG(s.options)));gdouble rate=0;gboolean erase=FALSE;
  g_object_get(copy,"rate",&rate,"use-color-blending",&erase,nullptr);g_assert_cmpfloat(rate,==,27);g_assert_true(erase);g_object_unref(copy);
}
static void foreground_picker () {
  Scene s;GdkModifierType mask=gimp_get_constrain_behavior_mask();
  s.klass->modifier_key(s.tool,mask,TRUE,mask,s.display);
  g_assert_true(gimp_color_tool_is_enabled(GIMP_COLOR_TOOL(s.tool)));
  s.klass->button_press(s.tool,&s.coords,0,mask,GIMP_BUTTON_PRESS_NORMAL,s.display);
  s.klass->button_release(s.tool,&s.coords,0,mask,GIMP_BUTTON_RELEASE_NORMAL,s.display);
  guchar rgba[4]={};gegl_color_get_pixel(gimp_context_get_foreground(GIMP_CONTEXT(s.options)),babl_format("R'G'B'A u8"),rgba);
  g_assert_cmpint(rgba[0],==,80);g_assert_cmpint(rgba[1],==,80);g_assert_cmpint(rgba[2],==,80);
  g_assert_false(s.pending());g_assert_cmpint(s.depth(),==,0);
  s.klass->modifier_key(s.tool,mask,FALSE,GdkModifierType(0),s.display);
  g_assert_false(gimp_color_tool_is_enabled(GIMP_COLOR_TOOL(s.tool)));
}
static void queued_settings_undo () {
  Scene s; s.press();s.motion(50,32);s.release();
  g_assert_true(s.pending());g_assert(s.pixels()==s.initial);g_assert_false(gimp_tool_control_is_active(s.tool->control));
  s.color("blue");s.coords.x=88;s.coords.y=88;s.press();s.release();s.color("green");
  s.drain();auto final=s.pixels();g_assert_cmpint(s.depth(),==,2);
  auto first=(32*128+32)*4,second=(88*128+88)*4;
  g_assert_cmpint(final[first],==,255);g_assert_cmpint(final[first+2],==,0);
  // Old full-byte Replace coverage is 255*255/65536, not exactly one.
  g_assert_cmpint(final[second],==,1);g_assert_cmpint(final[second+2],==,254);
  g_assert_true(gimp_image_undo(s.image));auto one=s.pixels();g_assert_cmpint(one[first],==,255);g_assert_cmpint(one[second],==,80);
  g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==s.initial);
  g_assert_true(gimp_image_redo(s.image));g_assert_true(gimp_image_redo(s.image));g_assert(s.pixels()==final);
}
static void repeated_press () {
  Scene s;s.press();s.coords.x=88;s.coords.y=88;s.press();s.release();s.drain();g_assert_cmpint(s.depth(),==,2);
}
static void cancel_before_after () {
  Scene s;s.press();s.release(GIMP_BUTTON_RELEASE_CANCEL);s.drain();g_assert(s.pixels()==s.initial);
  s.press();s.await_paint();s.release(GIMP_BUTTON_RELEASE_CANCEL);s.drain();g_assert(s.pixels()==s.initial);g_assert_cmpint(s.depth(),==,0);
  s.press();s.release();s.drain();g_assert_cmpint(s.depth(),==,1);
}
static void external_dirty () {
  Scene s;s.press();s.release();g_signal_emit_by_name(s.image,"dirty",GIMP_DIRTY_IMAGE);s.drain();g_assert(s.pixels()==s.initial);
  s.press();s.await_paint();g_signal_emit_by_name(s.image,"mask-changed");s.drain();g_assert(s.pixels()==s.initial);
}
static void display_image_change () {
  Scene s;s.press();s.release();auto *other=gimp_image_new(gimp,128,128,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  gimp_display_set_image(s.display,other);g_assert_false(s.pending());gimp_test_run_mainloop_until_idle();g_assert(s.pixels()==s.initial);
  gimp_display_set_image(s.display,s.image);g_object_unref(other);
}
static void dispose_queued () {
  Scene s;s.press();s.await_paint();g_object_run_dispose(G_OBJECT(s.tool));g_assert_false(s.pending());gimp_test_run_mainloop_until_idle();g_assert(s.pixels()==s.initial);
}
static void close_on_frozen(GObject *object,GParamSpec*,gpointer data) {
  auto *s=static_cast<Scene*>(data);if(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(object)))g_object_run_dispose(G_OBJECT(s->tool));
}
static void dispose_during_start () {
  Scene s;auto id=g_signal_connect(s.drawable,"notify::frozen",G_CALLBACK(close_on_frozen),&s);s.press();s.release();s.drain();g_signal_handler_disconnect(s.drawable,id);
  g_assert(s.pixels()==s.initial);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.drawable)));
}
struct Reenter { Scene *scene; gulong id; };
static void replace_on_update(GimpDrawable *drawable,gint,gint,gint,gint,gpointer data) {
  auto *call=static_cast<Reenter*>(data);g_signal_handler_disconnect(drawable,call->id);
  auto&s=*call->scene;gimp_tool_control(s.tool,GIMP_TOOL_ACTION_HALT,s.display);s.color("blue");s.coords.x=88;s.coords.y=88;s.press();s.release();
}
static void cancel_then_reentrant_stroke () {
  Scene s;Reenter data{&s,0};data.id=g_signal_connect(s.drawable,"update",G_CALLBACK(replace_on_update),&data);s.press();s.release();s.drain();auto p=s.pixels();
  g_assert_cmpint(p[(32*128+32)*4],==,80);g_assert_cmpint(p[(88*128+88)*4+2],==,254);g_assert_cmpint(s.depth(),==,1);
}
static void owner_loss_on_update(GimpDrawable*,gint,gint,gint,gint,gpointer data) {
  auto *s=static_cast<Scene*>(data);if(s->tool){auto *tool=s->tool;s->tool=nullptr;g_object_unref(tool);}
}
static void last_owner_release () {
  Scene s;auto id=g_signal_connect(s.drawable,"update",G_CALLBACK(owner_loss_on_update),&s);s.press();s.release();
  const gint64 deadline=g_get_monotonic_time()+10000000;while(s.tool && g_get_monotonic_time()<deadline)g_main_context_iteration(nullptr,FALSE);
  g_assert_null(s.tool);g_signal_handler_disconnect(s.drawable,id);gimp_test_run_mainloop_until_idle();g_assert(s.pixels()==s.initial);
}
static void press_on_rollback (GimpDrawable *drawable,gint,gint,gint,gint,gpointer data) {
  auto *call=static_cast<Reenter*>(data);g_signal_handler_disconnect(drawable,call->id);
  auto&s=*call->scene;s.color("blue");s.coords.x=88;s.coords.y=88;s.press();
}
static void rollback_reentrant_input () {
  Scene s;s.press();s.await_paint();Reenter data{&s,0};
  data.id=g_signal_connect(s.drawable,"update",G_CALLBACK(press_on_rollback),&data);
  g_signal_emit_by_name(s.image,"mask-changed");
  g_assert_true(gimp_tool_control_is_active(s.tool->control));s.motion(95,95);s.release();s.drain();
  auto p=s.pixels();g_assert_cmpint(p[(32*128+32)*4],==,80);g_assert_cmpint(p[(88*128+88)*4+2],==,255);g_assert_cmpint(s.depth(),==,1);
}
static void save_and_commit_pending () {
  Scene s;s.press();s.release();g_assert_true(gimp_image_has_pending_paint(s.image));
  gimp_image_saving(s.image);g_assert_true(s.pending());s.drain();g_assert_cmpint(s.depth(),==,1);
  g_assert_false(gimp_image_has_pending_paint(s.image));
  s.coords.x=88;s.coords.y=88;s.press();s.await_paint();
  gimp_tool_control(s.tool,GIMP_TOOL_ACTION_COMMIT,s.display);g_assert_true(s.pending());
  s.drain();g_assert_cmpint(s.depth(),==,2);g_assert_false(gimp_image_has_pending_paint(s.image));
}
static void undo_pending () {
  Scene s;s.press();s.release();s.drain();g_assert_cmpint(s.depth(),==,1);
  s.coords.x=88;s.coords.y=88;s.press();
  const auto before=s.pixels();const gint64 deadline=g_get_monotonic_time()+10000000;
  while(s.pixels()==before && g_get_monotonic_time()<deadline)g_main_context_iteration(nullptr,FALSE);
  g_assert(s.pixels()!=before);g_assert_true(gimp_image_undo(s.image));s.drain();g_assert(s.pixels()==s.initial);
  g_assert_true(gimp_image_redo(s.image));g_assert(s.pixels()==before);
}
static void outline_and_projection () {
  Scene s;auto *draw=GIMP_DRAW_TOOL(s.tool);s.press();
  const gint64 deadline=g_get_monotonic_time()+10000000;
  while(!draw->item && g_get_monotonic_time()<deadline)g_main_context_iteration(nullptr,FALSE);
  g_assert_nonnull(draw->item);auto *before=gimp_canvas_item_get_extents(draw->item);g_assert_nonnull(before);
  auto last=draw->last_draw_time;s.motion(95,95);
  while(draw->last_draw_time==last && g_get_monotonic_time()<deadline)g_main_context_iteration(nullptr,FALSE);
  g_assert_cmpuint(draw->last_draw_time,>,last);auto *after=gimp_canvas_item_get_extents(draw->item);g_assert_nonnull(after);
  g_assert_false(cairo_region_equal(before,after));cairo_region_destroy(before);cairo_region_destroy(after);
  s.await_paint();gimp_test_run_mainloop_until_idle();
  guchar pixel[4]={};auto *projection=GIMP_PICKABLE(gimp_image_get_projection(s.image));
  g_assert_true(gimp_pickable_get_pixel_at(projection,32,32,babl_format("R'G'B'A u8"),pixel));g_assert_cmpint(pixel[0],==,254);
  s.release(GIMP_BUTTON_RELEASE_CANCEL);gimp_test_run_mainloop_until_idle();
  g_assert_true(gimp_pickable_get_pixel_at(projection,32,32,babl_format("R'G'B'A u8"),pixel));g_assert_cmpint(pixel[0],==,80);
}
static GFile *sentinel_file () {
  GFileIOStream *io=nullptr;auto *file=g_file_new_tmp("smudge-pending-XXXXXX.xcf",&io,nullptr);
  g_assert_nonnull(file);g_io_stream_close(G_IO_STREAM(io),nullptr,nullptr);g_object_unref(io);
  g_assert_true(g_file_replace_contents(file,"keep-me",7,nullptr,FALSE,G_FILE_CREATE_NONE,nullptr,nullptr,nullptr));return file;
}
static void assert_sentinel (GFile *file) {
  gchar *bytes=nullptr;gsize length=0;g_assert_true(g_file_load_contents(file,nullptr,&bytes,&length,nullptr,nullptr));
  g_assert_cmpmem(bytes,length,"keep-me",7);g_free(bytes);
}
static GimpPlugInProcedure *save_proc () {
  auto *proc=gimp_pdb_lookup_procedure(gimp->pdb,"gimp-xcf-save");g_assert_true(GIMP_IS_PLUG_IN_PROCEDURE(proc));return GIMP_PLUG_IN_PROCEDURE(proc);
}
static void save_export_preflight () {
  Scene s;GFile *file=sentinel_file();GError *error=nullptr;s.press();s.release();
  for(int kind=0;kind<3;++kind){
    g_assert_cmpint(file_save(gimp,s.image,nullptr,file,save_proc(),GIMP_RUN_NONINTERACTIVE,kind==0,kind==1,kind==2,&error),==,GIMP_PDB_EXECUTION_ERROR);
    g_assert_error(error,G_IO_ERROR,G_IO_ERROR_BUSY);g_clear_error(&error);assert_sentinel(file);g_assert_true(s.pending());g_assert(s.pixels()==s.initial);
  }
  auto *values=gimp_pdb_execute_procedure_by_name(gimp->pdb,GIMP_CONTEXT(s.options),nullptr,&error,"gimp-file-save",GIMP_TYPE_RUN_MODE,GIMP_RUN_NONINTERACTIVE,GIMP_TYPE_IMAGE,s.image,G_TYPE_FILE,file,GIMP_TYPE_EXPORT_OPTIONS,nullptr,G_TYPE_NONE);
  g_assert_nonnull(error);g_clear_error(&error);g_assert_cmpint(g_value_get_enum(gimp_value_array_index(values,0)),!=,GIMP_PDB_SUCCESS);gimp_value_array_unref(values);assert_sentinel(file);
  g_assert_false(xcf_save_recovery_image(gimp,s.image,file,&error));g_assert_nonnull(error);g_clear_error(&error);assert_sentinel(file);
  auto *out=G_OUTPUT_STREAM(g_file_replace(file,nullptr,FALSE,G_FILE_CREATE_NONE,nullptr,&error));g_assert_no_error(error);
  g_assert_false(xcf_save_stream(gimp,s.image,out,file,nullptr,&error));g_assert_error(error,G_IO_ERROR,G_IO_ERROR_BUSY);g_clear_error(&error);g_object_unref(out);assert_sentinel(file);
  s.drain();auto final=s.pixels();g_assert_cmpint(s.depth(),==,1);
  g_assert_cmpint(file_save(gimp,s.image,nullptr,file,save_proc(),GIMP_RUN_NONINTERACTIVE,TRUE,FALSE,FALSE,&error),==,GIMP_PDB_SUCCESS);g_assert_no_error(error);g_assert(s.pixels()==final);
  g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
static void start_from_saving (GimpImage *, gpointer data) { auto *s=static_cast<Scene*>(data);s->press();s->release(); }
static void save_signal_reentry () {
  Scene s;GFile *file=sentinel_file();GError *error=nullptr;
  auto id=g_signal_connect(s.image,"saving",G_CALLBACK(start_from_saving),&s);
  g_assert_cmpint(file_save(gimp,s.image,nullptr,file,save_proc(),GIMP_RUN_NONINTERACTIVE,TRUE,FALSE,FALSE,&error),==,GIMP_PDB_EXECUTION_ERROR);
  g_assert_error(error,G_IO_ERROR,G_IO_ERROR_BUSY);g_clear_error(&error);g_signal_handler_disconnect(s.image,id);
  assert_sentinel(file);g_assert_true(s.pending());s.drain();g_assert_cmpint(s.depth(),==,1);g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
struct SmudgeSaveProgress { GObject parent; Scene *scene; gboolean late, fired, ended; };
struct SmudgeSaveProgressClass { GObjectClass parent; };
static void progress_paint (SmudgeSaveProgress *self) { if(!self->fired){self->fired=TRUE;self->scene->press();self->scene->release();} }
static GimpProgress *progress_start (GimpProgress *progress,gboolean,const gchar*) { auto *self=(SmudgeSaveProgress*)progress;if(!self->late)progress_paint(self);return progress; }
static void progress_text (GimpProgress *progress,const gchar *text) { auto *self=(SmudgeSaveProgress*)progress;if(self->late && g_str_has_prefix(text,"Closing"))progress_paint(self); }
static void progress_end (GimpProgress *progress) { ((SmudgeSaveProgress*)progress)->ended=TRUE; }
static gboolean progress_active (GimpProgress *progress) { return !((SmudgeSaveProgress*)progress)->ended; }
static void progress_iface (GimpProgressInterface *iface) { iface->start=progress_start;iface->set_text=progress_text;iface->end=progress_end;iface->is_active=progress_active; }
GType smudge_save_progress_get_type (void);
G_DEFINE_TYPE_WITH_CODE(SmudgeSaveProgress,smudge_save_progress,G_TYPE_OBJECT,G_IMPLEMENT_INTERFACE(GIMP_TYPE_PROGRESS,progress_iface))
static void smudge_save_progress_class_init (SmudgeSaveProgressClass*) {}
static void smudge_save_progress_init (SmudgeSaveProgress*) {}
static void xcf_progress_reentry () {
  for(int late=0;late<2;++late){Scene s;GFile *file=sentinel_file();GError *error=nullptr;
    auto *progress=(SmudgeSaveProgress*)g_object_new(smudge_save_progress_get_type(),nullptr);progress->scene=&s;progress->late=late;
    auto *values=gimp_pdb_execute_procedure_by_name(gimp->pdb,GIMP_CONTEXT(s.options),GIMP_PROGRESS(progress),&error,"gimp-xcf-save",GIMP_TYPE_RUN_MODE,GIMP_RUN_NONINTERACTIVE,GIMP_TYPE_IMAGE,s.image,G_TYPE_FILE,file,G_TYPE_NONE);
    g_assert_true(progress->fired);g_assert_true(progress->ended);g_assert_nonnull(error);g_clear_error(&error);
    g_assert_cmpint(g_value_get_enum(gimp_value_array_index(values,0)),!=,GIMP_PDB_SUCCESS);gimp_value_array_unref(values);g_object_unref(progress);assert_sentinel(file);
    g_assert_true(s.pending());s.drain();g_assert_cmpint(s.depth(),==,1);g_file_delete(file,nullptr,nullptr);g_object_unref(file);
  }
}
static void target_mutation () {
  { Scene s;s.press();s.await_paint();gimp_item_set_lock_content(GIMP_ITEM(s.drawable),TRUE,FALSE);s.drain();g_assert(s.pixels()==s.initial);g_assert_true(gimp_item_get_lock_content(GIMP_ITEM(s.drawable))); }
  { Scene s;s.press();s.await_paint();
    gimp_item_resize(GIMP_ITEM(s.drawable),GIMP_CONTEXT(s.options),GIMP_FILL_TRANSPARENT,96,96,0,0);s.drain();
    g_assert_cmpint(gimp_item_get_width(GIMP_ITEM(s.drawable)),==,96);g_assert_cmpint(gimp_item_get_height(GIMP_ITEM(s.drawable)),==,96);
    auto p=s.pixels();g_assert_cmpint(p[(32*128+32)*4],==,80);
  }
  { Scene s;s.press();s.await_paint();
    gimp_image_convert_precision(s.image,GIMP_PRECISION_FLOAT_LINEAR,GEGL_DITHER_NONE,GEGL_DITHER_NONE,GEGL_DITHER_NONE,nullptr);s.drain();
    g_assert_cmpint(gimp_image_get_precision(s.image),==,GIMP_PRECISION_FLOAT_LINEAR);auto p=s.pixels();g_assert_cmpint(p[(32*128+32)*4],==,80);
  }
}
static void external_write_preserved () {
  Scene s;s.press();s.await_paint();auto *color=gegl_color_new("blue");
  gegl_buffer_set_color(gimp_drawable_get_buffer(s.drawable),GEGL_RECTANGLE(29,29,7,7),color);g_object_unref(color);
  gimp_drawable_update(s.drawable,29,29,7,7);s.drain();auto p=s.pixels();
  g_assert_cmpint(p[(32*128+32)*4],==,0);g_assert_cmpint(p[(32*128+32)*4+2],==,255);
  g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.drawable)));
}
static void latency_large_and_queue () {
  Scene s(2048);g_object_set(s.options,"brush-size",512.0,nullptr);s.coords.x=512;s.coords.y=512;
  struct rusage before{},after{};getrusage(RUSAGE_SELF,&before);
  gint64 start=g_get_monotonic_time();s.press();gint64 press_us=g_get_monotonic_time()-start;
  for(int n=0;n<20;++n)s.motion(512+n*12,512+n*8);
  start=g_get_monotonic_time();s.release();gint64 release_us=g_get_monotonic_time()-start;
  gint64 maximum=0,total=g_get_monotonic_time();guint iterations=0;
  while(s.pending() && g_get_monotonic_time()-total<30000000){start=g_get_monotonic_time();g_main_context_iteration(nullptr,FALSE);maximum=std::max(maximum,g_get_monotonic_time()-start);++iterations;}
  g_assert_false(s.pending());getrusage(RUSAGE_SELF,&after);g_assert_cmpint(s.depth(),==,1);
  g_test_message("latency image=2048x2048 brush=512 points=21 press_us=%" G_GINT64_FORMAT " release_us=%" G_GINT64_FORMAT " max_main_iteration_us=%" G_GINT64_FORMAT " elapsed_us=%" G_GINT64_FORMAT " iterations=%u peak_rss_before_kib=%ld peak_rss_after_kib=%ld; includes native snapshot/publication/GTK, no total bound claimed",press_us,release_us,maximum,g_get_monotonic_time()-total,iterations,before.ru_maxrss,after.ru_maxrss);
  start=g_get_monotonic_time();s.press();for(int n=0;n<10000;++n)s.motion(512+(n%20)*12,512+(n%20)*8);gint64 admission=g_get_monotonic_time()-start;getrusage(RUSAGE_SELF,&after);
  start=g_get_monotonic_time();s.release(GIMP_BUTTON_RELEASE_CANCEL);gint64 cancel=g_get_monotonic_time()-start;g_assert_false(s.pending());
  g_test_message("raw_input_queue points=10001 admission_us=%" G_GINT64_FORMAT " cancellation_cleanup_us=%" G_GINT64_FORMAT " peak_rss_kib=%ld; no arbitrary queue cap, bounded-memory admission remains open",admission,cancel,after.ru_maxrss);
}
struct CloseSave { Scene *scene; GFile *file; gulong id; gboolean called; };
static void save_during_close (GimpDrawable *drawable,gint,gint,gint,gint,gpointer data) {
  auto *call=static_cast<CloseSave*>(data);g_signal_handler_disconnect(drawable,call->id);call->called=TRUE;
  auto&s=*call->scene;g_object_run_dispose(G_OBJECT(s.tool));
  g_assert_true(gimp_image_has_pending_paint(s.image));GError *error=nullptr;
  g_assert_false(xcf_save_recovery_image(gimp,s.image,call->file,&error));g_assert_nonnull(error);g_clear_error(&error);assert_sentinel(call->file);
}
static void save_reentrant_close () {
  Scene s;GFile *file=sentinel_file();CloseSave call{&s,file,0,FALSE};
  call.id=g_signal_connect(s.drawable,"update",G_CALLBACK(save_during_close),&call);s.press();s.release();s.drain();
  g_assert_true(call.called);g_assert_false(gimp_image_has_pending_paint(s.image));g_assert(s.pixels()==s.initial);assert_sentinel(file);
  g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
static void invalid_coordinates () {
  Scene s;s.press();s.await_paint();s.motion(INFINITY,32);s.drain();g_assert(s.pixels()==s.initial);
  s.coords.x=32;s.coords.pressure=NAN;
  s.klass->button_press(s.tool,&s.coords,0,GdkModifierType(0),GIMP_BUTTON_PRESS_NORMAL,s.display);
  g_assert_false(s.pending());g_assert_cmpint(s.depth(),==,0);
}
static void direct_export_preflight () {
  Scene s;s.press();s.release();GFile *file=sentinel_file();auto *executable=g_file_new_for_path("/nonexistent-fill-test-exporter");
  auto *proc=gimp_plug_in_procedure_new(GIMP_PDB_PROC_TYPE_PLUGIN,executable);g_object_unref(executable);
  gimp_object_set_static_name(GIMP_OBJECT(proc),"fill-test-export");
  gimp_plug_in_procedure_set_file_proc(GIMP_PLUG_IN_PROCEDURE(proc),"test","",nullptr);
  gimp_procedure_add_argument(proc,g_param_spec_enum("run-mode","mode","mode",GIMP_TYPE_RUN_MODE,GIMP_RUN_NONINTERACTIVE,G_PARAM_READWRITE));
  gimp_procedure_add_argument(proc,gimp_param_spec_image("image","image","image",FALSE,G_PARAM_READWRITE));
  gimp_procedure_add_argument(proc,g_param_spec_object("file","file","file",G_TYPE_FILE,G_PARAM_READWRITE));
  auto *args=gimp_procedure_get_arguments(proc);g_value_set_enum(gimp_value_array_index(args,0),GIMP_RUN_NONINTERACTIVE);
  g_value_set_object(gimp_value_array_index(args,1),s.image);g_value_set_object(gimp_value_array_index(args,2),file);
  GError *error=nullptr;auto *result=gimp_procedure_execute(proc,gimp,GIMP_CONTEXT(s.options),nullptr,args,&error);
  g_assert_error(error,G_IO_ERROR,G_IO_ERROR_BUSY);g_clear_error(&error);g_assert_cmpint(g_value_get_enum(gimp_value_array_index(result,0)),!=,GIMP_PDB_SUCCESS);
  gimp_value_array_unref(result);gimp_value_array_unref(args);g_object_unref(proc);assert_sentinel(file);g_assert_true(s.pending());s.drain();g_assert_cmpint(s.depth(),==,1);
  g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
static void latency_sample () {
  Scene s;g_object_set(s.options,"brush-size",120.0,nullptr);
  gint64 start=g_get_monotonic_time();s.press();gint64 press_us=g_get_monotonic_time()-start;
  for(int n=0;n<20;++n)s.motion(32+n*3,32+n*2);
  start=g_get_monotonic_time();s.release();gint64 release_us=g_get_monotonic_time()-start;
  gint64 max_dispatch=0,total_start=g_get_monotonic_time();guint count=0;
  while(s.pending() && g_get_monotonic_time()-total_start<10000000){start=g_get_monotonic_time();g_main_context_iteration(nullptr,FALSE);max_dispatch=std::max(max_dispatch,g_get_monotonic_time()-start);++count;}
  g_assert_false(s.pending());g_assert_cmpint(s.depth(),==,1);
  g_test_message("latency image=128x128 brush=120 points=21 press_us=%" G_GINT64_FORMAT " release_us=%" G_GINT64_FORMAT " max_main_iteration_us=%" G_GINT64_FORMAT " elapsed_us=%" G_GINT64_FORMAT " iterations=%u; no total latency/admission bound claimed",press_us,release_us,max_dispatch,g_get_monotonic_time()-total_start,count);
}
static void long_event_yields () {
  Scene s;gimp_brush_set_spacing(gimp_context_get_brush(GIMP_CONTEXT(s.options)),1);
  g_object_set(s.options,"brush-size",15.0,nullptr);
  g_object_set(gimp_dynamics_get_output(gimp_context_get_dynamics(GIMP_CONTEXT(s.options)),GIMP_DYNAMICS_OUTPUT_SPACING),"use-pressure",TRUE,nullptr);
  s.press();s.await_paint();gint64 start=g_get_monotonic_time();s.motion(1e9,32);s.release();gint64 admission=g_get_monotonic_time()-start;
  gint64 maximum=0;for(int n=0;n<100;++n){start=g_get_monotonic_time();g_main_context_iteration(nullptr,FALSE);maximum=std::max(maximum,g_get_monotonic_time()-start);}
  g_assert_true(s.pending());gimp_tool_control(s.tool,GIMP_TOOL_ACTION_HALT,s.display);s.drain();g_assert(s.pixels()==s.initial);g_assert_cmpint(s.depth(),==,0);
  g_test_message("billion-pixel event admission_us=%" G_GINT64_FORMAT " max_100_iteration_us=%" G_GINT64_FORMAT "; still pending before explicit cancel, no segment expansion loop",admission,maximum);
}
int main(int argc,char**argv) {
  g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  g_test_add_func("/smudge-ui/long-event-yields",long_event_yields);
  g_test_add_func("/smudge-ui/registration-options",registration_options);
  g_test_add_func("/smudge-ui/foreground-picker",foreground_picker);
  g_test_add_func("/smudge-ui/queued-settings-undo",queued_settings_undo);
  g_test_add_func("/smudge-ui/repeated-press",repeated_press);
  g_test_add_func("/smudge-ui/cancel-before-after",cancel_before_after);
  g_test_add_func("/smudge-ui/external-dirty",external_dirty);
  g_test_add_func("/smudge-ui/display-image-change",display_image_change);
  g_test_add_func("/smudge-ui/dispose-queued",dispose_queued);
  g_test_add_func("/smudge-ui/dispose-during-start",dispose_during_start);
  g_test_add_func("/smudge-ui/cancel-reentrant-stroke",cancel_then_reentrant_stroke);
  g_test_add_func("/smudge-ui/last-owner-release",last_owner_release);
  g_test_add_func("/smudge-ui/rollback-reentrant-input",rollback_reentrant_input);
  g_test_add_func("/smudge-ui/save-commit-pending",save_and_commit_pending);
  g_test_add_func("/smudge-ui/undo-pending",undo_pending);
  g_test_add_func("/smudge-ui/outline-projection",outline_and_projection);
  g_test_add_func("/smudge-ui/save-export-preflight",save_export_preflight);
  g_test_add_func("/smudge-ui/save-signal-reentry",save_signal_reentry);
  g_test_add_func("/smudge-ui/xcf-progress-reentry",xcf_progress_reentry);
  g_test_add_func("/smudge-ui/target-mutation",target_mutation);
  g_test_add_func("/smudge-ui/external-write-preserved",external_write_preserved);
  g_test_add_func("/smudge-ui/save-reentrant-close",save_reentrant_close);
  g_test_add_func("/smudge-ui/invalid-coordinates",invalid_coordinates);
  g_test_add_func("/smudge-ui/direct-export-preflight",direct_export_preflight);
  g_test_add_func("/smudge-ui/latency-sample",latency_sample);
  g_test_add_func("/smudge-ui/latency-large-queue",latency_large_and_queue);
  g_application_run(gimp->app,0,nullptr);gint result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
