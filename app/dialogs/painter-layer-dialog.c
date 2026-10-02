/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The Painter layers are independent document types, not drawable effects.
 * This editor only invokes explicitly supported compatibility executors. */
#include "config.h"
#include <math.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "dialogs-types.h"
#include "core/gimp.h"
#include "core/gimpclonelayer.h"
#include "core/gimpcontext.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "operations/layer-modes-legacy/gimpoperationpainterlegacy.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpviewabledialog.h"
#include "painter-layer-dialog.h"
#include "gimp-intl.h"

typedef struct
{
  GWeakRef   image;
  GWeakRef   layer;
  gboolean   editing;
  gboolean   closed;
  gboolean   dirty;
  GtkWidget *dialog;
  GtkWidget *grid;
  GtkWidget *name;
  GtkWidget *width;
  GtkWidget *height;
  GtkWidget *choice;
  GtkWidget *parameters;
  GtkWidget *amount;
  GtkWidget *wrap;
  GtkWidget *edge;
  GtkWidget *horizontal;
  GtkWidget *vertical;
  GtkWidget *method;
  GtkWidget *status;
  GtkWidget *error;
  gint       row;
} PainterLayerDialog;

static PainterLayerDialog *
painter_dialog_get (GtkWidget *dialog)
{
  return g_object_get_data (G_OBJECT (dialog), "painter-layer-dialog");
}

static void
painter_dialog_free (gpointer data)
{
  PainterLayerDialog *state = data;
  g_weak_ref_clear (&state->image);
  g_weak_ref_clear (&state->layer);
  g_free (state);
}

static void
painter_dialog_destroy (GtkWidget *dialog,
                        gpointer   data)
{
  painter_dialog_get (dialog)->closed = TRUE;
}

static void
painter_dialog_error (PainterLayerDialog *state,
                      const gchar        *message)
{
  gchar *valid;
  if (state->closed) return;
  valid = g_utf8_make_valid (message, -1);
  gtk_label_set_text (GTK_LABEL (state->error), valid);
  g_free (valid);
  if (! state->closed) gtk_widget_show (state->error);
}

static GtkWidget *
painter_dialog_field (PainterLayerDialog *state,
                      const gchar        *label,
                      GtkWidget          *widget,
                      const gchar        *name)
{
  gtk_widget_set_name (widget, name);
  gimp_grid_attach_aligned (GTK_GRID (state->grid), 0, state->row++,
                            label, 0.0, 0.5, widget, 1);
  return widget;
}

static GtkWidget *
painter_dialog_spin (PainterLayerDialog *state,
                     const gchar        *label,
                     const gchar        *name,
                     gdouble             min,
                     gdouble             max,
                     gdouble             value,
                     guint               digits)
{
  GtkWidget *spin = gtk_spin_button_new_with_range (min, max, digits ? 0.1 : 1.0);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spin), digits);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), value);
  return painter_dialog_field (state, label, spin, name);
}

static PainterLayerDialog *
painter_dialog_new (GimpImage   *image,
                    GimpLayer   *layer,
                    GimpContext *context,
                    GtkWidget   *parent,
                    const gchar *title,
                    const gchar *role,
                    const gchar *description)
{
  PainterLayerDialog *state = g_new0 (PainterLayerDialog, 1);
  GtkWidget         *content;

  g_weak_ref_init (&state->image, image);
  g_weak_ref_init (&state->layer, layer);
  state->editing = layer != NULL;
  state->dialog = gimp_viewable_dialog_new (
    g_list_prepend (NULL, layer ? GIMP_VIEWABLE (layer) : GIMP_VIEWABLE (image)),
    context, title, role, GIMP_ICON_LAYER, description, parent,
    gimp_standard_help_func, GIMP_HELP_LAYER_EDIT,
    _("_Cancel"), GTK_RESPONSE_CANCEL, _("_OK"), GTK_RESPONSE_OK, NULL);
  g_object_set_data_full (G_OBJECT (state->dialog), "painter-layer-dialog",
                          state, painter_dialog_free);
  g_signal_connect (state->dialog, "destroy", G_CALLBACK (painter_dialog_destroy), NULL);
  /* Item removal closes edits; image disconnect must also close dialogs whose
   * item is temporarily retained by Undo, a view or another caller. */
  g_signal_connect_object (image, "disconnect", G_CALLBACK (gtk_widget_destroy),
                           state->dialog, G_CONNECT_SWAPPED);
  gtk_dialog_set_default_response (GTK_DIALOG (state->dialog), GTK_RESPONSE_OK);
  content = gtk_dialog_get_content_area (GTK_DIALOG (state->dialog));
  state->grid = gtk_grid_new ();
  gtk_container_set_border_width (GTK_CONTAINER (state->grid), 12);
  gtk_grid_set_column_spacing (GTK_GRID (state->grid), 8);
  gtk_grid_set_row_spacing (GTK_GRID (state->grid), 8);
  gtk_box_pack_start (GTK_BOX (content), state->grid, TRUE, TRUE, 0);
  state->error = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (state->error), TRUE);
  gtk_label_set_xalign (GTK_LABEL (state->error), 0);
  gtk_widget_set_name (state->error, "painter-layer-error");
  gtk_box_pack_start (GTK_BOX (content), state->error, FALSE, FALSE, 8);
  return state;
}

static void
painter_clone_refresh (GtkWidget *button,
                       GtkWidget *dialog)
{
  PainterLayerDialog *state = painter_dialog_get (dialog);
  GimpImage         *image = g_weak_ref_get (&state->image);
  GimpLayer         *layer = g_weak_ref_get (&state->layer);
  gchar             *active = NULL;
  gchar             *source_name = NULL;
  gchar             *description = NULL;
  gchar             *valid = NULL;
  GtkTreeModel      *model = NULL;
  GList             *layers = NULL;
  GList             *iter;

  if (! image || ! layer || state->closed)
    goto out;

  /* remove_all emits changed while GtkListStore is still clearing. A handler
   * may destroy the combo and release its model before that method returns. */
  model = g_object_ref (gtk_combo_box_get_model (GTK_COMBO_BOX (state->choice)));
  active = g_strdup (gtk_combo_box_get_active_id (GTK_COMBO_BOX (state->choice)));
  gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (state->choice));
  if (state->closed) goto out;
  source_name = gimp_clone_layer_dup_source_name (GIMP_CLONE_LAYER (layer));
  description = g_strdup_printf (_("Keep current reference: %s (%s)"),
                                  source_name ? source_name : _("None"),
                                  gimp_clone_layer_get_source_state (GIMP_CLONE_LAYER (layer)) == GIMP_CLONE_SOURCE_LIVE ?
                                  _("live") : _("unresolved or empty"));
  valid = g_utf8_make_valid (description, -1);
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice), "keep", valid);
  if (state->closed) goto out;
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice), "none", _("No source"));
  if (state->closed) goto out;
  layers = gimp_image_get_layer_list (image);
  for (iter = layers; iter; iter = iter->next)
    g_object_ref (iter->data);
  for (iter = layers; iter && ! state->closed; iter = iter->next)
    {
      GimpItem *item = iter->data;
      GimpItem *ancestor;
      gchar    *id;
      gchar    *label;
      GString  *path;
      if (item == GIMP_ITEM (layer))
        continue;
      id = g_strdup_printf ("%d", gimp_item_get_id (item));
      path = g_string_new (gimp_object_get_name (item));
      for (ancestor = gimp_item_get_parent (item); ancestor;
           ancestor = gimp_item_get_parent (ancestor))
        {
          g_string_prepend (path, " / ");
          g_string_prepend (path, gimp_object_get_name (ancestor));
        }
      /* Identity, not a potentially duplicated/renamed label, is selected. */
      g_string_append_printf (path, " (#%s)", id);
      label = g_utf8_make_valid (path->str, -1);
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice), id, label);
      g_free (label);
      g_string_free (path, TRUE);
      g_free (id);
    }
  if (! state->closed &&
      (! active || ! gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice), active)))
    {
      if (! state->closed)
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice), "keep");
    }
out:
  g_free (active);
  g_free (source_name);
  g_free (description);
  g_free (valid);
  g_clear_object (&model);
  g_list_free_full (layers, g_object_unref);
  g_clear_object (&image);
  g_clear_object (&layer);
}

static void
painter_clone_response (GtkWidget *dialog,
                        gint       response,
                        gpointer   data)
{
  PainterLayerDialog *state = data;
  GimpImage         *image = NULL;
  GimpLayer         *layer = NULL;
  GimpItem          *source = NULL;
  const gchar       *id;
  GError            *error = NULL;
  gboolean           success;

  /* Core notifications can destroy the widgets and drop the outside dialog
   * reference. Keep its state alive, but never mistake that lease for live UI. */
  g_object_ref (dialog);
  if (response != GTK_RESPONSE_OK)
    goto close;
  image = g_weak_ref_get (&state->image);
  layer = g_weak_ref_get (&state->layer);
  if (state->closed || ! image || ! layer ||
      gimp_item_get_image (GIMP_ITEM (layer)) != image ||
      ! gimp_item_is_attached (GIMP_ITEM (layer)))
    goto close;
  id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (state->choice));
  if (! id || g_str_equal (id, "keep"))
    goto close;
  if (! g_str_equal (id, "none"))
    {
      source = gimp_item_get_by_id (image->gimp, (gint) g_ascii_strtoll (id, NULL, 10));
      if (! GIMP_IS_LAYER (source) || gimp_item_get_image (source) != image ||
          ! gimp_item_is_attached (source))
        {
          painter_dialog_error (state, _("The selected source was removed. Refresh the list and select a source again."));
          goto out;
        }
    }
  success = gimp_clone_layer_set_source_with_undo (GIMP_CLONE_LAYER (layer),
                                                  source ? GIMP_LAYER (source) : NULL,
                                                  _("Change Clone Layer Source"), &error);
  if (state->closed) goto close;
  if (! success)
    {
      painter_dialog_error (state, error ? error->message : _("The source could not be changed."));
      goto out;
    }
  gimp_image_flush (image);
close:
  gtk_widget_destroy (dialog);
out:
  g_clear_error (&error);
  g_clear_object (&image);
  g_clear_object (&layer);
  g_object_unref (dialog);
}

GtkWidget *
painter_clone_layer_dialog_new (GimpImage   *image,
                                GimpLayer   *layer,
                                GimpContext *context,
                                GtkWidget   *parent)
{
  PainterLayerDialog *state;
  GtkWidget         *refresh;
  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (GIMP_IS_CLONE_LAYER (layer), NULL);
  state = painter_dialog_new (image, layer, context, parent,
                              _("Clone Layer Source"), "gimp-clone-layer-source",
                              _("Choose the layer or group this clone follows"));
  state->choice = painter_dialog_field (state, _("_Source:"), gtk_combo_box_text_new (), "painter-clone-source");
  refresh = painter_dialog_field (state, NULL, gtk_button_new_with_mnemonic (_("_Refresh Sources")), "painter-clone-refresh");
  g_signal_connect_object (refresh, "clicked", G_CALLBACK (painter_clone_refresh), state->dialog, 0);
  g_signal_connect (state->dialog, "response", G_CALLBACK (painter_clone_response), state);
  painter_clone_refresh (NULL, state->dialog);
  gtk_widget_show_all (state->grid);
  return state->dialog;
}

static void
painter_filter_dirty (GtkWidget *widget,
                      GtkWidget *dialog)
{
  PainterLayerDialog *state = painter_dialog_get (dialog);
  if (! state->closed) state->dirty = TRUE;
}

static void
painter_filter_choice_changed (GtkWidget *widget,
                               GtkWidget *dialog)
{
  PainterLayerDialog *state = painter_dialog_get (dialog);
  const gchar       *id;
  if (state->closed) return;
  id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (state->choice));
  gtk_stack_set_visible_child_name (GTK_STACK (state->parameters), id ? id : "keep");
  state->dirty = TRUE;
}

static void
painter_filter_status (GimpFilterLayer *layer,
                       GtkWidget       *dialog)
{
  PainterLayerDialog *state = painter_dialog_get (dialog);
  gchar             *error;
  gchar             *valid;
  const gchar       *text;
  if (state->closed) return;
  error = gimp_filter_layer_dup_error (layer);
  switch (gimp_filter_layer_get_state (layer))
    {
    case GIMP_FILTER_LAYER_CLEAN: text = _("Up to date. Changes below this layer update it automatically."); break;
    case GIMP_FILTER_LAYER_FAILED: text = error ? error : _("This definition has no supported executor. Its saved data is retained."); break;
    case GIMP_FILTER_LAYER_CLOSED: text = _("Layer closed"); break;
    default: text = _("Updating in the background…"); break;
    }
  valid = g_utf8_make_valid (text, -1);
  gtk_label_set_text (GTK_LABEL (state->status), valid);
  g_free (valid);
  g_free (error);
}

static void
painter_filter_describe (GString                           *text,
                         const GimpFilterArgumentsSnapshot *snapshot,
                         guint                              depth,
                         guint                             *remaining)
{
  guint i;
  for (i = 0; i < gimp_filter_arguments_snapshot_count (snapshot) && *remaining; i++)
    {
      GType type = gimp_filter_arguments_snapshot_type (snapshot, i);
      GValue value = G_VALUE_INIT;
      guint j;
      (*remaining)--;
      g_string_append_printf (text, "%*s%u: %s", depth * 2, "", i, g_type_name (type));
      if (gimp_filter_arguments_snapshot_is_null (snapshot, i))
        g_string_append (text, " = NULL");
      else if (gimp_filter_arguments_snapshot_value (snapshot, i, &value))
        {
          gchar *printed = g_strdup_value_contents (&value);
          g_string_append_printf (text, " = %s", printed);
          g_free (printed); g_value_unset (&value);
        }
      for (j = 0; j < gimp_filter_arguments_snapshot_reference_count (snapshot, i); j++)
        {
          GimpFilterArgumentReference ref;
          if (gimp_filter_arguments_snapshot_reference (snapshot, i, j, &ref))
            g_string_append_printf (text, " [%u: %s #%" G_GINT64_FORMAT " %s]", j,
                                    g_type_name (ref.object_type), ref.id,
                                    ! ref.was_set ? "unset" : ref.expired ? "expired" : "saved");
        }
      g_string_append_c (text, '\n');
      if (type == GIMP_TYPE_VALUE_ARRAY)
        {
          GimpFilterArgumentsSnapshot *nested = gimp_filter_arguments_snapshot_nested (snapshot, i);
          if (nested)
            { painter_filter_describe (text, nested, depth + 1, remaining); gimp_filter_arguments_snapshot_free (nested); }
        }
    }
}

static void
painter_filter_details (PainterLayerDialog *state,
                        GimpFilterLayer    *layer)
{
  gchar                       *procedure = gimp_filter_layer_dup_procedure (layer);
  GBytes                      *raw = gimp_filter_layer_ref_definition (layer);
  GimpFilterArgumentsSnapshot *snapshot = gimp_filter_layer_snapshot_arguments (layer);
  GString                     *text = g_string_new (NULL);
  GtkWidget                   *expander;
  GtkWidget                   *scroll;
  GtkWidget                   *view;
  gchar                       *valid;
  guint                        remaining = 512;
  g_string_append_printf (text, _("Procedure: %s\nOriginal metadata: %" G_GSIZE_FORMAT " bytes (preserved)\n"),
                          procedure ? procedure : "", raw ? g_bytes_get_size (raw) : 0);
  if (snapshot) painter_filter_describe (text, snapshot, 0, &remaining);
  else g_string_append (text, _("Arguments remain in the original metadata.\n"));
  if (! remaining) g_string_append (text, _("Further arguments are preserved but omitted from this preview.\n"));
  if (raw && g_bytes_get_size (raw))
    {
      gsize size, i;
      const guchar *bytes = g_bytes_get_data (raw, &size);
      g_string_append (text, _("\nOriginal metadata (hex preview):\n"));
      for (i = 0; i < MIN (size, 4096); i++)
        g_string_append_printf (text, "%02x%s", bytes[i], i % 16 == 15 ? "\n" : " ");
      if (size > 4096) g_string_append (text, _("\nRemaining bytes are preserved.\n"));
    }
  expander = gtk_expander_new (_("Saved Definition"));
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (scroll, 420, 160);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  view = gtk_text_view_new ();
  gtk_widget_set_name (view, "painter-filter-definition");
  gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
  /* Legacy metadata is byte-preserving and need not be valid UTF-8. Repair
   * only this presentation copy, never the saved procedure/arguments/raw data. */
  valid = g_utf8_make_valid (text->str, -1);
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), valid, -1);
  g_free (valid);
  gtk_container_add (GTK_CONTAINER (scroll), view);
  gtk_container_add (GTK_CONTAINER (expander), scroll);
  gtk_grid_attach (GTK_GRID (state->grid), expander, 0, state->row++, 2, 1);
  g_string_free (text, TRUE);
  g_free (procedure);
  g_clear_pointer (&raw, g_bytes_unref);
  gimp_filter_arguments_snapshot_free (snapshot);
}

static gboolean
painter_filter_number (const GValue *value,
                       gdouble      min,
                       gdouble      max,
                       gdouble     *out)
{
  if (G_VALUE_HOLDS_DOUBLE (value)) *out = g_value_get_double (value);
  else if (G_VALUE_HOLDS_FLOAT (value)) *out = g_value_get_float (value);
  else return FALSE;
  return isfinite (*out) && *out >= min && *out <= max;
}

/* Keep unsupported shapes selected as "keep", including known procedure names
 * with future/unknown arguments. Merely opening and accepting never normalizes. */
static void
painter_filter_load (PainterLayerDialog *state,
                     GimpFilterLayer    *layer)
{
  gchar          *procedure = gimp_filter_layer_dup_procedure (layer);
  GimpValueArray *args = gimp_filter_layer_dup_args (layer);
  guint          count = args ? gimp_value_array_length (args) : 0;
  gdouble        first, second;
  const GValue  *a, *b, *c;
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice), "keep");
  if (count != 5 && count != 6) goto out;
  a = gimp_value_array_index (args, 3);
  b = gimp_value_array_index (args, 4);
  c = count == 6 ? gimp_value_array_index (args, 5) : NULL;
  if (! g_strcmp0 (procedure, "plug-in-edge") && painter_filter_number (a, 0, 10, &first) &&
      G_VALUE_HOLDS_INT (b) && g_value_get_int (b) >= 1 && g_value_get_int (b) <= 3 &&
      (! c || (G_VALUE_HOLDS_INT (c) && g_value_get_int (c) >= 0 && g_value_get_int (c) <= 5)))
    {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (state->amount), first);
      gtk_combo_box_set_active (GTK_COMBO_BOX (state->wrap), g_value_get_int (b) - 1);
      gtk_combo_box_set_active (GTK_COMBO_BOX (state->edge), c ? g_value_get_int (c) : 0);
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice), "edge");
    }
  else if (! g_strcmp0 (procedure, "plug-in-gauss") && count == 6 &&
           painter_filter_number (a, 0, 500, &first) && painter_filter_number (b, 0, 500, &second) &&
           (first > 0 || second > 0) && G_VALUE_HOLDS_INT (c) &&
           g_value_get_int (c) >= 0 && g_value_get_int (c) <= 1)
    {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (state->horizontal), first);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (state->vertical), second);
      gtk_combo_box_set_active (GTK_COMBO_BOX (state->method), g_value_get_int (c));
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice), "gauss");
    }
out:
  g_free (procedure);
  g_clear_pointer (&args, gimp_value_array_unref);
}

static void
painter_filter_response (GtkWidget *dialog,
                         gint       response,
                         gpointer   data)
{
  PainterLayerDialog *state = data;
  GimpImage         *image = NULL;
  GimpLayer         *layer = NULL;
  GimpValueArray    *args = NULL;
  GError            *error = NULL;
  const gchar       *choice;
  const gchar       *procedure;
  GBytes            *raw = NULL;
  gboolean           success = TRUE;

  g_object_ref (dialog);
  if (response != GTK_RESPONSE_OK)
    goto close;
  image = g_weak_ref_get (&state->image);
  layer = g_weak_ref_get (&state->layer);
  if (state->closed || ! image || (state->editing &&
      (! layer || gimp_item_get_image (GIMP_ITEM (layer)) != image ||
       ! gimp_item_is_attached (GIMP_ITEM (layer)))))
    goto close;
  choice = gtk_combo_box_get_active_id (GTK_COMBO_BOX (state->choice));
  if (state->editing && (! state->dirty || ! choice || g_str_equal (choice, "keep")))
    goto close;
  if (! choice || g_str_equal (choice, "keep"))
    procedure = "";
  else if (g_str_equal (choice, "edge"))
    {
      procedure = "plug-in-edge";
      args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 1,
        G_TYPE_INT, gimp_image_get_id (image), G_TYPE_INT, 0,
        G_TYPE_DOUBLE, gtk_spin_button_get_value (GTK_SPIN_BUTTON (state->amount)),
        G_TYPE_INT, gtk_combo_box_get_active (GTK_COMBO_BOX (state->wrap)) + 1,
        G_TYPE_INT, gtk_combo_box_get_active (GTK_COMBO_BOX (state->edge)), G_TYPE_NONE);
    }
  else
    {
      gdouble horizontal = gtk_spin_button_get_value (GTK_SPIN_BUTTON (state->horizontal));
      gdouble vertical = gtk_spin_button_get_value (GTK_SPIN_BUTTON (state->vertical));
      if (horizontal <= 0 && vertical <= 0)
        {
          painter_dialog_error (state, _("At least one blur radius must be greater than zero."));
          goto out;
        }
      procedure = "plug-in-gauss";
      args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 1,
        G_TYPE_INT, gimp_image_get_id (image), G_TYPE_INT, 0,
        G_TYPE_DOUBLE, horizontal, G_TYPE_DOUBLE, vertical,
        G_TYPE_INT, gtk_combo_box_get_active (GTK_COMBO_BOX (state->method)), G_TYPE_NONE);
    }
  if (state->editing)
    {
      gchar *old_procedure = gimp_filter_layer_dup_procedure (GIMP_FILTER_LAYER (layer));
      raw = gimp_filter_layer_ref_definition (GIMP_FILTER_LAYER (layer));
      if (! g_strcmp0 (old_procedure, procedure))
        {
          GimpValueArray *old_args = gimp_filter_layer_dup_args (GIMP_FILTER_LAYER (layer));
          guint count = old_args ? gimp_value_array_length (old_args) : 0;
          /* Preserve the exact existing array shape/types when it is supported,
           * including float arguments and edge's historical five-slot form. */
          if (args && (count == 6 || (g_str_equal (choice, "edge") && count == 5)))
            {
              gboolean compatible = TRUE;
              for (guint i = 3; i < count; i++)
                {
                  const GValue *old = gimp_value_array_index (old_args, i);
                  const GValue *current = gimp_value_array_index (args, i);
                  if (G_VALUE_HOLDS_DOUBLE (current) ?
                      ! (G_VALUE_HOLDS_DOUBLE (old) || G_VALUE_HOLDS_FLOAT (old)) :
                      ! G_VALUE_HOLDS_INT (old))
                    compatible = FALSE;
                }
              if (compatible)
                {
                  for (guint i = 3; i < count; i++)
                    {
                      GValue *old = gimp_value_array_index (old_args, i);
                      const GValue *current = gimp_value_array_index (args, i);
                      if (G_VALUE_HOLDS_FLOAT (old))
                        g_value_set_float (old, g_value_get_double (current));
                      else if (G_VALUE_HOLDS_DOUBLE (old))
                        g_value_set_double (old, g_value_get_double (current));
                      else g_value_set_int (old, g_value_get_int (current));
                    }
                  /* Selecting a non-Sobel algorithm explicitly extends the
                   * five-slot legacy form; retaining Sobel leaves it intact. */
                  if (count == 5 && gtk_combo_box_get_active (GTK_COMBO_BOX (state->edge)) != 0)
                    gimp_value_array_append (old_args, gimp_value_array_index (args, 5));
                  gimp_value_array_unref (args);
                  args = old_args;
                  old_args = NULL;
                }
            }
          g_clear_pointer (&old_args, gimp_value_array_unref);
        }
      g_free (old_procedure);
      success = gimp_filter_layer_edit_definition (GIMP_FILTER_LAYER (layer), procedure, raw, args, &error);
    }
  else
    {
      GimpLayerMode mode;
      gimp_painter_layer_mode_from_legacy (24, &mode);
      layer = gimp_filter_layer_new (image,
        gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (state->width)),
        gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (state->height)),
        gtk_entry_get_text (GTK_ENTRY (state->name)), 1.0, mode);
      if (layer)
        {
          g_object_ref_sink (layer);
          if (state->closed) goto close;
          success = gimp_filter_layer_set_definition (GIMP_FILTER_LAYER (layer), procedure, NULL, args, &error);
          if (state->closed) goto close;
          if (success)
            success = gimp_image_add_layer (image, layer, GIMP_IMAGE_ACTIVE_PARENT, -1, TRUE);
        }
      else success = FALSE;
    }
  if (state->closed) goto close;
  if (! success)
    {
      painter_dialog_error (state, error ? error->message : _("The filter layer could not be created or changed."));
      goto out;
    }
  gimp_image_flush (image);
close:
  gtk_widget_destroy (dialog);
out:
  g_clear_pointer (&raw, g_bytes_unref);
  g_clear_pointer (&args, gimp_value_array_unref);
  g_clear_error (&error);
  g_clear_object (&image);
  g_clear_object (&layer);
  g_object_unref (dialog);
}

GtkWidget *
painter_filter_layer_dialog_new (GimpImage   *image,
                                 GimpLayer   *layer,
                                 GimpContext *context,
                                 GtkWidget   *parent)
{
  PainterLayerDialog *state;
  GtkWidget         *main_grid;
  GtkWidget         *grid;
  gint               row;
  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (layer == NULL || GIMP_IS_FILTER_LAYER (layer), NULL);
  state = painter_dialog_new (image, layer, context, parent,
                              layer ? _("Edit Filter Layer") : _("New Filter Layer"),
                              "gimp-painter-filter-layer",
                              _("Filter the visible layers below this layer"));
  if (! layer)
    {
      state->name = painter_dialog_field (state, _("_Name:"), gtk_entry_new (), "painter-filter-name");
      gtk_entry_set_text (GTK_ENTRY (state->name), _("Filter Layer"));
      gtk_entry_set_activates_default (GTK_ENTRY (state->name), TRUE);
      state->width = painter_dialog_spin (state, _("_Width (pixels):"), "painter-filter-width", 1, GIMP_MAX_IMAGE_SIZE, gimp_image_get_width (image), 0);
      state->height = painter_dialog_spin (state, _("_Height (pixels):"), "painter-filter-height", 1, GIMP_MAX_IMAGE_SIZE, gimp_image_get_height (image), 0);
    }
  state->choice = painter_dialog_field (state, _("_Filter:"), gtk_combo_box_text_new (), "painter-filter-choice");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice), "keep", layer ? _("Keep saved definition") : _("No filter (configure later)"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice), "edge", _("Edge Detect (Painter compatibility)"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice), "gauss", _("Gaussian Blur (Painter compatibility)"));
  state->parameters = gtk_stack_new ();
  gtk_stack_set_homogeneous (GTK_STACK (state->parameters), FALSE);
  gtk_grid_attach (GTK_GRID (state->grid), state->parameters, 0, state->row++, 2, 1);
  grid = gtk_label_new (_("Saved procedures and arguments remain intact. Only listed compatibility filters can be executed here."));
  gtk_label_set_line_wrap (GTK_LABEL (grid), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (grid), 60);
  gtk_stack_add_named (GTK_STACK (state->parameters), grid, "keep");
  main_grid = state->grid; row = state->row;
  state->grid = grid = gtk_grid_new (); state->row = 0;
  gtk_grid_set_row_spacing (GTK_GRID (grid), 8);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 8);
  gtk_stack_add_named (GTK_STACK (state->parameters), grid, "edge");
  state->amount = painter_dialog_spin (state, _("_Amount:"), "painter-edge-amount", 0, 10, 2, 3);
  state->wrap = painter_dialog_field (state, _("_Border:"), gtk_combo_box_text_new (), "painter-edge-wrap");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->wrap), _("Wrap"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->wrap), _("Smear"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->wrap), _("Black"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (state->wrap), 1);
  state->edge = painter_dialog_field (state, _("_Algorithm:"), gtk_combo_box_text_new (), "painter-edge-method");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->edge), "Sobel");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->edge), "Prewitt");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->edge), "Gradient");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->edge), "Roberts");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->edge), _("Differential"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->edge), "Laplace");
  gtk_combo_box_set_active (GTK_COMBO_BOX (state->edge), 0);
  state->grid = grid = gtk_grid_new (); state->row = 0;
  gtk_grid_set_row_spacing (GTK_GRID (grid), 8);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 8);
  gtk_stack_add_named (GTK_STACK (state->parameters), grid, "gauss");
  state->horizontal = painter_dialog_spin (state, _("_Horizontal radius:"), "painter-gauss-horizontal", 0, 500, 5, 3);
  state->vertical = painter_dialog_spin (state, _("_Vertical radius:"), "painter-gauss-vertical", 0, 500, 5, 3);
  state->method = painter_dialog_field (state, _("_Method:"), gtk_combo_box_text_new (), "painter-gauss-method");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->method), "IIR");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->method), "RLE");
  gtk_combo_box_set_active (GTK_COMBO_BOX (state->method), 0);
  state->grid = main_grid; state->row = row;
  state->status = painter_dialog_field (state, NULL, gtk_label_new (NULL), "painter-filter-status");
  gtk_label_set_line_wrap (GTK_LABEL (state->status), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (state->status), 60);
  gtk_label_set_xalign (GTK_LABEL (state->status), 0);
  gtk_widget_show_all (state->grid);
  if (layer)
    {
      painter_filter_load (state, GIMP_FILTER_LAYER (layer));
      painter_filter_details (state, GIMP_FILTER_LAYER (layer));
      painter_filter_status (GIMP_FILTER_LAYER (layer), state->dialog);
      g_signal_connect_object (layer, "filter-state-changed", G_CALLBACK (painter_filter_status), state->dialog, 0);
    }
  else
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice), "keep");
  painter_filter_choice_changed (state->choice, state->dialog);
  state->dirty = FALSE;
  /* A retained/reparented child can outlive the dialog. The object closure
   * disconnects at owner disposal and leases the dialog during each callback. */
  g_signal_connect_object (state->choice, "changed", G_CALLBACK (painter_filter_choice_changed), state->dialog, 0);
  g_signal_connect_object (state->amount, "value-changed", G_CALLBACK (painter_filter_dirty), state->dialog, 0);
  g_signal_connect_object (state->wrap, "changed", G_CALLBACK (painter_filter_dirty), state->dialog, 0);
  g_signal_connect_object (state->edge, "changed", G_CALLBACK (painter_filter_dirty), state->dialog, 0);
  g_signal_connect_object (state->horizontal, "value-changed", G_CALLBACK (painter_filter_dirty), state->dialog, 0);
  g_signal_connect_object (state->vertical, "value-changed", G_CALLBACK (painter_filter_dirty), state->dialog, 0);
  g_signal_connect_object (state->method, "changed", G_CALLBACK (painter_filter_dirty), state->dialog, 0);
  g_signal_connect (state->dialog, "response", G_CALLBACK (painter_filter_response), state);
  gtk_widget_show_all (state->grid);
  return state->dialog;
}
