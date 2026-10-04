/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/gimpprogress.h"
static void
filter_progress_widgets (void)
{
  GimpImage *image=new_image();
  GimpValueArray *args=isolated_args(0,0);
  GimpFilterLayer *layer=isolated_layer(image,0,args);
  gimp_filter_layer_mark_as_loaded(layer);
  GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));
  GtkWidget *bar=field(dialog,"painter-filter-progress"), *cancel=field(dialog,"painter-filter-cancel");
  g_assert_false(gtk_widget_get_visible(bar));
  g_assert_true(gimp_progress_start(GIMP_PROGRESS(layer),TRUE,"Visible progress")==GIMP_PROGRESS(layer));
  g_assert_true(gtk_widget_get_visible(bar));g_assert_true(gtk_widget_is_sensitive(cancel));
  gimp_progress_set_value(GIMP_PROGRESS(layer),0.375);
  g_assert_cmpfloat(gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(bar)),==,0.375);
  g_assert_cmpstr(gtk_progress_bar_get_text(GTK_PROGRESS_BAR(bar)),==,"Visible progress");
  g_assert_true(gimp_progress_message(GIMP_PROGRESS(layer),gimp,GIMP_MESSAGE_WARNING,"test","Owner warning"));
  g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(field(dialog,"painter-filter-status"))),==,"Owner warning");
  gtk_button_clicked(GTK_BUTTON(cancel));
  g_assert_cmpint(gimp_filter_layer_get_state(layer),==,GIMP_FILTER_LAYER_FAILED);
  g_assert_false(gtk_widget_get_visible(bar));
  g_assert_true(gimp_progress_start(GIMP_PROGRESS(layer),FALSE,"Cannot cancel")==GIMP_PROGRESS(layer));
  g_assert_false(gtk_widget_is_sensitive(cancel));
  gimp_progress_end(GIMP_PROGRESS(layer));g_assert_false(gtk_widget_get_visible(bar));
  g_object_ref(cancel);gtk_widget_destroy(dialog);g_object_unref(dialog);
  const guint64 generation=gimp_filter_layer_get_generation(layer);
  gtk_button_clicked(GTK_BUTTON(cancel));
  g_assert_cmpuint(generation,==,gimp_filter_layer_get_generation(layer));
  g_object_unref(cancel);gimp_value_array_unref(args);g_object_unref(image);
}
static void
filter_progress_running_cancel (void)
{
  const gint size=512;
  const guint8 source_pixel[]={31,121,217,255}, cached[]={13,29,47,255};
  for(guint route=0;route<4;++route) {
    GimpImage *image=gimp_image_new(gimp,size,size,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
    GimpLayer *source=gimp_layer_new(image,size,size,babl_format("R'G'B'A u8"),"source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
    gimp_image_add_layer(image,source,NULL,0,FALSE);isolated_fill(source,source_pixel);
    GimpValueArray *args=isolated_args(route,0);
    if(route==2)g_value_set_int(gimp_value_array_index(args,4),2);
    GimpFilterLayer *layer=isolated_layer(image,route,args);
    isolated_fill(GIMP_LAYER(layer),cached);
    GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));
    GtkWidget *bar=field(dialog,"painter-filter-progress"), *cancel=field(dialog,"painter-filter-cancel");
    isolated_wait(layer,GIMP_FILTER_LAYER_RUNNING);
    g_assert_true(gtk_widget_get_visible(bar));g_assert_true(gtk_widget_is_sensitive(cancel));
    g_assert_cmpfloat(gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(bar)),>=,0.15);
    gtk_button_clicked(GTK_BUTTON(cancel));
    g_assert_cmpint(gimp_filter_layer_get_state(layer),==,GIMP_FILTER_LAYER_FAILED);
    isolated_assert_raw(layer);isolated_assert_args(layer,route,args);
    g_assert_false(gtk_widget_get_visible(bar));
    // A stale visible cancel cannot reject a just-installed newer generation.
    g_assert_true(gimp_progress_start(GIMP_PROGRESS(layer),TRUE,"old UI session")==GIMP_PROGRESS(layer));
    gchar *procedure=g_strconcat("plug-in-",isolated_choices[route],NULL);
    gimp_filter_layer_set_definition(layer,procedure,NULL,args,NULL);g_free(procedure);
    const guint64 generation=gimp_filter_layer_get_generation(layer);
    gtk_button_clicked(GTK_BUTTON(cancel));
    g_assert_cmpuint(generation,==,gimp_filter_layer_get_generation(layer));
    g_assert_cmpint(gimp_filter_layer_get_state(layer),!=,GIMP_FILTER_LAYER_FAILED);
    gtk_widget_destroy(dialog);g_object_unref(dialog);
    g_object_unref(image);gimp_value_array_unref(args);
  }
}

typedef struct { GimpFilterLayer *layer; GtkWidget *dialog; GimpValueArray *args; guint action; gboolean acted; } ProgressUiReentry;
static void
progress_ui_reenter (GObject *bar, GParamSpec *pspec, gpointer data)
{
  ProgressUiReentry *state=data;
  if(state->acted)return;
  state->acted=TRUE;
  if(state->action==0)gimp_progress_cancel(GIMP_PROGRESS(state->layer));
  else if(state->action==1)gimp_filter_layer_set_definition(state->layer,"plug-in-blinds",NULL,state->args,NULL);
  else gtk_widget_destroy(state->dialog);
}
static void
filter_progress_render_reentry (void)
{
  for(guint action=0;action<3;++action) {
    GimpImage *image=new_image();GimpValueArray *args=isolated_args(0,0);
    GimpFilterLayer *layer=isolated_layer(image,0,args);
    gimp_filter_layer_mark_as_loaded(layer);
    GtkWidget *dialog=new_filter_dialog(image,GIMP_LAYER(layer));
    GtkWidget *bar=field(dialog,"painter-filter-progress"),*cancel=field(dialog,"painter-filter-cancel");
    g_object_ref(bar);g_object_ref(cancel);
    g_assert_nonnull(gimp_progress_start(GIMP_PROGRESS(layer),TRUE,"superseded display"));
    ProgressUiReentry state={layer,dialog,args,action,FALSE};
    g_signal_connect(bar,"notify::fraction",G_CALLBACK(progress_ui_reenter),&state);
    gimp_progress_set_value(GIMP_PROGRESS(layer),0.5);
    g_assert_true(state.acted);
    if(action<2) {
      g_assert_false(gtk_widget_get_visible(bar));
      g_assert_false(gtk_widget_is_sensitive(cancel));
      g_assert_false(gimp_progress_is_active(GIMP_PROGRESS(layer)));
    }
    gtk_widget_destroy(dialog);g_object_unref(dialog);
    g_object_unref(bar);g_object_unref(cancel);g_object_unref(image);gimp_value_array_unref(args);
  }
}
