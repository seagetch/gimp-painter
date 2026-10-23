/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <string.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpclonelayer.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimpgrouplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpchannel.h"
#include "widgets/gimpviewabledialog.h"
#include "widgets/gimpaction.h"
#include "widgets/gimpactiongroup.h"
#include "actions/layers-actions.h"
#include "actions/layers-commands.h"
#include "dialogs/painter-layer-dialog.h"
#include "painter/gimp-painter-binding.h"
#include "gimp-app-test-utils.h"
#include "tests.h"

static Gimp *gimp;
static GtkWidget *parent;

static GtkWidget *find_widget (GtkWidget *widget, const gchar *name)
{
  if (! g_strcmp0 (gtk_widget_get_name (widget), name)) return widget;
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
      GtkWidget *found = NULL;
      for (GList *iter = children; iter && ! found; iter = iter->next)
        found = find_widget (iter->data, name);
      g_list_free (children);
      return found;
    }
  return NULL;
}
static GtkWidget *field (GtkWidget *dialog, const gchar *name)
{
  GtkWidget *widget = find_widget (dialog, name);
  g_assert_nonnull (widget);
  return widget;
}
static void choice (GtkWidget *dialog, const gchar *name, const gchar *id)
{ g_assert_true (gtk_combo_box_set_active_id (GTK_COMBO_BOX (field (dialog, name)), id)); }
static void number (GtkWidget *dialog, const gchar *name, gdouble value)
{ gtk_spin_button_set_value (GTK_SPIN_BUTTON (field (dialog, name)), value); }
static GimpImage *new_image (void)
{ return gimp_image_new (gimp, 16, 16, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR); }
static GimpLayer *new_source (GimpImage *image, GimpLayer *group, const gchar *name)
{
  GimpLayer *layer = gimp_layer_new (image, 16, 16, babl_format ("R'G'B'A u8"), name, 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_true (gimp_image_add_layer (image, layer, group, 0, FALSE));
  return layer;
}
static void select_one (GimpImage *image, GimpLayer *layer)
{
  GList *selection = layer ? g_list_prepend (NULL, layer) : NULL;
  gimp_image_set_selected_layers (image, selection); g_list_free (selection);
  gimp_context_set_image (gimp_get_user_context (gimp), image);
}
static GtkWidget *new_filter_dialog (GimpImage *image, GimpLayer *layer)
{
  GtkWidget *dialog = painter_filter_layer_dialog_new (image, layer, gimp_get_user_context (gimp), parent);
  g_object_ref_sink (dialog); gtk_widget_show (dialog); return dialog;
}
static GtkWidget *new_clone_dialog (GimpImage *image, GimpLayer *layer)
{
  GtkWidget *dialog = painter_clone_layer_dialog_new (image, layer, gimp_get_user_context (gimp), parent);
  g_object_ref_sink (dialog); gtk_widget_show (dialog); return dialog;
}
static void destroyed (GtkWidget *widget, gpointer data) { *(gboolean *) data = TRUE; }
static void settle (GimpFilterLayer *layer)
{
  gint64 deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (layer) != GIMP_FILTER_LAYER_CLEAN && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL, FALSE); g_usleep (100); }
  g_assert_cmpint (gimp_filter_layer_get_state (layer), ==, GIMP_FILTER_LAYER_CLEAN);
}
static void clone_creation_and_parent (void)
{
  GimpImage *image = new_image ();
  GimpLayer *group = gimp_group_layer_new (image);
  GimpLayer *clone;
  gimp_image_add_layer (image, group, NULL, 0, FALSE);
  new_source (image, group, "child"); select_one (image, group);
  layers_new_clone_cmd_callback (NULL, NULL, gimp);
  clone = gimp_image_get_selected_layers (image)->data;
  g_assert_true (GIMP_IS_CLONE_LAYER (clone));
  g_assert_null (gimp_layer_get_parent (clone));
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)) == group);
  g_assert_cmpint (gimp_item_get_index (GIMP_ITEM (clone)), ==, 0);
  g_assert_cmpint (gimp_layer_get_mode (clone), ==, GIMP_LAYER_MODE_PAINTER_NORMAL);
  g_assert_true (gimp_image_undo (image));
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 2);
  g_assert_true (gimp_image_redo (image));
  select_one (image, group);
  layers_new_clone_cmd_callback (NULL, NULL, gimp);
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 4);
  gimp_context_set_image (gimp_get_user_context (gimp), NULL);
  g_object_unref (image);
}
static void remove_source_during_creation (GimpDrawable *drawable,
                                           gint x, gint y, gint width, gint height,
                                           gpointer data)
{
  gboolean *removed = data;
  if (! *removed)
    {
      *removed = TRUE;
      gimp_image_remove_layer (gimp_item_get_image (GIMP_ITEM (drawable)),
                               GIMP_LAYER (drawable), FALSE, NULL);
    }
}
static void clone_creation_source_reentry (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_source (image, NULL, "source");
  gboolean removed = FALSE;
  select_one (image, source);
  g_signal_connect (source, "update", G_CALLBACK (remove_source_during_creation), &removed);
  layers_new_clone_cmd_callback (NULL, NULL, gimp);
  g_assert_true (removed);
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 0);
  gimp_context_set_image (gimp_get_user_context (gimp), NULL);
  g_object_unref (image);
}
static void clone_edit_cancel_undo_live (void)
{
  GimpImage *image = new_image ();
  GimpLayer *a = new_source (image, NULL, "same");
  GimpLayer *b = new_source (image, NULL, "same");
  GimpLayer *clone = gimp_clone_layer_new (image, a, 0, 0, "clone", 1, GIMP_LAYER_MODE_PAINTER_NORMAL);
  GtkWidget *dialog;
  GeglColor *color;
  guchar pixel[4];
  gchar *id = g_strdup_printf ("%d", gimp_item_get_id (GIMP_ITEM (b)));
  gimp_image_add_layer (image, clone, NULL, 0, FALSE);
  dialog = new_clone_dialog (image, clone); choice (dialog, "painter-clone-source", id);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL); g_object_unref (dialog);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)) == a);
  dialog = new_clone_dialog (image, clone); choice (dialog, "painter-clone-source", id);
  gimp_object_set_name (GIMP_OBJECT (b), "renamed after selection");
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK); g_object_unref (dialog);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)) == b);
  color = gegl_color_new ("red");
  gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (b)), NULL, color); g_object_unref (color);
  gimp_drawable_update (GIMP_DRAWABLE (b), 0, 0, 16, 16);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (clone)), GEGL_RECTANGLE (0, 0, 1, 1), 1, babl_format ("R'G'B'A u8"), pixel, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpint (pixel[0], ==, 255); g_assert_cmpint (pixel[1], ==, 0);
  g_assert_true (gimp_image_undo (image));
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)) == a);
  g_assert_true (gimp_image_redo (image));
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)) == b);
  dialog = new_clone_dialog (image, clone); choice (dialog, "painter-clone-source", "none");
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK); g_object_unref (dialog);
  g_assert_cmpint (gimp_clone_layer_get_source_state (GIMP_CLONE_LAYER (clone)), ==, GIMP_CLONE_SOURCE_NONE);
  g_free (id); g_object_unref (image);
}
static void clone_deleted_source_and_cycle (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_source (image, NULL, "source");
  GimpLayer *group = gimp_group_layer_new (image);
  GimpLayer *clone = gimp_clone_layer_new (image, source, 0, 0, "clone", 1, GIMP_LAYER_MODE_PAINTER_NORMAL);
  GtkWidget *dialog;
  gboolean closed = FALSE;
  gchar *id;
  gimp_image_add_layer (image, group, NULL, 0, FALSE);
  gimp_image_add_layer (image, clone, group, 0, FALSE);
  dialog = new_clone_dialog (image, clone);
  g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
  id = g_strdup_printf ("%d", gimp_item_get_id (GIMP_ITEM (source)));
  choice (dialog, "painter-clone-source", id); g_free (id);
  gimp_image_remove_layer (image, source, TRUE, NULL);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  g_assert_false (closed);
  g_assert_true (gtk_widget_get_visible (field (dialog, "painter-layer-error")));
  id = g_strdup_printf ("%d", gimp_item_get_id (GIMP_ITEM (group)));
  choice (dialog, "painter-clone-source", id); g_free (id);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  /* Legacy ancestor cycles retain their references/cache; the core suppresses
   * recursive evaluation. Do not change that document contract in the UI. */
  g_assert_true (closed);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)) == group);
  g_object_unref (dialog);
  dialog = new_clone_dialog (image, clone);
  gtk_button_clicked (GTK_BUTTON (field (dialog, "painter-clone-refresh")));
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL); g_object_unref (dialog);
  g_object_unref (image);
}
static void filter_create_cancel_and_validate (void)
{
  GimpImage *image = new_image ();
  GtkWidget *dialog;
  GimpLayer *filter;
  gboolean closed = FALSE;
  new_source (image, NULL, "source");
  for (guint i = 0; i < 3; i++)
    {
      dialog = new_filter_dialog (image, NULL);
      choice (dialog, "painter-filter-choice", "edge");
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL); g_object_unref (dialog);
      g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
    }
  dialog = new_filter_dialog (image, NULL);
  g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
  choice (dialog, "painter-filter-choice", "gauss");
  number (dialog, "painter-gauss-horizontal", 0); number (dialog, "painter-gauss-vertical", 0);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  g_assert_false (closed); g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
  number (dialog, "painter-gauss-horizontal", 2.5);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK); g_object_unref (dialog);
  filter = gimp_image_get_selected_layers (image)->data;
  g_assert_true (GIMP_IS_FILTER_LAYER (filter));
  g_assert_cmpint (gimp_layer_get_mode (filter), ==, GIMP_LAYER_MODE_PAINTER_REPLACE);
  settle (GIMP_FILTER_LAYER (filter));
  g_assert_true (gimp_image_undo (image));
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
  g_assert_true (gimp_image_redo (image)); settle (GIMP_FILTER_LAYER (filter));
  g_object_unref (image);
}
static void filter_preserve_unknown_and_undo (void)
{
  GimpImage *image = new_image ();
  GimpLayer *layer = gimp_filter_layer_new (image, 16, 16, "unknown", 1, GIMP_LAYER_MODE_PAINTER_REPLACE);
  GBytes *raw = g_bytes_new_static ("opaque\0raw", 10), *copy;
  GimpValueArray *args = gimp_value_array_new_from_types (NULL, G_TYPE_STRING, "retained unknown value", G_TYPE_NONE);
  GtkWidget *dialog;
  gchar *procedure;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  gchar *text;
  new_source (image, NULL, "source");
  gimp_filter_layer_set_definition (GIMP_FILTER_LAYER (layer), "unknown-plugin", raw, args, NULL);
  gimp_value_array_unref (args); gimp_image_add_layer (image, layer, NULL, 0, FALSE);
  dialog = new_filter_dialog (image, layer);
  g_assert_cmpstr (gtk_combo_box_get_active_id (GTK_COMBO_BOX (field (dialog, "painter-filter-choice"))), ==, "keep");
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (field (dialog, "painter-filter-definition")));
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  g_assert_nonnull (strstr (text, "unknown-plugin")); g_assert_nonnull (strstr (text, "retained unknown value")); g_free (text);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK); g_object_unref (dialog);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
  copy = gimp_filter_layer_ref_definition (GIMP_FILTER_LAYER (layer)); g_assert_true (g_bytes_equal (raw, copy)); g_bytes_unref (copy);
  dialog = new_filter_dialog (image, layer); choice (dialog, "painter-filter-choice", "edge");
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK); g_object_unref (dialog); settle (GIMP_FILTER_LAYER (layer));
  copy = gimp_filter_layer_ref_definition (GIMP_FILTER_LAYER (layer)); g_assert_true (g_bytes_equal (raw, copy)); g_bytes_unref (copy);
  g_assert_true (gimp_image_undo (image));
  procedure = gimp_filter_layer_dup_procedure (GIMP_FILTER_LAYER (layer)); g_assert_cmpstr (procedure, ==, "unknown-plugin"); g_free (procedure);
  g_assert_true (gimp_image_redo (image)); settle (GIMP_FILTER_LAYER (layer));
  g_bytes_unref (raw); g_object_unref (image);
}
static void filter_preserve_float_shape (void)
{
  GimpImage *image = new_image ();
  GimpLayer *layer = gimp_filter_layer_new (image, 16, 16, "edge", 1, GIMP_LAYER_MODE_PAINTER_REPLACE);
  GimpValueArray *args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 7, G_TYPE_INT, 123, G_TYPE_INT, 456, G_TYPE_FLOAT, 2.0, G_TYPE_INT, 2, G_TYPE_NONE);
  GtkWidget *dialog;
  new_source (image, NULL, "source");
  gimp_filter_layer_set_definition (GIMP_FILTER_LAYER (layer), "plug-in-edge", NULL, args, NULL);
  gimp_value_array_unref (args); gimp_image_add_layer (image, layer, NULL, 0, FALSE);
  dialog = new_filter_dialog (image, layer); number (dialog, "painter-edge-amount", 3.5);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK); g_object_unref (dialog);
  args = gimp_filter_layer_dup_args (GIMP_FILTER_LAYER (layer));
  g_assert_cmpint (gimp_value_array_length (args), ==, 5);
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 0)), ==, 7);
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 2)), ==, 456);
  g_assert_true (G_VALUE_HOLDS_FLOAT (gimp_value_array_index (args, 3)));
  g_assert_cmpfloat (g_value_get_float (gimp_value_array_index (args, 3)), ==, 3.5);
  gimp_value_array_unref (args); settle (GIMP_FILTER_LAYER (layer)); g_object_unref (image);
}
static void filter_gaussian_alias_forms (void)
{
  const gchar *names[]={"plug-in-gauss-iir","plug-in-gauss-rle","plug-in-gauss-iir2","plug-in-gauss-rle2"};
  for(guint i=0;i<G_N_ELEMENTS(names);++i)
    {
      GimpImage *image=new_image();
      GimpLayer *layer=gimp_filter_layer_new(image,16,16,"alias",1,GIMP_LAYER_MODE_PAINTER_REPLACE);
      GimpValueArray *args;
      GBytes *raw=g_bytes_new_static("alias original metadata",23),*retained;
      gchar *name;
      GtkWidget *dialog;
      guint64 revision;
      new_source(image,NULL,"source");gimp_image_add_layer(image,layer,NULL,0,FALSE);
      args=i<2?gimp_value_array_new_from_types(NULL,G_TYPE_INT,7,G_TYPE_INT,123,G_TYPE_INT,456,
        G_TYPE_FLOAT,2.5,G_TYPE_INT,-9,G_TYPE_INT,0,G_TYPE_NONE):
        gimp_value_array_new_from_types(NULL,G_TYPE_INT,7,G_TYPE_INT,123,G_TYPE_INT,456,
        G_TYPE_FLOAT,2.5,G_TYPE_DOUBLE,-2.,G_TYPE_NONE);
      g_assert_true(gimp_filter_layer_set_definition(GIMP_FILTER_LAYER(layer),names[i],raw,args,NULL));
      gimp_value_array_unref(args);revision=gimp_filter_layer_get_definition_revision(GIMP_FILTER_LAYER(layer));
      dialog=new_filter_dialog(image,layer);
      g_assert_cmpstr(gtk_combo_box_get_active_id(GTK_COMBO_BOX(field(dialog,"painter-filter-choice"))),==,names[i]+8);
      gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);
      g_assert_cmpuint(gimp_filter_layer_get_definition_revision(GIMP_FILTER_LAYER(layer)),==,revision);
      dialog=new_filter_dialog(image,layer);
      number(dialog,i<2?"painter-gauss-radius":"painter-gauss-horizontal",3.75);
      gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);
      name=gimp_filter_layer_dup_procedure(GIMP_FILTER_LAYER(layer));g_assert_cmpstr(name,==,names[i]);g_free(name);
      args=gimp_filter_layer_dup_args(GIMP_FILTER_LAYER(layer));
      g_assert_cmpuint(gimp_value_array_length(args),==,i<2?6:5);
      g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,0)),==,7);
      g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,1)),==,123);
      g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,2)),==,456);
      g_assert_true(G_VALUE_HOLDS_FLOAT(gimp_value_array_index(args,3)));
      g_assert_cmpfloat(g_value_get_float(gimp_value_array_index(args,3)),==,3.75);
      if(i<2) {g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,4)),==,-9);g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,5)),==,0);}
      else {g_assert_true(G_VALUE_HOLDS_DOUBLE(gimp_value_array_index(args,4)));g_assert_cmpfloat(g_value_get_double(gimp_value_array_index(args,4)),==,-2);}
      gimp_value_array_unref(args);
      retained=gimp_filter_layer_ref_definition(GIMP_FILTER_LAYER(layer));g_assert_true(g_bytes_equal(raw,retained));g_bytes_unref(retained);
      g_assert_true(gimp_image_undo(image));args=gimp_filter_layer_dup_args(GIMP_FILTER_LAYER(layer));
      g_assert_cmpfloat(g_value_get_float(gimp_value_array_index(args,3)),==,2.5);gimp_value_array_unref(args);
      g_assert_true(gimp_image_redo(image));settle(GIMP_FILTER_LAYER(layer));
      g_bytes_unref(raw);g_object_unref(image);
    }
}

static void filter_point_creation_and_edit (void)
{
  const gchar *choices[]={"vinvert","max-rgb","threshold-alpha"};
  for(guint i=0;i<3;++i)
    {
      GimpImage *image=new_image();
      GtkWidget *dialog;
      GimpFilterLayer *filter;
      GimpValueArray *args;
      gchar *procedure,*expected=g_strconcat("plug-in-",choices[i],NULL);
      new_source(image,NULL,"source");dialog=new_filter_dialog(image,NULL);
      choice(dialog,"painter-filter-choice",choices[i]);
      if(i)number(dialog,"painter-point-argument",i==1?-9:127);
      gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);
      filter=GIMP_FILTER_LAYER(gimp_image_get_selected_layers(image)->data);g_assert_true(GIMP_IS_FILTER_LAYER(filter));
      settle(filter);procedure=gimp_filter_layer_dup_procedure(filter);g_assert_cmpstr(procedure,==,expected);g_free(procedure);
      args=gimp_filter_layer_dup_args(filter);g_assert_cmpuint(gimp_value_array_length(args),==,i?4:3);gimp_value_array_unref(args);
      dialog=new_filter_dialog(image,GIMP_LAYER(filter));
      g_assert_cmpstr(gtk_combo_box_get_active_id(GTK_COMBO_BOX(field(dialog,"painter-filter-choice"))),==,choices[i]);
      if(i)number(dialog,"painter-point-argument",i==1?3:255);
      gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_object_unref(dialog);settle(filter);
      args=gimp_filter_layer_dup_args(filter);
      if(i)g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,3)),==,i==1?3:255);
      gimp_value_array_unref(args);g_free(expected);g_object_unref(image);
    }
}

static void filter_choice_replace_during_stack_notify (GObject *object,GParamSpec *spec,gpointer data)
{
  GtkWidget *dialog=data;
  GtkWidget *combo=field(dialog,"painter-filter-choice");
  if(!g_strcmp0(gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo)),"gauss"))
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo),"max-rgb");
}
static void filter_choice_reentry_preserves_latest (void)
{
  GimpImage *image=new_image();
  GtkWidget *dialog=new_filter_dialog(image,NULL);
  GtkWidget *stack=gtk_widget_get_parent(gtk_widget_get_parent(field(dialog,"painter-point-argument")));
  g_assert_true(GTK_IS_STACK(stack));
  g_signal_connect(stack,"notify::visible-child-name",G_CALLBACK(filter_choice_replace_during_stack_notify),dialog);
  choice(dialog,"painter-filter-choice","gauss");
  g_assert_cmpstr(gtk_combo_box_get_active_id(GTK_COMBO_BOX(field(dialog,"painter-filter-choice"))),==,"max-rgb");
  g_assert_true(gtk_widget_get_sensitive(field(dialog,"painter-point-argument")));
  gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);g_object_unref(dialog);g_object_unref(image);
}

static void dialog_lifetimes (void)
{
  GimpImage *image;
  GimpLayer *layer;
  GtkWidget *dialog;
  gboolean closed = FALSE;
  for (guint kind = 0; kind < 3; kind++)
    {
      GimpImage *image = new_image ();
      GimpLayer *source = new_source (image, NULL, "source");
      GimpLayer *layer;
      GtkWidget *dialog;
      gboolean closed = FALSE;
      if (kind == 0) dialog = new_filter_dialog (image, NULL);
      else
        {
          layer = kind == 1 ? gimp_clone_layer_new (image, source, 0, 0, "clone", 1, GIMP_LAYER_MODE_PAINTER_NORMAL) :
                              gimp_filter_layer_new (image, 16, 16, "filter", 1, GIMP_LAYER_MODE_PAINTER_REPLACE);
          gimp_image_add_layer (image, layer, NULL, 0, FALSE);
          dialog = kind == 1 ? new_clone_dialog (image, layer) : new_filter_dialog (image, layer);
        }
      g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
      g_object_unref (image);
      g_assert_true (closed);
      g_object_unref (dialog);
    }
  image = new_image ();
  layer = gimp_filter_layer_new (image, 16, 16, "filter", 1, GIMP_LAYER_MODE_PAINTER_REPLACE);
  gimp_image_add_layer (image, layer, NULL, 0, FALSE);
  dialog = new_filter_dialog (image, layer);
  g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
  gimp_image_remove_layer (image, layer, TRUE, NULL);
  g_assert_true (closed); g_object_unref (dialog); g_object_unref (image);
}
static void action_enablement (void)
{
  GimpImage *image = new_image ();
  GimpContext *context = gimp_get_user_context (gimp);
  GimpActionGroup *group = gimp_action_group_new (gimp, "painter-test", "Painter", NULL, context, layers_actions_update);
  GimpLayer *a = new_source (image, NULL, "a");
  GimpLayer *b = new_source (image, NULL, "b");
  GList *selection;
  layers_actions_setup (group);
#define SENSITIVE(name) gimp_action_get_sensitive (gimp_action_group_get_action (group, name), NULL)
  select_one (image, a); layers_actions_update (group, gimp);
  g_assert_true (SENSITIVE ("layers-new-clone")); g_assert_true (SENSITIVE ("layers-new-filter"));
  g_assert_false (SENSITIVE ("layers-edit-filter"));
  selection = g_list_append (g_list_prepend (NULL, a), b);
  gimp_image_set_selected_layers (image, selection); g_list_free (selection);
  layers_actions_update (group, gimp); g_assert_false (SENSITIVE ("layers-new-clone"));
  layers_new_clone_cmd_callback (NULL, NULL, gimp); g_assert_cmpint (gimp_image_get_n_layers (image), ==, 2);
  select_one (image, a); layers_new_clone_cmd_callback (NULL, NULL, gimp);
  layers_actions_update (group, gimp); g_assert_true (SENSITIVE ("layers-edit-clone"));
  gimp_context_set_image (context, NULL); layers_actions_update (group, gimp);
  g_assert_false (SENSITIVE ("layers-new-filter")); g_assert_false (SENSITIVE ("layers-new-clone"));
  g_object_unref (group); g_object_unref (image);
}

static void child_widget_lifetimes (gconstpointer data)
{
  const gboolean detach = GPOINTER_TO_INT (data);
  const gchar *names[] = { "painter-filter-choice", "painter-edge-amount",
                          "painter-edge-wrap", "painter-edge-method",
                          "painter-gauss-horizontal", "painter-gauss-vertical",
                          "painter-gauss-method", "painter-clone-refresh" };
  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    {
      GimpImage *image = new_image ();
      GimpLayer *source = new_source (image, NULL, "source");
      GimpLayer *clone = gimp_clone_layer_new (image, source, 0, 0, "clone", 1, GIMP_LAYER_MODE_PAINTER_NORMAL);
      GtkWidget *dialog, *child;
      gpointer weak_dialog;
      gimp_image_add_layer (image, clone, NULL, 0, FALSE);
      dialog = i == G_N_ELEMENTS (names) - 1 ? new_clone_dialog (image, clone) : new_filter_dialog (image, NULL);
      child = g_object_ref (field (dialog, names[i]));
      if (detach) gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (child)), child);
      weak_dialog = dialog;
      g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
      gtk_widget_destroy (dialog); g_object_unref (dialog);
      g_assert_null (weak_dialog);
      g_signal_emit_by_name (child, GTK_IS_BUTTON (child) ? "clicked" :
                                  GTK_IS_SPIN_BUTTON (child) ? "value-changed" : "changed");
      gtk_widget_destroy (child); g_object_unref (child); g_object_unref (image);
    }
}

static void close_editor_on_choice (GtkWidget *choice, gpointer data)
{
  GtkWidget **dialog = data;
  if (*dialog)
    {
      GtkWidget *closing = *dialog;
      *dialog = NULL;
      gtk_widget_destroy (closing); g_object_unref (closing);
    }
}

static void clone_refresh_close_reentry (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_source (image, NULL, "source");
  GimpLayer *clone = gimp_clone_layer_new (image, source, 0, 0, "clone", 1, GIMP_LAYER_MODE_PAINTER_NORMAL);
  GtkWidget *dialog, *refresh;
  gpointer weak_dialog;
  gimp_image_add_layer (image, clone, NULL, 0, FALSE);
  dialog = new_clone_dialog (image, clone); weak_dialog = dialog;
  g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
  refresh = g_object_ref (field (dialog, "painter-clone-refresh"));
  g_signal_connect (field (dialog, "painter-clone-source"), "changed", G_CALLBACK (close_editor_on_choice), &dialog);
  gtk_button_clicked (GTK_BUTTON (refresh));
  g_assert_null (dialog); g_assert_null (weak_dialog);
  g_object_unref (refresh); g_object_unref (image);
}

static void filter_non_utf8_preview (void)
{
  const gchar invalid[] = "unknown-\xff-procedure";
  GimpImage *image = new_image ();
  GimpLayer *layer = gimp_filter_layer_new (image, 16, 16, "filter", 1, GIMP_LAYER_MODE_PAINTER_REPLACE);
  GBytes *raw = g_bytes_new_static ("\xff\0\xfe", 3), *copy;
  GimpValueArray *args = gimp_value_array_new_from_types (NULL, G_TYPE_STRING, "string-\xfe", G_TYPE_NONE);
  GtkWidget *dialog;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  gchar *text, *procedure;
  g_assert_true (gimp_filter_layer_set_definition (GIMP_FILTER_LAYER (layer), invalid, raw, args, NULL));
  gimp_value_array_unref (args); gimp_image_add_layer (image, layer, NULL, 0, FALSE);
  dialog = new_filter_dialog (image, layer);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (field (dialog, "painter-filter-definition")));
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  g_assert_true (g_utf8_validate (text, -1, NULL));
  g_assert_nonnull (strstr (text, "unknown-\357\277\275-procedure")); g_free (text);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK); g_object_unref (dialog);
  procedure = gimp_filter_layer_dup_procedure (GIMP_FILTER_LAYER (layer));
  g_assert_cmpmem (procedure, strlen (procedure), invalid, strlen (invalid)); g_free (procedure);
  copy = gimp_filter_layer_ref_definition (GIMP_FILTER_LAYER (layer));
  g_assert_true (g_bytes_equal (copy, raw)); g_bytes_unref (copy); g_bytes_unref (raw);
  args = gimp_filter_layer_dup_args (GIMP_FILTER_LAYER (layer));
  g_assert_cmpstr (g_value_get_string (gimp_value_array_index (args, 0)), ==, "string-\xfe");
  gimp_value_array_unref (args); g_object_unref (image);
}

typedef struct
{
  GtkWidget *dialog;
  GimpImage *image;
  GimpLayer *layer;
  gboolean fail;
  gboolean called;
} ResponseClose;

static void close_editor_on_dirty (GimpImage *image, GimpDirtyMask dirty, gpointer data)
{
  ResponseClose *state = data;
  if (state->called) return;
  state->called = TRUE;
  if (state->fail) g_assert_true (gimp_painter_binding_close (G_OBJECT (state->layer), NULL));
  gtk_widget_destroy (state->dialog); g_clear_object (&state->dialog);
  /* Release the outside image owner during core edit, not a synthetic signal.
   * The response's lease defers actual image disposal until its cleanup. */
  g_clear_object (&state->image);
}

static void response_close_reentry (gconstpointer data)
{
  const gboolean filter = GPOINTER_TO_INT (data);
  for (guint fail = 0; fail < 2; fail++)
    {
      GimpImage *image = new_image ();
      GimpLayer *source = new_source (image, NULL, "source");
      GimpLayer *layer = filter ? gimp_filter_layer_new (image, 16, 16, "filter", 1, GIMP_LAYER_MODE_PAINTER_REPLACE) :
        gimp_clone_layer_new (image, NULL, 0, 0, "clone", 1, GIMP_LAYER_MODE_PAINTER_NORMAL);
      GtkWidget *dialog;
      gpointer weak_image = image, weak_dialog;
      ResponseClose state;
      gimp_image_add_layer (image, layer, NULL, 0, FALSE);
      dialog = filter ? new_filter_dialog (image, layer) : new_clone_dialog (image, layer);
      weak_dialog = dialog;
      g_object_add_weak_pointer (G_OBJECT (image), &weak_image);
      g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
      if (filter) choice (dialog, "painter-filter-choice", "edge");
      else
        {
          gchar *id = g_strdup_printf ("%d", gimp_item_get_id (GIMP_ITEM (source)));
          choice (dialog, "painter-clone-source", id); g_free (id);
        }
      state = (ResponseClose) { dialog, image, layer, fail, FALSE };
      g_signal_connect (image, "dirty", G_CALLBACK (close_editor_on_dirty), &state);
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
      g_assert_true (state.called); g_assert_null (state.dialog); g_assert_null (state.image);
      g_assert_null (weak_dialog); g_assert_null (weak_image);
    }
}

static gboolean close_new_filter_on_status (GSignalInvocationHint *hint,
                                            guint n_values, const GValue *values,
                                            gpointer data)
{
  ResponseClose *state = data;
  if (! state->called)
    {
      state->called = TRUE;
      gtk_widget_destroy (state->dialog); g_clear_object (&state->dialog);
    }
  return TRUE;
}

static void filter_creation_close_reentry (void)
{
  GimpImage *image = new_image ();
  GtkWidget *dialog;
  ResponseClose state;
  guint signal;
  gulong hook;
  gpointer filter_class = g_type_class_ref (GIMP_TYPE_FILTER_LAYER);
  new_source (image, NULL, "source");
  dialog = new_filter_dialog (image, NULL);
  choice (dialog, "painter-filter-choice", "edge");
  state = (ResponseClose) { dialog, image, NULL, FALSE, FALSE };
  signal = g_signal_lookup ("filter-state-changed", GIMP_TYPE_FILTER_LAYER);
  hook = g_signal_add_emission_hook (signal, 0, close_new_filter_on_status, &state, NULL);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  g_signal_remove_emission_hook (signal, hook);
  g_type_class_unref (filter_class);
  g_assert_true (state.called); g_assert_null (state.dialog);
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
  g_object_unref (image);
}
/* A controller closed through the common lifecycle API must become inert even
 * while the native dialog and independently retained controls remain alive. */
static void dialog_binding_close (void)
{
  for (guint kind = 0; kind < 3; kind++)
    {
      GimpImage *image = new_image ();
      GimpLayer *source = new_source (image, NULL, "source");
      GimpLayer *layer = kind == 1 ?
        gimp_clone_layer_new (image, source, 0, 0, "clone", 1, GIMP_LAYER_MODE_PAINTER_NORMAL) :
        kind == 2 ? gimp_filter_layer_new (image, 16, 16, "filter", 1, GIMP_LAYER_MODE_PAINTER_REPLACE) : NULL;
      GtkWidget *dialog, *child;
      gpointer weak_dialog;
      gint layers;
      if (layer) gimp_image_add_layer (image, layer, NULL, 0, FALSE);
      dialog = kind == 1 ? new_clone_dialog (image, layer) : new_filter_dialog (image, layer);
      if (kind == 1) choice (dialog, "painter-clone-source", "none");
      else choice (dialog, "painter-filter-choice", "edge");
      child = g_object_ref (field (dialog, kind == 1 ? "painter-clone-refresh" : "painter-edge-amount"));
      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (child)), child);
      layers = gimp_image_get_n_layers (image);
      g_assert_true (gimp_painter_binding_close (G_OBJECT (dialog), NULL));
      g_assert_true (gimp_painter_binding_close (G_OBJECT (dialog), NULL));
      g_signal_emit_by_name (child, kind == 1 ? "clicked" : "value-changed");
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
      g_assert_cmpint (gimp_image_get_n_layers (image), ==, layers);
      if (kind == 1)
        g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (layer)) == source);
      if (kind == 2)
        {
          gchar *procedure = gimp_filter_layer_dup_procedure (GIMP_FILTER_LAYER (layer));
          g_assert_true (!procedure || !*procedure);
          g_free (procedure);
        }
      weak_dialog = dialog;
      g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
      gtk_widget_destroy (dialog);
      gtk_widget_destroy (dialog);
      g_object_unref (dialog);
      g_assert_null (weak_dialog);
      g_signal_emit_by_name (child, kind == 1 ? "clicked" : "value-changed");
      gtk_widget_destroy (child); g_object_unref (child); g_object_unref (image);
    }
}

static void close_editor_on_notify (GObject *object, GParamSpec *pspec, gpointer data)
{
  close_editor_on_choice (NULL, data);
}

static void filter_choice_last_owner_reentry (void)
{
  GimpImage *image = new_image ();
  GtkWidget *dialog = new_filter_dialog (image, NULL);
  GtkWidget *combo = g_object_ref (field (dialog, "painter-filter-choice"));
  GtkWidget *stack = gtk_widget_get_parent (gtk_widget_get_parent (field (dialog, "painter-edge-amount")));
  gpointer weak_dialog = dialog;
  g_assert_true (GTK_IS_STACK (stack));
  g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
  g_signal_connect (stack, "notify::visible-child-name", G_CALLBACK (close_editor_on_notify), &dialog);
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), "edge");
  g_assert_null (dialog); g_assert_null (weak_dialog);
  g_signal_emit_by_name (combo, "changed");
  g_object_unref (combo); g_object_unref (image);
}

static void filter_status_last_owner_reentry (void)
{
  GimpImage *image = new_image ();
  GimpLayer *layer = gimp_filter_layer_new (image, 16, 16, "filter", 1, GIMP_LAYER_MODE_PAINTER_REPLACE);
  GtkWidget *dialog;
  gpointer weak_dialog;
  gimp_image_add_layer (image, layer, NULL, 0, FALSE);
  dialog = new_filter_dialog (image, layer); weak_dialog = dialog;
  g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
  gtk_label_set_text (GTK_LABEL (field (dialog, "painter-filter-status")), "force a fresh status notification");
  g_signal_connect (field (dialog, "painter-filter-status"), "notify::label", G_CALLBACK (close_editor_on_notify), &dialog);
  g_signal_emit_by_name (layer, "filter-state-changed");
  g_assert_null (dialog); g_assert_null (weak_dialog);
  g_signal_emit_by_name (layer, "filter-state-changed");
  g_object_unref (image);
}

static void dialog_destroy_last_owner_reentry (void)
{
  GimpImage *image = new_image ();
  GtkWidget *dialog = new_filter_dialog (image, NULL);
  GtkWidget *child = g_object_ref (field (dialog, "painter-filter-choice"));
  gpointer weak_dialog = dialog;
  g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
  g_signal_connect (dialog, "destroy", G_CALLBACK (close_editor_on_choice), &dialog);
  /* The image's real disconnect emits destroy, then the external handler drops
   * the last caller reference while the owner-leased callback is still active. */
  g_object_unref (image);
  g_assert_null (dialog); g_assert_null (weak_dialog);
  g_signal_emit_by_name (child, "changed");
  g_object_unref (child);
}

typedef struct
{
  GimpImage *image;
  gpointer weak_dialog;
  gboolean close_only;
  gboolean called;
} FactoryReentry;

static gboolean factory_reentry_hook (GSignalInvocationHint *hint,
                                       guint n_values, const GValue *values,
                                       gpointer data)
{
  FactoryReentry *state = data;
  GtkWidget *widget = g_value_get_object (&values[0]);
  GtkWidget *dialog;
  if (state->called) return TRUE;
  if (state->close_only &&
      (! GTK_IS_COMBO_BOX (widget) ||
       g_strcmp0 (gtk_widget_get_name (widget), "painter-clone-source")))
    return TRUE;
  dialog = gtk_widget_get_toplevel (widget);
  if (! GIMP_IS_VIEWABLE_DIALOG (dialog)) return TRUE;
  state->called = TRUE;
  state->weak_dialog = dialog;
  g_object_add_weak_pointer (G_OBJECT (dialog), &state->weak_dialog);
  if (state->close_only)
    {
      /* The initial source selection runs after slot activation. Common close
       * does not destroy a native window: the factory must clean it up. */
      g_assert_true (gimp_painter_binding_close (G_OBJECT (dialog), NULL));
    }
  else
    {
      /* The first native child show runs inside g_object_new, before the
       * factory can initialize its slot. Drop the last outside image owner. */
      g_clear_object (&state->image);
    }
  return TRUE;
}

static void dialog_factory_reentry (void)
{
  for (guint close_only = 0; close_only < 2; close_only++)
    {
      GimpImage *image = new_image ();
      GimpLayer *source = new_source (image, NULL, "source");
      GimpLayer *clone = gimp_clone_layer_new (image, source, 0, 0, "clone", 1, GIMP_LAYER_MODE_PAINTER_NORMAL);
      FactoryReentry state = { image, NULL, close_only, FALSE };
      gpointer weak_image = image;
      GtkWidget *dialog;
      guint signal = g_signal_lookup (close_only ? "changed" : "show",
                                       close_only ? GTK_TYPE_COMBO_BOX : GTK_TYPE_WIDGET);
      gulong hook;
      gimp_image_add_layer (image, clone, NULL, 0, FALSE);
      g_object_add_weak_pointer (G_OBJECT (image), &weak_image);
      hook = g_signal_add_emission_hook (signal, 0, factory_reentry_hook, &state, NULL);
      dialog = painter_clone_layer_dialog_new (image, clone, gimp_get_user_context (gimp), parent);
      g_signal_remove_emission_hook (signal, hook);
      g_assert_true (state.called);
      g_assert_null (dialog);
      g_assert_null (state.weak_dialog);
      if (close_only) g_clear_object (&state.image);
      g_assert_null (state.image);
      g_assert_null (weak_image);
    }
}

#include "test-isolated-filter-editors.inc"
#include "test-filter-schema-editors.inc"
#include "test-filter-schema-persistence.inc"
#include "test-filter-schema-persistence-failures.inc"
#include "test-filter-progress-ui.inc"

/* C entry points implemented by the native C++ metadata boundary fixtures. */
void gimp_test_filter_editor_metadata (Gimp *application);
void gimp_test_filter_editor_manager_lifetime (Gimp *application);

static void filter_schema_metadata_snapshot (void)
{ gimp_test_filter_editor_metadata (gimp); }

static void filter_schema_manager_lifetime (void)
{ gimp_test_filter_editor_manager_lifetime (gimp); }

void gimp_test_filter_argument_patch (Gimp *application);
static void filter_schema_argument_patch (void)
{ gimp_test_filter_argument_patch (gimp); }

int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  if (! gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  parent = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (parent);
#define ADD(name) g_test_add_func ("/painter-layer-ui/" #name, name)
  ADD (filter_progress_widgets); ADD (filter_progress_running_cancel); ADD (filter_progress_render_reentry);
  ADD (clone_creation_source_reentry); ADD (clone_creation_and_parent); ADD (clone_edit_cancel_undo_live); ADD (clone_deleted_source_and_cycle);
  ADD (filter_create_cancel_and_validate); ADD (filter_preserve_unknown_and_undo); ADD (filter_preserve_float_shape);
  ADD (dialog_lifetimes); ADD (action_enablement);
  g_test_add_data_func ("/painter-layer-ui/retained_child_widgets", GINT_TO_POINTER (FALSE), child_widget_lifetimes);
  g_test_add_data_func ("/painter-layer-ui/detached_child_widgets", GINT_TO_POINTER (TRUE), child_widget_lifetimes);
  g_test_add_data_func ("/painter-layer-ui/clone_response_close_reentry", GINT_TO_POINTER (FALSE), response_close_reentry);
  g_test_add_data_func ("/painter-layer-ui/filter_response_close_reentry", GINT_TO_POINTER (TRUE), response_close_reentry);
  ADD (filter_creation_close_reentry); ADD (filter_gaussian_alias_forms); ADD (filter_point_creation_and_edit); ADD (filter_choice_reentry_preserves_latest);
  ADD (clone_refresh_close_reentry); ADD (filter_non_utf8_preview);
  ADD (dialog_binding_close); ADD (filter_choice_last_owner_reentry);
  ADD (filter_status_last_owner_reentry); ADD (dialog_destroy_last_owner_reentry);
  ADD (dialog_factory_reentry);
  ADD (isolated_editor_create_reedit);
  ADD (isolated_editor_preserve_and_undo);
  ADD (isolated_editor_stale_definition);
  ADD (isolated_editor_saved_enums);
  ADD (isolated_editor_every_field);
  ADD (isolated_editor_invalid_input);
  ADD (isolated_editor_precision_ranges);
  ADD (isolated_editor_unknown_shapes);
  ADD (isolated_editor_lifetimes);
  ADD (isolated_editor_reentry);
  ADD (isolated_editor_running_cache);
  ADD (isolated_editor_save_reopen);
  ADD (filter_schema_registry_invalidation);
  ADD (filter_schema_unavailable_creation);
  ADD (filter_schema_incompatible_metadata);
  ADD (filter_schema_provider_identity);
  ADD (filter_schema_reordered_keys);
  ADD (filter_schema_bounded_preview);
  ADD (filter_schema_materialization_budget);
  ADD (filter_schema_untouched_nonfinite);
  ADD (filter_schema_blinds_context_types);
  ADD (filter_schema_reference_tail_provenance);
  ADD (filter_schema_metadata_snapshot);
  ADD (filter_schema_manager_lifetime);
  ADD (filter_schema_argument_patch);
  ADD (filter_schema_undo_provider_reentry);
  ADD (filter_schema_persistence_routes);
  ADD (filter_schema_persistence_special_bits);
  ADD (filter_schema_persistence_large_tails);
  ADD (filter_schema_persistence_reference_identity);
  ADD (filter_schema_persistence_failures);
  ADD (filter_schema_persistence_nested_null);
  ADD (filter_schema_persistence_unknown_opaque);
  ADD (filter_schema_persistence_active_save);
  ADD (filter_schema_persistence_reference_lease);
  ADD (filter_schema_persistence_double_cache);
  result = g_test_run ();
  gtk_widget_destroy (parent); g_object_unref (parent);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE); return result;
}
