/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "display/display-types.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
#include "gimpcoreapp.h"
static GimpDisplay *colorify_test_display;
static GtkWidget *colorify_test_saved_parent;
static GimpImage *
colorify_test_image (void)
{
  GimpImage *image=new_image();
  if(g_getenv("GIMP_TEST_COLORIFY_ONCANVAS")){
    g_assert_null(colorify_test_display);gimp_set_focused_once(gimp);
    colorify_test_display=gimp_create_display(gimp,image,gimp_unit_pixel(),32,NULL);g_assert_nonnull(colorify_test_display);
    GimpDisplayShell *shell=gimp_display_get_shell(colorify_test_display);
    colorify_test_saved_parent=parent;parent=g_object_ref(gtk_widget_get_toplevel(GTK_WIDGET(shell)));
    gtk_window_resize(GTK_WINDOW(parent),1040,1800);gtk_window_move(GTK_WINDOW(parent),0,0);gtk_window_present(GTK_WINDOW(parent));
    gimp_context_set_image(gimp_get_user_context(gimp),image);
    gimp_painter_canvas_ui_set_visible(shell,TRUE);
    g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
    gboolean enabled=FALSE;g_object_get(gimp->config,"painter-canvas-ui",&enabled,NULL);g_assert_true(enabled);
    GdkWindow *root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
    g_test_message("Actual per-fixture GimpDisplay shell; oncanvas visible and config enabled;1080x1920");
  }
  return image;
}
static void
colorify_test_close (GimpImage *image)
{
  g_object_unref(image);
  if(colorify_test_display){
    GimpDisplay *display=colorify_test_display;colorify_test_display=NULL;
    GtkWidget *window=parent;parent=colorify_test_saved_parent;colorify_test_saved_parent=NULL;
    gimp_display_close(display);g_object_unref(window);gimp_test_run_mainloop_until_idle();
  }
}
static void
colorify_test_screenshot (const char *name)
{
  if(!g_getenv("GIMP_TEST_COLORIFY_SCREENSHOT_DIR"))return;
  g_assert_nonnull(colorify_test_display);
  g_assert_true(gimp_painter_canvas_ui_get_visible(gimp_display_get_shell(colorify_test_display)));
  gint64 deadline=g_get_monotonic_time()+G_TIME_SPAN_SECOND;
  while(g_get_monotonic_time()<deadline){g_main_context_iteration(NULL,FALSE);g_usleep(10000);}
  GdkWindow *root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
  GdkPixbuf *shot=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(shot);
  gchar *path=g_build_filename(g_getenv("GIMP_TEST_COLORIFY_SCREENSHOT_DIR"),name,NULL);GError *error=NULL;
  g_assert_true(gdk_pixbuf_save(shot,path,"png",&error,NULL));g_assert_no_error(error);g_free(path);g_object_unref(shot);
}
static GVariant *
ui_colorify_tint (gdouble r,gdouble g,gdouble b,gdouble a)
{ return g_variant_ref_sink(g_variant_new("(sudddd)","gimp-painter-colorify-rgba",1u,r,g,b,a)); }
static void
ui_colorify_define (GimpFilterLayer *layer,GBytes *raw,GVariant *v)
{
  GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,-123,G_TYPE_INT,456,G_TYPE_INT,-789,G_TYPE_VARIANT,v,G_TYPE_NONE);
  g_assert_true(gimp_filter_layer_set_definition(layer,"plug-in-colorify",raw,args,NULL));gimp_value_array_unref(args);
}
static void
ui_colorify_equal (GimpFilterLayer *layer,GBytes *raw,GVariant *v)
{
  GimpValueArray *args=gimp_filter_layer_dup_args(layer);GBytes *copy=gimp_filter_layer_ref_definition(layer);
  g_assert_true(g_bytes_equal(raw,copy));g_bytes_unref(copy);
  g_assert_cmpuint(gimp_value_array_length(args),==,4);
  const gint values[]={-123,456,-789};for(guint i=0;i<3;i++){
    g_assert_true(G_VALUE_HOLDS_INT(gimp_value_array_index(args,i)));g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,i)),==,values[i]);}
  GVariant *actual=g_value_get_variant(gimp_value_array_index(args,3));
  if(v){g_assert_nonnull(actual);g_assert_cmpstr(g_variant_get_type_string(actual),==,g_variant_get_type_string(v));
    g_assert_cmpmem(g_variant_get_data(actual),g_variant_get_size(actual),g_variant_get_data(v),g_variant_get_size(v));}
  else g_assert_null(actual);
  gimp_value_array_unref(args);
}
static GimpFilterLayer *
ui_colorify_layer (GimpImage *image,GBytes *raw,GVariant *v)
{
  new_source(image,NULL,"source");
  GimpFilterLayer *layer=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,16,16,"tint",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
  ui_colorify_define(layer,raw,v);g_assert_true(gimp_image_add_layer(image,GIMP_LAYER(layer),NULL,0,FALSE));return layer;
}
static void
colorify_editor_noop_and_edit (void)
{
  GimpImage *image=colorify_test_image();GBytes *raw=g_bytes_new_static("color\0raw",9);
  GVariant *v=ui_colorify_tint(-0.,0.5,1.,-0.);GimpFilterLayer *layer=ui_colorify_layer(image,raw,v);settle(layer);
  for(guint action=0;action<2;action++){
    GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));
    g_assert_cmpstr(gtk_combo_box_get_active_id(GTK_COMBO_BOX(field(dialog,"painter-filter-choice"))),==,"colorify");
    gtk_dialog_response(GTK_DIALOG(dialog),action?GTK_RESPONSE_CANCEL:GTK_RESPONSE_OK);g_object_unref(dialog);
    ui_colorify_equal(layer,raw,v);g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
  }
  GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));
  gtk_entry_set_text(GTK_ENTRY(field(dialog,"painter-colorify-green")),"0.25");
  gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);settle(layer);
  GVariant *edited=ui_colorify_tint(-0.,0.25,1.,-0.);ui_colorify_equal(layer,raw,edited);
  g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
  g_assert_true(gimp_image_undo(image));ui_colorify_equal(layer,raw,v);
  g_assert_true(gimp_image_redo(image));ui_colorify_equal(layer,raw,edited);
  g_variant_unref(edited);g_variant_unref(v);g_bytes_unref(raw);colorify_test_close(image);
}
static void
colorify_editor_unknown_keep (void)
{
  GVariant *variants[]={g_variant_new("(sudddd)","future-tint",1u,1.,1.,1.,1.),
    g_variant_new("(sudddd)","gimp-painter-colorify-rgba",2u,1.,1.,1.,1.),
    g_variant_new("(sudddd)","gimp-painter-colorify-rgba",1u,-1.,1.,1.,1.),
    g_variant_new_string("old boxed tint unavailable"),NULL};
  for(guint i=0;i<G_N_ELEMENTS(variants);i++){
    GVariant *v=variants[i]?g_variant_ref_sink(variants[i]):NULL;GBytes *raw=g_bytes_new_static("opaque",6);GimpImage *image=colorify_test_image();
    GimpFilterLayer *layer=ui_colorify_layer(image,raw,v);GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));
    g_assert_cmpstr(gtk_combo_box_get_active_id(GTK_COMBO_BOX(field(dialog,"painter-filter-choice"))),==,"keep");
    gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);ui_colorify_equal(layer,raw,v);
    g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
    if(v)g_variant_unref(v);g_bytes_unref(raw);colorify_test_close(image);
  }
}
static void
colorify_editor_invalid_and_stale (void)
{
  GimpImage *image=colorify_test_image();GBytes *raw=g_bytes_new_static("raw",3);GVariant *v=ui_colorify_tint(1,1,1,1);
  GimpFilterLayer *layer=ui_colorify_layer(image,raw,v);settle(layer);
  GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));
  const char *invalid[]={"", "NaN", "inf", "-0.1", "1.01", "0.5garbage"};
  for(guint i=0;i<G_N_ELEMENTS(invalid);i++){
    gtk_entry_set_text(GTK_ENTRY(field(dialog,"painter-colorify-red")),invalid[i]);
    gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);ui_colorify_equal(layer,raw,v);
    g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
  }
  gtk_entry_set_text(GTK_ENTRY(field(dialog,"painter-colorify-red")),"0.5");
  GVariant *external=ui_colorify_tint(0.25,0.5,0.75,1);ui_colorify_define(layer,raw,external);
  gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);ui_colorify_equal(layer,raw,external);
  g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
  gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);g_object_unref(dialog);
  g_variant_unref(external);g_variant_unref(v);g_bytes_unref(raw);colorify_test_close(image);
}
static void
colorify_editor_create_pixels (void)
{
  GimpImage *image=colorify_test_image();GimpLayer *source=new_source(image,NULL,"white source");
  guint8 white[16*16*4],actual[16*16*4];memset(white,255,sizeof white);
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,babl_format("R'G'B'A u8"),white,GEGL_AUTO_ROWSTRIDE);
  GtkWidget *dialog=new_filter_dialog(image,NULL);choice(dialog,"painter-filter-choice","colorify");
  gtk_entry_set_text(GTK_ENTRY(field(dialog,"painter-colorify-red")),"0.5");
  gtk_entry_set_text(GTK_ENTRY(field(dialog,"painter-colorify-green")),"0.25");
  gtk_entry_set_text(GTK_ENTRY(field(dialog,"painter-colorify-blue")),"1");
  gtk_entry_set_text(GTK_ENTRY(field(dialog,"painter-colorify-alpha")),"0");
  colorify_test_screenshot("colorify-editor-before-oncanvas-portrait.png");
  gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);
  GimpFilterLayer *layer=GIMP_FILTER_LAYER(gimp_image_get_selected_layers(image)->data);g_assert_true(GIMP_IS_FILTER_LAYER(layer));settle(layer);
  gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),NULL,1,babl_format("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  for(guint p=0;p<sizeof actual;p+=4){g_assert_cmpuint(actual[p],==,127);g_assert_cmpuint(actual[p+1],==,63);g_assert_cmpuint(actual[p+2],==,254);g_assert_cmpuint(actual[p+3],==,255);}
  gimp_image_flush(image);colorify_test_screenshot("colorify-editor-after-oncanvas-portrait.png");
  colorify_test_close(image);
}
static void
colorify_editor_closed_controls (void)
{
  GimpImage *image=colorify_test_image();GBytes *raw=g_bytes_new_static("raw",3);GVariant *v=ui_colorify_tint(1,1,1,1);
  GimpFilterLayer *layer=ui_colorify_layer(image,raw,v);GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));
  GtkWidget *entry=g_object_ref(field(dialog,"painter-colorify-red"));
  gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(entry)),entry);
  g_assert_true(gimp_painter_binding_close(G_OBJECT(dialog),NULL));
  gtk_entry_set_text(GTK_ENTRY(entry),"0.5");gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);
  ui_colorify_equal(layer,raw,v);g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
  gtk_widget_destroy(dialog);g_object_unref(dialog);gtk_widget_destroy(entry);g_object_unref(entry);
  g_variant_unref(v);g_bytes_unref(raw);colorify_test_close(image);
}
static void
colorify_editor_creation_close_reentry (void)
{
  GimpImage *image=colorify_test_image();new_source(image,NULL,"source");
  GtkWidget *dialog=new_filter_dialog(image,NULL);choice(dialog,"painter-filter-choice","colorify");
  ResponseClose state={dialog,image,NULL,FALSE,FALSE};gpointer klass=g_type_class_ref(GIMP_TYPE_FILTER_LAYER);
  guint signal=g_signal_lookup("filter-state-changed",GIMP_TYPE_FILTER_LAYER);
  gulong hook=g_signal_add_emission_hook(signal,0,close_new_filter_on_status,&state,NULL);
  gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);
  g_signal_remove_emission_hook(signal,hook);g_type_class_unref(klass);
  g_assert_true(state.called);g_assert_null(state.dialog);g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
  g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);colorify_test_close(image);
}
typedef struct {GtkWidget *entry;gboolean called;} ColorifyDraftMutation;
static void
colorify_mutate_draft_at_undo (GimpImage *image,GimpUndoEvent event,GimpUndo *undo,gpointer data)
{
  ColorifyDraftMutation *state=data;
  if(event!=GIMP_UNDO_EVENT_UNDO_PUSHED || state->called)return;
  state->called=TRUE;gtk_entry_set_text(GTK_ENTRY(state->entry),"0.75");
}
static void
colorify_editor_draft_reentry (void)
{
  GimpImage *image=colorify_test_image();GBytes *raw=g_bytes_new_static("raw",3);GVariant *v=ui_colorify_tint(1,1,1,1);
  GimpFilterLayer *layer=ui_colorify_layer(image,raw,v);settle(layer);GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));
  GtkWidget *entry=field(dialog,"painter-colorify-red");gtk_entry_set_text(GTK_ENTRY(entry),"0.5");
  ColorifyDraftMutation mutation={entry,FALSE};
  gulong hook=g_signal_connect(image,"undo-event",G_CALLBACK(colorify_mutate_draft_at_undo),&mutation);
  gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_assert_true(mutation.called);
  ui_colorify_equal(layer,raw,v);g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
  g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(entry)),==,"0.75");
  g_signal_handler_disconnect(image,hook);gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);g_object_unref(dialog);
  g_variant_unref(v);g_bytes_unref(raw);colorify_test_close(image);
}
