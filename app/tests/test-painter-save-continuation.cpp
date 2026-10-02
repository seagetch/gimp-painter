/* SPDX-License-Identifier: GPL-3.0-or-later
 * Reuse the established real-GTK Fill fixture without duplicating its setup. */
int fill_interpolation_ui_main(int argc, char **argv);
#define main fill_interpolation_ui_main
#include "test-painter-fill-brush-ui.cpp"
#undef main
extern "C" {
#include "dialogs/file-save-deferred.h"
#include "actions/actions-types.h"
#include "actions/file-commands.h"
#include "tools/tool_manager.h"
#include "core/gimp-gui.h"
#include "dialogs/file-save-dialog.h"
#include "widgets/gimpfiledialog.h"
#include "widgets/gimpsavedialog.h"
}
struct CompletionState {bool done=false,success=false;int destroyed=0;};
static void completed(gboolean success,gpointer data){auto*s=static_cast<CompletionState*>(data);g_assert_false(s->done);s->done=true;s->success=success;}
static void completion_destroy(gpointer data){++static_cast<CompletionState*>(data)->destroyed;}
static void wait_completed(CompletionState&state){const auto deadline=g_get_monotonic_time()+10000000;while(!state.destroyed&&g_get_monotonic_time()<deadline){g_main_context_iteration(nullptr,FALSE);g_usleep(1000);}g_assert_cmpint(state.destroyed,==,1);}
static void start_save(Scene&s,GObject*owner,GFile*file,CompletionState&state,int kind=0){file_save_dialog_save_image_async(owner,nullptr,gimp,s.image,file,save_proc(),GIMP_RUN_NONINTERACTIVE,kind==0,kind==1,kind==2,FALSE,FALSE,completed,&state,completion_destroy);}
static void assert_saved_pixels(GFile*file,const std::vector<guchar>&expected,int side){
 GError*error=nullptr;auto*stream=g_file_read(file,nullptr,&error);g_assert_no_error(error);g_assert_nonnull(stream);
 auto*image=xcf_load_stream(gimp,G_INPUT_STREAM(stream),file,nullptr,&error);g_assert_no_error(error);g_assert_nonnull(image);g_object_unref(stream);
 auto*layers=gimp_image_get_layer_list(image);g_assert_nonnull(layers);auto*d=GIMP_DRAWABLE(layers->data);std::vector<guchar>pixels(expected.size());
 gegl_buffer_get(gimp_drawable_get_buffer(d),GEGL_RECTANGLE(0,0,side,side),1,babl_format("R'G'B'A u8"),pixels.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert(pixels==expected);g_list_free(layers);g_object_unref(image);
}
static void deferred_save_export(){
 for(int kind=0;kind<3;++kind){Scene s;auto*file=sentinel_file();CompletionState state;
  if(kind==1)gimp_image_set_imported_file(s.image,file);
  s.press();s.motion(50,32);s.release();s.color("blue");s.coords.x=88;s.coords.y=88;s.press();s.release();
  start_save(s,G_OBJECT(s.display),file,state,kind);g_assert_false(state.done);g_assert_cmpint(state.destroyed,==,0);assert_sentinel(file);g_assert_true(s.pending());
  s.drain();auto expected=s.pixels();wait_completed(state);g_assert_true(state.done);g_assert_true(state.success);g_assert_cmpint(s.depth(),==,2);assert_saved_pixels(file,expected,s.side);
  g_assert_true(gimp_image_undo(s.image));g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==s.initial);
  g_file_delete(file,nullptr,nullptr);g_object_unref(file);
 }
}
static void deferred_owner_destroy(){
 Scene s;auto*file=sentinel_file();CompletionState state;auto*window=gtk_window_new(GTK_WINDOW_TOPLEVEL);g_object_ref_sink(window);s.press();s.release();
 start_save(s,G_OBJECT(window),file,state);gtk_widget_destroy(window);g_assert_cmpint(state.destroyed,==,1);g_assert_false(state.done);g_assert_true(s.pending());s.drain();g_assert(s.pixels()!=s.initial);assert_sentinel(file);g_object_unref(window);g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
static void deferred_display_change(){
 Scene s;auto*file=sentinel_file();CompletionState state;s.press();s.release();start_save(s,G_OBJECT(s.display),file,state);
 auto*other=gimp_image_new(gimp,128,128,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);gimp_display_set_image(s.display,other);g_assert_cmpint(state.destroyed,==,1);g_assert_false(state.done);assert_sentinel(file);
 gimp_display_set_image(s.display,s.image);g_object_unref(other);g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
static gboolean release_wait_owner(GimpImage*,gpointer data){auto**owner=static_cast<GObject**>(data);g_clear_object(owner);return FALSE;}
static void deferred_query_owner_loss(){
 Scene s;auto*file=sentinel_file();CompletionState state;auto*owner=G_OBJECT(g_object_new(G_TYPE_OBJECT,nullptr));
 auto id=g_signal_connect(s.image,"query-pending-paint",G_CALLBACK(release_wait_owner),&owner);s.press();s.release();start_save(s,owner,file,state);g_signal_handler_disconnect(s.image,id);
 g_assert_null(owner);g_assert_cmpint(state.destroyed,==,1);g_assert_false(state.done);assert_sentinel(file);s.drain();g_assert_cmpint(s.depth(),==,1);g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
struct StartOnce {Scene*scene;bool fired=false;};
static void save_starts_once(GimpImage*,gpointer data){auto*s=static_cast<StartOnce*>(data);if(!s->fired){s->fired=true;s->scene->press();s->scene->release();}}
static void deferred_saving_reentry(){
 Scene s;auto*file=sentinel_file();CompletionState state;StartOnce once{&s};auto id=g_signal_connect(s.image,"saving",G_CALLBACK(save_starts_once),&once);
 start_save(s,G_OBJECT(s.display),file,state);g_assert_true(once.fired);g_assert_false(state.done);assert_sentinel(file);s.drain();auto expected=s.pixels();wait_completed(state);g_assert_true(state.success);assert_saved_pixels(file,expected,s.side);g_signal_handler_disconnect(s.image,id);g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
static void pending_clean_save_action(){
 Scene s;auto*file=sentinel_file();gimp_image_set_file(s.image,file);gimp_image_set_save_proc(s.image,save_proc());gimp_image_clean_all(s.image);s.press();s.release();
 auto*mode=g_variant_ref_sink(g_variant_new_int32(GIMP_SAVE_MODE_SAVE));file_save_cmd_callback(nullptr,mode,s.display);g_variant_unref(mode);assert_sentinel(file);g_assert_true(s.pending());s.drain();auto expected=s.pixels();
 const auto deadline=g_get_monotonic_time()+10000000;while(gimp_image_is_dirty(s.image)&&g_get_monotonic_time()<deadline){g_main_context_iteration(nullptr,FALSE);g_usleep(1000);}g_assert_false(gimp_image_is_dirty(s.image));g_assert_cmpint(s.depth(),==,1);assert_saved_pixels(file,expected,s.side);g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
struct ReplaceSaveFile {GFile*other;bool fired=false;};
static gboolean replace_save_file(GimpImage*image,gpointer data){auto*replace=static_cast<ReplaceSaveFile*>(data);if(!replace->fired){replace->fired=true;gimp_image_set_file(image,replace->other);}return FALSE;}
static void save_query_replaces_file(){
 Scene s;auto*file=sentinel_file();auto*other=sentinel_file();auto*path=g_file_get_path(file);auto*expected_file=g_file_new_for_path(path);g_free(path);
 gimp_image_set_file(s.image,file);gimp_image_set_save_proc(s.image,save_proc());g_object_unref(file); // image is the only owner before command admission
 ReplaceSaveFile replace{other};auto id=g_signal_connect(s.image,"query-pending-paint",G_CALLBACK(replace_save_file),&replace);s.press();s.release();
 auto*mode=g_variant_ref_sink(g_variant_new_int32(GIMP_SAVE_MODE_SAVE));file_save_cmd_callback(nullptr,mode,s.display);g_variant_unref(mode);g_assert_true(replace.fired);assert_sentinel(expected_file);assert_sentinel(other);s.drain();auto expected=s.pixels();
 const auto deadline=g_get_monotonic_time()+10000000;while(gimp_image_is_dirty(s.image)&&g_get_monotonic_time()<deadline){g_main_context_iteration(nullptr,FALSE);g_usleep(1000);}g_assert_false(gimp_image_is_dirty(s.image));assert_saved_pixels(expected_file,expected,s.side);assert_sentinel(other);
 g_signal_handler_disconnect(s.image,id);g_file_delete(expected_file,nullptr,nullptr);g_file_delete(other,nullptr,nullptr);g_object_unref(expected_file);g_object_unref(other);
}
static void active_tool_commit(){
 Scene s;auto*file=sentinel_file();CompletionState state;
 gimp_context_set_tool(gimp_get_user_context(gimp),s.tool->tool_info);
 auto*active=tool_manager_get_active(gimp);g_assert_true(GIMP_IS_FILL_BRUSH_TOOL(active));g_object_ref(active);g_object_unref(s.tool);s.tool=active;s.klass=GIMP_TOOL_GET_CLASS(active);s.options=GIMP_PAINT_TOOL_GET_OPTIONS(active);
 s.press();s.motion(50,32);g_assert_true(gimp_tool_control_is_active(s.tool->control));
 start_save(s,G_OBJECT(s.display),file,state);g_assert_false(state.done);g_assert_false(gimp_tool_control_is_active(s.tool->control));assert_sentinel(file);
 s.drain();auto expected=s.pixels();wait_completed(state);g_assert_true(state.success);g_assert_cmpint(s.depth(),==,1);assert_saved_pixels(file,expected,s.side);g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
static void dialog_destroyed(GtkWidget*,gpointer data){*static_cast<bool*>(data)=true;}
static gboolean held_pending(GimpImage*,gpointer data){return *static_cast<bool*>(data);}
static void wait_dialog_admission(GtkWidget*dialog,bool&destroyed){const auto deadline=g_get_monotonic_time()+10000000;while(!destroyed&&!GIMP_FILE_DIALOG(dialog)->busy&&g_get_monotonic_time()<deadline){g_main_context_iteration(nullptr,FALSE);g_usleep(1000);}g_assert_true(destroyed||GIMP_FILE_DIALOG(dialog)->busy);}
static void save_dialog_continuation(){
 for(bool cancel:{false,true}){Scene s;auto*file=sentinel_file();auto*other=g_file_new_for_path("/tmp/fill-save-other.xcf");GError*error=nullptr;g_assert_true(g_file_replace_contents(other,"other sentinel",14,nullptr,FALSE,G_FILE_CREATE_NONE,nullptr,nullptr,&error));g_assert_no_error(error);
  gimp_image_set_file(s.image,file);auto*dialog=file_save_dialog_new(gimp,FALSE);g_object_ref_sink(dialog);gimp_save_dialog_set_image(GIMP_SAVE_DIALOG(dialog),s.image,FALSE,FALSE,GIMP_OBJECT(s.display));gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),FALSE);g_assert_true(gtk_file_chooser_set_file(GTK_FILE_CHOOSER(dialog),file,&error));g_assert_no_error(error);gtk_widget_show(dialog);gimp_test_run_mainloop_until_idle();
  bool destroyed=false,held=true;auto pending_id=g_signal_connect(s.image,"query-pending-paint",G_CALLBACK(held_pending),&held);g_signal_connect(dialog,"destroy",G_CALLBACK(dialog_destroyed),&destroyed);s.press();s.release();gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);wait_dialog_admission(dialog,destroyed);g_assert_true(GIMP_FILE_DIALOG(dialog)->busy);g_assert_false(destroyed);assert_sentinel(file);
  if(cancel){gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);g_assert_true(destroyed);held=false;s.drain();assert_sentinel(file);g_assert(s.pixels()!=s.initial);}
  else{g_assert_true(gtk_file_chooser_set_file(GTK_FILE_CHOOSER(dialog),other,&error));g_assert_no_error(error);held=false;s.drain();auto expected=s.pixels();const auto deadline=g_get_monotonic_time()+10000000;while(!destroyed&&g_get_monotonic_time()<deadline){g_main_context_iteration(nullptr,FALSE);g_usleep(1000);}g_assert_true(destroyed);assert_saved_pixels(file,expected,s.side);gchar*bytes=nullptr;gsize length=0;g_assert_true(g_file_load_contents(other,nullptr,&bytes,&length,nullptr,&error));g_assert_no_error(error);g_assert_cmpuint(length,==,14);g_assert_cmpmem(bytes,length,"other sentinel",14);g_free(bytes);}
  g_signal_handler_disconnect(s.image,pending_id);g_object_unref(dialog);g_file_delete(file,nullptr,nullptr);g_file_delete(other,nullptr,nullptr);g_object_unref(file);g_object_unref(other);
 }
}
struct DeferredProgress {GObject parent;Scene*scene;gboolean fired,active;};
struct DeferredProgressClass {GObjectClass parent;};
GType deferred_progress_get_type(void);
static GimpProgress* deferred_progress_start(GimpProgress*p,gboolean,const gchar*){((DeferredProgress*)p)->active=TRUE;return p;}
static void deferred_progress_text(GimpProgress*p,const gchar*text){auto*self=(DeferredProgress*)p;if(!self->fired&&g_str_has_prefix(text,"Closing")){self->fired=TRUE;self->scene->press();self->scene->release();}}
static void deferred_progress_end(GimpProgress*p){((DeferredProgress*)p)->active=FALSE;}
static gboolean deferred_progress_active(GimpProgress*p){return ((DeferredProgress*)p)->active;}
static void deferred_progress_iface(GimpProgressInterface*iface){iface->start=deferred_progress_start;iface->set_text=deferred_progress_text;iface->end=deferred_progress_end;iface->is_active=deferred_progress_active;}
G_DEFINE_TYPE_WITH_CODE(DeferredProgress,deferred_progress,G_TYPE_OBJECT,G_IMPLEMENT_INTERFACE(GIMP_TYPE_PROGRESS,deferred_progress_iface))
static void deferred_progress_class_init(DeferredProgressClass*){}
static void deferred_progress_init(DeferredProgress*){}
static void late_writer_reentry(){
 Scene s;auto*file=sentinel_file();CompletionState state;auto*progress=(DeferredProgress*)g_object_new(deferred_progress_get_type(),nullptr);progress->scene=&s;
 file_save_dialog_save_image_async(G_OBJECT(s.display),GIMP_PROGRESS(progress),gimp,s.image,file,save_proc(),GIMP_RUN_NONINTERACTIVE,TRUE,FALSE,FALSE,FALSE,FALSE,completed,&state,completion_destroy);
 g_assert_true(progress->fired);g_assert_false(state.done);assert_sentinel(file);g_assert_true(s.pending());s.drain();auto expected=s.pixels();wait_completed(state);g_assert_true(state.success);assert_saved_pixels(file,expected,s.side);g_object_unref(progress);g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
static void save_and_close_completion(){
 Scene s;auto*file=sentinel_file();gimp_image_set_file(s.image,file);gimp_image_set_save_proc(s.image,save_proc());auto*old=GIMP_DISPLAY(g_object_ref(s.display));
 s.press();s.motion(50,32);s.release();auto*mode=g_variant_ref_sink(g_variant_new_int32(GIMP_SAVE_MODE_SAVE_AND_CLOSE));file_save_cmd_callback(nullptr,mode,s.display);g_variant_unref(mode);g_assert_true(gimp_display_get_image(old)==s.image);assert_sentinel(file);s.drain();auto expected=s.pixels();
 const auto deadline=g_get_monotonic_time()+10000000;while(gimp_display_get_image(old)&&g_get_monotonic_time()<deadline){g_main_context_iteration(nullptr,FALSE);g_usleep(1000);}g_assert_null(gimp_display_get_image(old));g_assert_cmpint(s.depth(),==,1);assert_saved_pixels(file,expected,s.side);
 s.display=gimp_create_display(gimp,s.image,gimp_unit_pixel(),1.0,nullptr);g_assert_nonnull(s.display);g_object_unref(old);g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
struct NestedCommitTool {GimpFillBrushTool parent;gboolean reenter;};
struct NestedCommitToolClass {GimpFillBrushToolClass parent;};
GType nested_commit_tool_get_type(void);
G_DEFINE_TYPE(NestedCommitTool,nested_commit_tool,GIMP_TYPE_FILL_BRUSH_TOOL)
static void nested_control(GimpTool*tool,GimpToolAction action,GimpDisplay*display){
 auto*self=(NestedCommitTool*)tool;GIMP_TOOL_CLASS(nested_commit_tool_parent_class)->control(tool,action,display);
 if(action==GIMP_TOOL_ACTION_COMMIT&&!self->reenter){self->reenter=TRUE;gimp_tool_control(tool,GIMP_TOOL_ACTION_COMMIT,display);self->reenter=FALSE;}
}
static void nested_commit_tool_class_init(NestedCommitToolClass*klass){GIMP_TOOL_CLASS(klass)->control=nested_control;}
static void nested_commit_tool_init(NestedCommitTool*){}
static void nested_commit_requests(){
 Scene s;auto*tool=GIMP_TOOL(g_object_new(nested_commit_tool_get_type(),"tool-info",s.tool->tool_info,nullptr));g_object_unref(s.tool);s.tool=tool;s.klass=GIMP_TOOL_GET_CLASS(tool);s.options=GIMP_PAINT_TOOL_GET_OPTIONS(tool);
 auto*file=sentinel_file();CompletionState state;s.press();s.motion(50,32);start_save(s,G_OBJECT(s.display),file,state);g_assert_true(s.pending());g_assert_false(state.done);s.drain();wait_completed(state);g_assert_true(state.success);g_assert_cmpint(s.depth(),==,1);g_assert(s.pixels()!=s.initial);assert_saved_pixels(file,s.pixels(),s.side);g_file_delete(file,nullptr,nullptr);g_object_unref(file);
}
static void destroy_from_sensitive(GObject*button,GParamSpec*,gpointer data){if(!gtk_widget_get_sensitive(GTK_WIDGET(button)))gtk_widget_destroy(GTK_WIDGET(data));}
static void destroy_from_cancel_enabled(GObject*button,GParamSpec*,gpointer data){if(gtk_widget_get_sensitive(GTK_WIDGET(button)))gtk_widget_destroy(GTK_WIDGET(data));}
static void dialog_closes_before_admission(){
 for(bool on_cancel:{false,true}){Scene s;auto*file=sentinel_file();gimp_image_set_file(s.image,file);auto*dialog=file_save_dialog_new(gimp,FALSE);g_object_ref_sink(dialog);gimp_save_dialog_set_image(GIMP_SAVE_DIALOG(dialog),s.image,FALSE,FALSE,GIMP_OBJECT(s.display));gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),FALSE);GError*error=nullptr;g_assert_true(gtk_file_chooser_set_file(GTK_FILE_CHOOSER(dialog),file,&error));g_assert_no_error(error);gtk_widget_show(dialog);gimp_test_run_mainloop_until_idle();
 bool destroyed=false;g_signal_connect(dialog,"destroy",G_CALLBACK(dialog_destroyed),&destroyed);auto*button=gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog),on_cancel?GTK_RESPONSE_CANCEL:GTK_RESPONSE_OK);g_assert_nonnull(button);g_signal_connect(button,"notify::sensitive",on_cancel?G_CALLBACK(destroy_from_cancel_enabled):G_CALLBACK(destroy_from_sensitive),dialog);
 s.press();s.release();gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);wait_dialog_admission(dialog,destroyed);g_assert_true(destroyed);s.drain();assert_sentinel(file);g_assert(s.pixels()!=s.initial);g_object_unref(dialog);g_file_delete(file,nullptr,nullptr);g_object_unref(file);}
}
int main(int argc,char**argv){
 g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
 g_test_add_func("/save-continuation/query-replaces-file",save_query_replaces_file);g_test_add_func("/save-continuation/dialog-close-before-admission",dialog_closes_before_admission);g_test_add_func("/save-continuation/nested-commit",nested_commit_requests);g_test_add_func("/save-continuation/save-and-close",save_and_close_completion);g_test_add_func("/save-continuation/late-writer-reentry",late_writer_reentry);g_test_add_func("/save-continuation/dialog-response",save_dialog_continuation);g_test_add_func("/save-continuation/active-tool-commit",active_tool_commit);g_test_add_func("/save-continuation/save-export-pixels",deferred_save_export);g_test_add_func("/save-continuation/owner-destroy",deferred_owner_destroy);g_test_add_func("/save-continuation/display-change",deferred_display_change);g_test_add_func("/save-continuation/query-owner-loss",deferred_query_owner_loss);g_test_add_func("/save-continuation/saving-reentry",deferred_saving_reentry);g_test_add_func("/save-continuation/clean-save-action",pending_clean_save_action);
 g_application_run(gimp->app,0,nullptr);gint result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
