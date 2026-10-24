/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Uses the existing actual-display/oncanvas fixture lifecycle. The legacy
 * COLORIFY environment switch names that shared opt-in harness, not this route. */
static void
ui_video_define (GimpFilterLayer *layer,GBytes *raw,gint pattern,gint additive,gint rotated)
{
 GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,G_MININT,G_TYPE_INT,456,G_TYPE_INT,-789,
   G_TYPE_INT,pattern,G_TYPE_INT,additive,G_TYPE_INT,rotated,G_TYPE_NONE);
 g_assert_true(gimp_filter_layer_set_definition(layer,"plug-in-video",raw,args,NULL));gimp_value_array_unref(args);
}
static void
ui_video_equal (GimpFilterLayer *layer,GBytes *raw,gint pattern,gint additive,gint rotated)
{
 const gint values[]={G_MININT,456,-789,pattern,additive,rotated};GimpValueArray *args=gimp_filter_layer_dup_args(layer);
 gchar *name=gimp_filter_layer_dup_procedure(layer);g_assert_cmpstr(name,==,"plug-in-video");g_free(name);
 GBytes *copy=gimp_filter_layer_ref_definition(layer);g_assert_true(g_bytes_equal(raw,copy));g_bytes_unref(copy);
 g_assert_nonnull(args);g_assert_cmpuint(gimp_value_array_length(args),==,6);
 for(guint i=0;i<6;i++){g_assert_true(G_VALUE_HOLDS_INT(gimp_value_array_index(args,i)));g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,i)),==,values[i]);}
 gimp_value_array_unref(args);
}
static GimpFilterLayer *
ui_video_layer (GimpImage *image,GBytes *raw,gint pattern,gint additive,gint rotated)
{
 new_source(image,NULL,"source");GimpFilterLayer *layer=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,16,16,"video",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
 ui_video_define(layer,raw,pattern,additive,rotated);g_assert_true(gimp_image_add_layer(image,GIMP_LAYER(layer),NULL,0,FALSE));return layer;
}
static void
video_editor_create_pixels (void)
{
#include "../../migration/tests/video-foundation/reference-patterns.h"
 GimpImage *image=colorify_test_image();GimpLayer *source=new_source(image,NULL,"white source");
 guint8 white[16*16*4],actual[16*16*4];memset(white,255,sizeof white);
 gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,babl_format("R'G'B'A u8"),white,GEGL_AUTO_ROWSTRIDE);
 GtkWidget *dialog=new_filter_dialog(image,NULL);choice(dialog,"painter-filter-choice","video");
 /* Fresh controls follow pinned old defaults: striped/additive/not rotated. */
 g_assert_cmpstr(gtk_combo_box_get_active_id(GTK_COMBO_BOX(field(dialog,"painter-video-pattern"))),==,"2");
 g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(field(dialog,"painter-video-additive"))));
 g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(field(dialog,"painter-video-rotated"))));
 choice(dialog,"painter-video-pattern","7");gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(field(dialog,"painter-video-additive")),FALSE);
 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(field(dialog,"painter-video-rotated")),TRUE);
 colorify_test_screenshot("video-editor-before-oncanvas-portrait.png",dialog);
 gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);
 GimpFilterLayer *layer=GIMP_FILTER_LAYER(gimp_image_get_selected_layers(image)->data);g_assert_true(GIMP_IS_FILTER_LAYER(layer));settle(layer);
 gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),NULL,1,babl_format("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
 for(guint y=0;y<16;y++)for(guint x=0;x<16;x++){
  guint selected=rows[7][x%heights[7]][y%widths[7]]-'0',p=(y*16+x)*4;
  for(guint c=0;c<3;c++)g_assert_cmpuint(actual[p+c],==,c==selected?255:0);g_assert_cmpuint(actual[p+3],==,255);}
 gimp_image_flush(image);colorify_test_screenshot("video-editor-after-oncanvas-portrait.png",NULL);colorify_test_close(image);
}
static void
video_editor_noop_and_edit (void)
{
 GimpImage *image=colorify_test_image();GBytes *raw=g_bytes_new_static("video\0raw",9);
 GimpFilterLayer *layer=ui_video_layer(image,raw,7,G_MININT,G_MAXINT);settle(layer);
 for(guint action=0;action<3;action++){
  GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));
  g_assert_cmpstr(gtk_combo_box_get_active_id(GTK_COMBO_BOX(field(dialog,"painter-filter-choice"))),==,"video");
  if(action==2){GtkToggleButton *b=GTK_TOGGLE_BUTTON(field(dialog,"painter-video-additive"));gtk_toggle_button_set_active(b,FALSE);gtk_toggle_button_set_active(b,TRUE);}
  gtk_dialog_response(GTK_DIALOG(dialog),action==1?GTK_RESPONSE_CANCEL:GTK_RESPONSE_OK);g_object_unref(dialog);
  ui_video_equal(layer,raw,7,G_MININT,G_MAXINT);g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
 }
 GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));choice(dialog,"painter-video-pattern","8");
 gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);settle(layer);ui_video_equal(layer,raw,8,G_MININT,G_MAXINT);
 g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
 g_assert_true(gimp_image_undo(image));ui_video_equal(layer,raw,7,G_MININT,G_MAXINT);
 g_assert_true(gimp_image_redo(image));ui_video_equal(layer,raw,8,G_MININT,G_MAXINT);
 g_bytes_unref(raw);colorify_test_close(image);
}
static void
video_editor_flag_edits (void)
{
 for(guint flag=0;flag<2;flag++){
  GimpImage *image=colorify_test_image();GBytes *raw=g_bytes_new_static("raw",3);GimpFilterLayer *layer=ui_video_layer(image,raw,7,G_MININT,G_MAXINT);settle(layer);
  GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(field(dialog,flag?"painter-video-rotated":"painter-video-additive")),FALSE);
  gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);settle(layer);
  ui_video_equal(layer,raw,7,flag?G_MININT:0,flag?0:G_MAXINT);
  g_assert_true(gimp_image_undo(image));ui_video_equal(layer,raw,7,G_MININT,G_MAXINT);
  g_assert_true(gimp_image_redo(image));ui_video_equal(layer,raw,7,flag?G_MININT:0,flag?0:G_MAXINT);
  g_bytes_unref(raw);colorify_test_close(image);
 }
}
static void
video_editor_unknown_keep_replace (void)
{
 GimpImage *image=colorify_test_image();GBytes *raw=g_bytes_new_static("invalid-old-pattern",19);
 GimpFilterLayer *layer=ui_video_layer(image,raw,9,G_MININT,G_MAXINT);
 GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));g_assert_cmpstr(gtk_combo_box_get_active_id(GTK_COMBO_BOX(field(dialog,"painter-filter-choice"))),==,"keep");
 gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);ui_video_equal(layer,raw,9,G_MININT,G_MAXINT);
 g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
 dialog=new_filter_dialog(image,GIMP_LAYER(layer));choice(dialog,"painter-filter-choice","video");
 gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);settle(layer);
 ui_video_equal(layer,raw,2,1,0);g_assert_true(gimp_image_undo(image));ui_video_equal(layer,raw,9,G_MININT,G_MAXINT);
 g_bytes_unref(raw);colorify_test_close(image);
}
static void
video_editor_malformed_context_replace (void)
{
 GimpImage *image=colorify_test_image();GBytes *raw=g_bytes_new_static("opaque",6);
 GimpFilterLayer *layer=ui_video_layer(image,raw,2,0,0);
 GimpValueArray *bad=gimp_value_array_new_from_types(NULL,G_TYPE_UINT,7u,G_TYPE_INT,456,G_TYPE_INT,-789,G_TYPE_INT,2,G_TYPE_INT,0,G_TYPE_INT,0,G_TYPE_NONE);
 g_assert_true(gimp_filter_layer_set_definition(layer,"plug-in-video",raw,bad,NULL));gimp_value_array_unref(bad);
 GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));g_assert_cmpstr(gtk_combo_box_get_active_id(GTK_COMBO_BOX(field(dialog,"painter-filter-choice"))),==,"keep");
 gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);
 GimpValueArray *args=gimp_filter_layer_dup_args(layer);g_assert_true(G_VALUE_HOLDS_UINT(gimp_value_array_index(args,0)));g_assert_cmpuint(g_value_get_uint(gimp_value_array_index(args,0)),==,7);gimp_value_array_unref(args);
 g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
 dialog=new_filter_dialog(image,GIMP_LAYER(layer));choice(dialog,"painter-filter-choice","video");gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);settle(layer);
 args=gimp_filter_layer_dup_args(layer);g_assert_cmpuint(gimp_value_array_length(args),==,6);const gint values[]={0,0,0,2,1,0};
 for(guint i=0;i<6;i++){g_assert_true(G_VALUE_HOLDS_INT(gimp_value_array_index(args,i)));g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,i)),==,values[i]);}gimp_value_array_unref(args);
 GBytes *copy=gimp_filter_layer_ref_definition(layer);g_assert_true(g_bytes_equal(raw,copy));g_bytes_unref(copy);
 g_assert_true(gimp_image_undo(image));args=gimp_filter_layer_dup_args(layer);g_assert_true(G_VALUE_HOLDS_UINT(gimp_value_array_index(args,0)));g_assert_cmpuint(g_value_get_uint(gimp_value_array_index(args,0)),==,7);gimp_value_array_unref(args);
 g_bytes_unref(raw);colorify_test_close(image);
}
static void
video_editor_stale_and_invalid_dropdown (void)
{
 GimpImage *image=colorify_test_image();GBytes *raw=g_bytes_new_static("raw",3);GimpFilterLayer *layer=ui_video_layer(image,raw,7,G_MININT,G_MAXINT);settle(layer);
 GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));GtkComboBox *pattern=GTK_COMBO_BOX(field(dialog,"painter-video-pattern"));
 gtk_combo_box_set_active(pattern,-1);gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);
 ui_video_equal(layer,raw,7,G_MININT,G_MAXINT);g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
 choice(dialog,"painter-video-pattern","8");ui_video_define(layer,raw,6,-1,0);
 gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);ui_video_equal(layer,raw,6,-1,0);
 g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
 gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);g_object_unref(dialog);g_bytes_unref(raw);colorify_test_close(image);
}
static void
video_editor_closed_controls (void)
{
 GimpImage *image=colorify_test_image();GBytes *raw=g_bytes_new_static("raw",3);GimpFilterLayer *layer=ui_video_layer(image,raw,7,G_MININT,G_MAXINT);
 GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));GtkWidget *toggle=g_object_ref(field(dialog,"painter-video-additive"));
 gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(toggle)),toggle);g_assert_true(gimp_painter_binding_close(G_OBJECT(dialog),NULL));
 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),FALSE);gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);
 ui_video_equal(layer,raw,7,G_MININT,G_MAXINT);g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
 gtk_widget_destroy(dialog);g_object_unref(dialog);gtk_widget_destroy(toggle);g_object_unref(toggle);g_bytes_unref(raw);colorify_test_close(image);
}
static void
video_editor_creation_close_reentry (void)
{
 GimpImage *image=colorify_test_image();new_source(image,NULL,"source");GtkWidget *dialog=new_filter_dialog(image,NULL);choice(dialog,"painter-filter-choice","video");
 ResponseClose state={dialog,image,NULL,FALSE,FALSE};gpointer klass=g_type_class_ref(GIMP_TYPE_FILTER_LAYER);
 guint signal=g_signal_lookup("filter-state-changed",GIMP_TYPE_FILTER_LAYER);gulong hook=g_signal_add_emission_hook(signal,0,close_new_filter_on_status,&state,NULL);
 gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_signal_remove_emission_hook(signal,hook);g_type_class_unref(klass);
 g_assert_true(state.called);g_assert_null(state.dialog);g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
 g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);colorify_test_close(image);
}
typedef struct {GtkWidget *toggle;gboolean called;} VideoDraftMutation;
static void
video_mutate_draft_at_undo (GimpImage *image,GimpUndoEvent event,GimpUndo *undo,gpointer data)
{
 VideoDraftMutation *state=data;if(event!=GIMP_UNDO_EVENT_UNDO_PUSHED||state->called)return;state->called=TRUE;
 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(state->toggle),FALSE);
}
static void
video_editor_draft_reentry (void)
{
 GimpImage *image=colorify_test_image();GBytes *raw=g_bytes_new_static("raw",3);GimpFilterLayer *layer=ui_video_layer(image,raw,7,G_MININT,G_MAXINT);settle(layer);
 GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));choice(dialog,"painter-video-pattern","8");
 VideoDraftMutation mutation={field(dialog,"painter-video-additive"),FALSE};gulong hook=g_signal_connect(image,"undo-event",G_CALLBACK(video_mutate_draft_at_undo),&mutation);
 gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_assert_true(mutation.called);
 ui_video_equal(layer,raw,7,G_MININT,G_MAXINT);g_assert_cmpuint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
 g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mutation.toggle)));
 g_signal_handler_disconnect(image,hook);gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);g_object_unref(dialog);g_bytes_unref(raw);colorify_test_close(image);
}
