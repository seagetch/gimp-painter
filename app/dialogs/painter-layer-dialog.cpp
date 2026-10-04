/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The Painter layers are independent document types, not drawable effects.
 * This editor only invokes explicitly supported compatibility executors. */
#include "config.h"
#include <math.h>
#include <errno.h>
#include <limits>
#include <string>
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
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
}
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/gimp-painter-binding.h"

using namespace GimpPainter;

namespace {

/* ASCII entries avoid GtkSpinButton's display rounding and silent clamping.
 * Each field patches only its own saved scalar/array element. */
struct FilterNumberField
{
  GtkWidget *entry;
  std::string route;
  guint argument;
  gint element; // -1: scalar, otherwise an element in a typed array
  bool integer;
  gdouble minimum, maximum;
  std::string initial;
};

struct FilterFlagField
{
  GtkWidget *widget;
  std::string route;
  guint argument;
  gint element;
  bool initial;
};

struct FilterEnumField
{
  GtkWidget *widget;
  std::string route;
  guint argument;
  gint initial;
};

/* The native dialog owns exactly one typed implementation. Helpers borrow it
 * only inside initialize()/with(); no closure or qdata key owns a raw Impl. */
struct PainterLayerDialog
{
  GWeakRef   image;
  GWeakRef   layer;
  gboolean   editing = 0;
  gboolean   closed = 0;
  gboolean   dirty = 0;
  GtkWidget *dialog = nullptr;
  GtkWidget *grid = nullptr;
  GtkWidget *name = nullptr;
  GtkWidget *width = nullptr;
  GtkWidget *height = nullptr;
  GtkWidget *choice = nullptr;
  GtkWidget *parameters = nullptr;
  GtkWidget *amount = nullptr;
  GtkWidget *wrap = nullptr;
  GtkWidget *edge = nullptr;
  GtkWidget *horizontal = nullptr;
  GtkWidget *vertical = nullptr;
  GtkWidget *method = nullptr;
  GtkWidget *point_argument = nullptr;
  GtkWidget *radius = nullptr;
  GtkWidget *horizontal_flag = nullptr;
  GtkWidget *vertical_flag = nullptr;
  GtkWidget *status = nullptr;
  GtkWidget *error = nullptr;
  gint       row = 0;
  std::vector<Connection> connections;
  std::vector<FilterNumberField> numbers;
  std::vector<FilterFlagField> flags;
  std::vector<FilterEnumField> enums;
  std::string loaded_route;
  guint64 loaded_revision = 0;

  PainterLayerDialog (GtkWidget *owner, GimpImage *image, GimpLayer *layer)
    : editing (layer != nullptr), dialog (owner)
  {
    g_weak_ref_init (&this->image, image);
    g_weak_ref_init (&this->layer, layer);
  }
  ~PainterLayerDialog () noexcept
  {
    close ();
    g_weak_ref_clear (&image);
    g_weak_ref_clear (&layer);
  }
  void close () noexcept
  {
    if (closed) return;
    closed = TRUE;
    /* Invalidate before disconnecting: a callback already on the stack retains
     * its scoped store borrow, but must stop touching destroyed children. */
    auto old_connections = std::move (connections);
    g_weak_ref_set (&image, nullptr);
    g_weak_ref_set (&layer, nullptr);
  }
};

struct DialogSlot : SlotSpec<GObject, PainterLayerDialog>
{
  static GType owner_type () { return GIMP_TYPE_VIEWABLE_DIALOG; }
};

struct DialogHook
{
  WeakRef<GObject> owner;
  std::uint64_t generation;
  explicit DialogHook (GtkWidget *dialog)
    : owner (ObjectRef<GObject>::retain (G_OBJECT (dialog))),
      generation (BindingStore::require (G_OBJECT (dialog)).generation ()) {}
};

static void
painter_dialog_hook_free (gpointer data, GClosure *)
{
  delete static_cast<DialogHook *> (data);
}

static void
painter_dialog_connect (PainterLayerDialog *state,
                        gpointer            emitter,
                        const gchar        *signal,
                        GCallback           callback)
{
  auto hook = std::unique_ptr<DialogHook> (new DialogHook (state->dialog));
  auto connection = Connection::connect (ObjectRef<GObject>::retain (G_OBJECT (emitter)),
                                         signal, callback, hook.get (), painter_dialog_hook_free);
  hook.release (); // Connection now owns the hook even if vector growth throws.
  state->connections.push_back (std::move (connection));
}

template<class Function>
static void
painter_dialog_dispatch (gpointer data, Function&& function) noexcept
{
  boundary_void (nullptr, [&] {
    auto *hook = static_cast<DialogHook *> (data);
    auto owner = hook->owner.lock ();
    const auto generation = hook->generation;
    /* Copy all callback data before invoking UI/core code. close() can remove
     * this very closure while signal emission is still in progress. */
    if (!owner) return;
    auto *store = BindingStore::find (owner.get ());
    if (store && store->accepts (generation))
      store->with<DialogSlot> (std::forward<Function> (function));
  });
}

static void
painter_dialog_destroy (GtkWidget *dialog, gpointer)
{
  /* GtkWidget::destroy is the actual lifecycle boundary. GObject has no
   * generic dispose signal, and final qdata destruction is too late. */
  gimp_painter_binding_close (G_OBJECT (dialog), nullptr);
}

static void
painter_dialog_image_disconnect (GimpImage *, gpointer data)
{
  painter_dialog_dispatch (data, [] (PainterLayerDialog& state) {
    gtk_widget_destroy (state.dialog);
  });
}

static void
painter_dialog_error (PainterLayerDialog *state,
                      const gchar        *message)
{
  gchar *valid;
  if (state->closed) return;
  valid = g_utf8_make_valid (message, -1);
  auto error = ObjectRef<GObject>::retain (G_OBJECT (state->error));
  gtk_label_set_text (GTK_LABEL (error.get ()), valid);
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

template<class Initialize, class Activate>
static GtkWidget *
painter_dialog_new (GimpImage   *image,
                    GimpLayer   *layer,
                    GimpContext *context,
                    GtkWidget   *parent,
                    const gchar *title,
                    const gchar *role,
                    const gchar *description,
                    Initialize&& initialize,
                    Activate&&   activate) noexcept
{
  GtkWidget *dialog = nullptr;
  return boundary<GtkWidget *> (nullptr, nullptr, [&] {
    /* Native construction/property/show signals are reentrant too. An outside
     * callback may drop the caller's last input reference before new() returns. */
    auto image_lease = ObjectRef<GObject>::retain (G_OBJECT (image));
    auto layer_lease = ObjectRef<GObject>::retain (G_OBJECT (layer));
    auto context_lease = ObjectRef<GObject>::retain (G_OBJECT (context));
    auto parent_lease = ObjectRef<GObject>::retain (G_OBJECT (parent));
    auto release_inputs = [&] {
      parent_lease.reset ();
      context_lease.reset ();
      layer_lease.reset ();
      image_lease.reset ();
    };
    dialog = gimp_viewable_dialog_new (
      g_list_prepend (NULL, layer ? GIMP_VIEWABLE (layer) : GIMP_VIEWABLE (image)),
      context, title, role, GIMP_ICON_LAYER, description, parent,
      gimp_standard_help_func, GIMP_HELP_LAYER_EDIT,
      _("_Cancel"), GTK_RESPONSE_CANCEL, _("_OK"), GTK_RESPONSE_OK, NULL);
    auto owner = ObjectRef<GObject>::retain (G_OBJECT (dialog));
    try
      {
        auto& store = BindingStore::ensure (owner.get ());
        store.emplace<DialogSlot> (dialog, image, layer);
        store.initialize<DialogSlot> ([&] (PainterLayerDialog& value) {
          auto *state = &value;
          GtkWidget *content;
          state->connections.push_back (Connection::connect (owner, "destroy",
            G_CALLBACK (painter_dialog_destroy), nullptr, nullptr));
          painter_dialog_connect (state, image, "disconnect", G_CALLBACK (painter_dialog_image_disconnect));
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
          initialize (value);
        });
        store.activate ();
        store.with<DialogSlot> (std::forward<Activate> (activate));
        /* Input release can itself disconnect the image or destroy the parent.
         * Keep the dialog lease until this final lifecycle check has finished. */
        release_inputs ();
        if (store.state () != BindingStore::State::active)
          {
            gtk_widget_destroy (dialog);
            return static_cast<GtkWidget *> (nullptr);
          }
        return dialog;
      }
    catch (...)
      {
        gtk_widget_destroy (dialog);
        release_inputs ();
        throw;
      }
  });
}

static void
painter_clone_refresh (PainterLayerDialog *state)
{
  GimpImage         *image = GIMP_IMAGE (g_weak_ref_get (&state->image));
  GimpLayer         *layer = GIMP_LAYER (g_weak_ref_get (&state->layer));
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
      GimpItem *item = GIMP_ITEM (iter->data);
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
painter_clone_response (PainterLayerDialog *state,
                        gint                response)
{
  GtkWidget         *dialog = state->dialog;
  GimpImage         *image = NULL;
  GimpLayer         *layer = NULL;
  GimpItem          *source = NULL;
  const gchar       *id;
  GError            *error = NULL;
  gboolean           success;

  /* The dispatcher/store lease keeps this borrow alive across core reentry. */
  if (response != GTK_RESPONSE_OK)
    goto close;
  image = GIMP_IMAGE (g_weak_ref_get (&state->image));
  layer = GIMP_LAYER (g_weak_ref_get (&state->layer));
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
}

static void
painter_clone_refresh_clicked (GtkButton *, gpointer data)
{
  painter_dialog_dispatch (data, [] (PainterLayerDialog& state) {
    painter_clone_refresh (&state);
  });
}

static void
painter_clone_response_received (GtkDialog *, gint response, gpointer data)
{
  painter_dialog_dispatch (data, [&] (PainterLayerDialog& state) {
    painter_clone_response (&state, response);
  });
}

} // namespace

GtkWidget *
painter_clone_layer_dialog_new (GimpImage   *image,
                                GimpLayer   *layer,
                                GimpContext *context,
                                GtkWidget   *parent)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (GIMP_IS_CLONE_LAYER (layer), NULL);
  return painter_dialog_new (image, layer, context, parent,
    _("Clone Layer Source"), "gimp-clone-layer-source",
    _("Choose the layer or group this clone follows"),
    [] (PainterLayerDialog& value) {
      auto *state = &value;
      state->choice = painter_dialog_field (state, _("_Source:"), gtk_combo_box_text_new (), "painter-clone-source");
      GtkWidget *refresh = painter_dialog_field (state, NULL, gtk_button_new_with_mnemonic (_("_Refresh Sources")), "painter-clone-refresh");
      painter_dialog_connect (state, refresh, "clicked", G_CALLBACK (painter_clone_refresh_clicked));
      painter_dialog_connect (state, state->dialog, "response", G_CALLBACK (painter_clone_response_received));
    },
    [] (PainterLayerDialog& state) {
      painter_clone_refresh (&state);
      if (!state.closed) gtk_widget_show_all (state.grid);
    });
}

namespace {
static void
painter_filter_dirty (GtkWidget *, gpointer data)
{
  painter_dialog_dispatch (data, [] (PainterLayerDialog& state) {
    state.dirty = TRUE;
  });
}

static void
painter_filter_choice_changed (PainterLayerDialog *state)
{
  const gchar       *id;
  if (state->closed) return;
  id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (state->choice));
  /* Stack/sensitivity signals can synchronously change the selection. Keep
   * its string independently alive across those reentrant widget calls. */
  const std::string selection = id ? id : "keep";
  id = selection.c_str ();
  auto parameters = ObjectRef<GObject>::retain (G_OBJECT (state->parameters));
  const bool flags = id && (!strcmp (id,"gauss-iir") || !strcmp (id,"gauss-rle"));
  const bool fixed = id && g_str_has_prefix (id,"gauss-");
  const bool point = id && (!strcmp (id,"vinvert") || !strcmp (id,"max-rgb") || !strcmp (id,"threshold-alpha"));
  gtk_stack_set_visible_child_name (GTK_STACK (parameters.get ()),
    point ? "point" : flags ? "gauss-flags" : fixed ? "gauss" : id ? id : "keep");
  if (state->closed || g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (state->choice)),id)) return;
  gtk_widget_set_sensitive (state->method, !fixed);
  if (state->closed || g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (state->choice)),id)) return;
  gtk_widget_set_sensitive (state->point_argument, point && strcmp (id,"vinvert"));
  if (!state->closed) state->dirty = TRUE;
}

static void
painter_filter_status (PainterLayerDialog *state,
                       GimpFilterLayer    *layer)
{
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
  auto status = ObjectRef<GObject>::retain (G_OBJECT (state->status));
  gtk_label_set_text (GTK_LABEL (status.get ()), valid);
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
      else
        {
          const GValue *contents = gimp_filter_arguments_snapshot_peek_value (snapshot, i);
          if (!contents && gimp_filter_arguments_snapshot_value (snapshot, i, &value)) contents = &value;
          if (contents)
            {
              if (type == GIMP_TYPE_DOUBLE_ARRAY || type == GIMP_TYPE_INT32_ARRAY)
                {
                  const auto *array = static_cast<const GimpArray *> (g_value_get_boxed (contents));
                  if (!array) g_string_append (text, " = NULL");
                  else
                    {
                      const gsize size = type == GIMP_TYPE_DOUBLE_ARRAY ? sizeof (gdouble) : sizeof (gint32);
                      g_string_append_printf (text, " (%" G_GSIZE_FORMAT " bytes) = [", array->length);
                      for (gsize at = 0; array->data && at + size <= array->length && at / size < 256; at += size)
                        {
                          if (at) g_string_append (text, ", ");
                          if (type == GIMP_TYPE_DOUBLE_ARRAY)
                            {
                              gdouble number; gchar printed[G_ASCII_DTOSTR_BUF_SIZE];
                              memcpy (&number, array->data + at, sizeof number);
                              g_string_append (text, g_ascii_dtostr (printed, sizeof printed, number));
                            }
                          else
                            {
                              gint32 number; memcpy (&number, array->data + at, sizeof number);
                              g_string_append_printf (text, "%" G_GINT32_FORMAT, number);
                            }
                        }
                      if (array->length / size > 256) g_string_append (text, ", …");
                      if (array->length % size) g_string_append (text, "; incomplete trailing element retained");
                      g_string_append_c (text, ']');
                    }
                }
              else
                {
                  gchar *printed = g_strdup_value_contents (contents);
                  g_string_append_printf (text, " = %s", printed);
                  g_free (printed);
                }
            }
          if (G_IS_VALUE (&value)) g_value_unset (&value);
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
      const guchar *bytes = static_cast<const guchar *> (g_bytes_get_data (raw, &size));
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

static bool
painter_filter_isolated (const gchar *choice)
{
  return choice && (!strcmp (choice, "blinds") || !strcmp (choice, "small-tiles") ||
                    !strcmp (choice, "retinex") || !strcmp (choice, "convmatrix"));
}

static void
painter_filter_entry_value (FilterNumberField& field, gdouble value)
{
  gchar text[G_ASCII_DTOSTR_BUF_SIZE];
  g_ascii_dtostr (text, sizeof text, value);
  field.initial = text;
  gtk_entry_set_text (GTK_ENTRY (field.entry), text);
}

static GtkWidget *
painter_filter_entry (PainterLayerDialog *state, const gchar *route,
                      guint argument, gint element, bool integer,
                      const gchar *label, const gchar *name,
                      gdouble minimum, gdouble maximum, gdouble value,
                      bool attach = true)
{
  GtkWidget *entry = gtk_entry_new ();
  gtk_widget_set_name (entry, name);
  gtk_entry_set_width_chars (GTK_ENTRY (entry), element >= 0 ? 8 : 22);
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  state->numbers.push_back ({entry, route, argument, element, integer, minimum, maximum, ""});
  painter_filter_entry_value (state->numbers.back (), value);
  if (attach) painter_dialog_field (state, label, entry, name);
  return entry;
}

static bool
painter_filter_flag_active (const FilterFlagField& field)
{
  return GTK_IS_COMBO_BOX (field.widget) ? gtk_combo_box_get_active (GTK_COMBO_BOX (field.widget)) == 1 :
                                         gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (field.widget));
}

static GtkWidget *
painter_filter_flag (PainterLayerDialog *state, const gchar *route, guint argument, gint element,
                     const gchar *label, const gchar *name, bool initial, bool attach = true)
{
  GtkWidget *widget = gtk_check_button_new_with_mnemonic (label);
  gtk_widget_set_name (widget, name);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), initial);
  state->flags.push_back ({widget, route, argument, element, initial});
  if (attach) painter_dialog_field (state, NULL, widget, name);
  return widget;
}

static gint
painter_filter_enum_value (const FilterEnumField& field)
{
  const gchar *id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (field.widget));
  return id && strcmp (id, "saved") ? static_cast<gint> (g_ascii_strtoll (id, NULL, 10)) : field.initial;
}

static void
painter_filter_enum (PainterLayerDialog *state, const gchar *route, guint argument,
                    const gchar *label, const gchar *name, const gchar *const labels[3], gint initial)
{
  GtkWidget *widget = painter_dialog_field (state, label, gtk_combo_box_text_new (), name);
  for (guint i = 0; i < 3; ++i)
    {
      gchar *id = g_strdup_printf ("%u", i);
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), id, labels[i]);
      g_free (id);
    }
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), initial);
  state->enums.push_back ({widget, route, argument, initial});
}

static bool
painter_filter_isolated_shape (const gchar *route, const GimpFilterArgumentsSnapshot *args)
{
  const guint count = args ? gimp_filter_arguments_snapshot_count (args) : 0;
  const auto integer = [&] (guint i) { return i < count && G_VALUE_HOLDS_INT (gimp_filter_arguments_snapshot_peek_value (args, i)); };
  if (!integer (0) || !integer (1) || !integer (2)) return false;
  if (!strcmp (route, "blinds"))
    return count == 7 && integer (3) && integer (4) && integer (5) && integer (6);
  if (!strcmp (route, "small-tiles")) return count == 4 && integer (3);
  if (!strcmp (route, "retinex"))
    return count == 7 && integer (3) && integer (4) && integer (5) &&
           G_VALUE_HOLDS_DOUBLE (gimp_filter_arguments_snapshot_peek_value (args, 6));
  if (strcmp (route, "convmatrix") || (count != 11 && count != 12) ||
      !integer (3) || !integer (5) || !integer (8) || !integer (10) ||
      g_value_get_int (gimp_filter_arguments_snapshot_peek_value (args, 3)) != 25 ||
      g_value_get_int (gimp_filter_arguments_snapshot_peek_value (args, 8)) != 5 ||
      !G_VALUE_HOLDS (gimp_filter_arguments_snapshot_peek_value (args, 4), GIMP_TYPE_DOUBLE_ARRAY) ||
      !G_VALUE_HOLDS_DOUBLE (gimp_filter_arguments_snapshot_peek_value (args, 6)) ||
      !G_VALUE_HOLDS_DOUBLE (gimp_filter_arguments_snapshot_peek_value (args, 7)) ||
      !G_VALUE_HOLDS (gimp_filter_arguments_snapshot_peek_value (args, 9), GIMP_TYPE_INT32_ARRAY)) return false;
  const auto *matrix = static_cast<const GimpArray *> (g_value_get_boxed (gimp_filter_arguments_snapshot_peek_value (args, 4)));
  const auto *channels = static_cast<const GimpArray *> (g_value_get_boxed (gimp_filter_arguments_snapshot_peek_value (args, 9)));
  return matrix && matrix->data && matrix->length == 25 * sizeof (gdouble) &&
         channels && channels->data && channels->length == 5 * sizeof (gint32);
}

static void
painter_filter_isolated_load (PainterLayerDialog *state, const gchar *route,
                             const GimpFilterArgumentsSnapshot *args)
{
  for (auto& field : state->numbers)
    if (field.route == route)
      {
        const GValue *value = gimp_filter_arguments_snapshot_peek_value (args, field.argument);
        gdouble number;
        if (field.element < 0)
          number = field.integer ? g_value_get_int (value) : g_value_get_double (value);
        else
          {
            const auto *array = static_cast<const GimpArray *> (g_value_get_boxed (value));
            if (field.integer)
              { gint32 v; memcpy (&v, array->data + field.element * sizeof v, sizeof v); number = v; }
            else memcpy (&number, array->data + field.element * sizeof number, sizeof number);
          }
        painter_filter_entry_value (field, number);
      }
  for (auto& field : state->flags)
    if (field.route == route)
      {
        const GValue *value = gimp_filter_arguments_snapshot_peek_value (args, field.argument);
        gint flag;
        if (field.element < 0) flag = g_value_get_int (value);
        else
          {
            const auto *array = static_cast<const GimpArray *> (g_value_get_boxed (value));
            memcpy (&flag, array->data + field.element * sizeof (gint32), sizeof (gint32));
          }
        field.initial = GTK_IS_COMBO_BOX (field.widget) ? flag == 1 : flag != 0;
        if (GTK_IS_COMBO_BOX (field.widget)) gtk_combo_box_set_active (GTK_COMBO_BOX (field.widget), field.initial ? 1 : 0);
        else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (field.widget), field.initial);
      }
  for (auto& field : state->enums)
    if (field.route == route)
      {
        field.initial = g_value_get_int (gimp_filter_arguments_snapshot_peek_value (args, field.argument));
        if (field.initial >= 0 && field.initial <= 2)
          gtk_combo_box_set_active (GTK_COMBO_BOX (field.widget), field.initial);
        else
          {
            gchar *label = g_strdup_printf (_("Saved value: %d (unsupported)"), field.initial);
            gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (field.widget), "saved", label);
            gtk_combo_box_set_active_id (GTK_COMBO_BOX (field.widget), "saved");
            g_free (label);
          }
      }
  state->loaded_route = route;
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice), route);
}

static GimpValueArray *
painter_filter_isolated_defaults (const gchar *route, GimpImage *image)
{
  if (!strcmp (route, "blinds"))
    return gimp_value_array_new_from_types (NULL, G_TYPE_INT, 1, G_TYPE_INT, gimp_image_get_id (image),
      G_TYPE_INT, 0, G_TYPE_INT, 30, G_TYPE_INT, 3, G_TYPE_INT, 0, G_TYPE_INT, 0, G_TYPE_NONE);
  if (!strcmp (route, "small-tiles"))
    return gimp_value_array_new_from_types (NULL, G_TYPE_INT, 1, G_TYPE_INT, gimp_image_get_id (image),
      G_TYPE_INT, 0, G_TYPE_INT, 2, G_TYPE_NONE);
  if (!strcmp (route, "retinex"))
    return gimp_value_array_new_from_types (NULL, G_TYPE_INT, 1, G_TYPE_INT, gimp_image_get_id (image),
      G_TYPE_INT, 0, G_TYPE_INT, 240, G_TYPE_INT, 3, G_TYPE_INT, 0, G_TYPE_DOUBLE, 1.2, G_TYPE_NONE);
  GimpValueArray *args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 1, G_TYPE_INT, gimp_image_get_id (image),
    G_TYPE_INT, 0, G_TYPE_INT, 25, GIMP_TYPE_DOUBLE_ARRAY, NULL, G_TYPE_INT, 1,
    G_TYPE_DOUBLE, 1., G_TYPE_DOUBLE, 0., G_TYPE_INT, 5, GIMP_TYPE_INT32_ARRAY, NULL, G_TYPE_INT, 2, G_TYPE_NONE);
  gdouble matrix[25] = {}; matrix[12] = 1;
  const gint32 channels[5] = {1, 1, 1, 1, 1};
  gimp_value_set_double_array (gimp_value_array_index (args, 4), matrix, 25);
  gimp_value_set_int32_array (gimp_value_array_index (args, 9), channels, 5);
  return args;
}

static gboolean
painter_filter_isolated_args (PainterLayerDialog *state, const gchar *route,
                             GimpImage *image, GimpLayer *layer, GimpValueArray **result)
{
  const bool preserve = layer && state->loaded_route == route;
  if (preserve && state->loaded_revision != gimp_filter_layer_get_definition_revision (GIMP_FILTER_LAYER (layer)))
    {
      painter_dialog_error (state, _("This filter definition changed while the editor was open. Reopen the editor to use the latest values."));
      return FALSE;
    }
  GimpValueArray *args = preserve ? gimp_filter_layer_dup_args (GIMP_FILTER_LAYER (layer)) :
                                  painter_filter_isolated_defaults (route, image);
  if (preserve)
    {
      bool changed = false;
      for (const auto& field : state->numbers)
        if (field.route == route && field.initial != gtk_entry_get_text (GTK_ENTRY (field.entry))) changed = true;
      for (const auto& field : state->flags)
        if (field.route == route && field.initial != painter_filter_flag_active (field)) changed = true;
      for (const auto& field : state->enums)
        if (field.route == route && field.initial != painter_filter_enum_value (field)) changed = true;
      if (!changed) { gimp_value_array_unref (args); *result = NULL; return TRUE; }
    }
  bool changed = !preserve;
  const bool convolution = !strcmp (route, "convmatrix");
  const bool legacy = gimp_image_get_precision (image) == GIMP_PRECISION_U8_NON_LINEAR;
  gdouble matrix[25] = {};
  gint32 channels[5] = {};
  bool matrix_changed = false, channels_changed = false;
  if (convolution)
    {
      memcpy (matrix, gimp_value_get_double_array (gimp_value_array_index (args, 4), NULL), sizeof matrix);
      memcpy (channels, gimp_value_get_int32_array (gimp_value_array_index (args, 9), NULL), sizeof channels);
    }
  for (const auto& field : state->numbers)
    if (field.route == route)
      {
        const gchar *text = gtk_entry_get_text (GTK_ENTRY (field.entry));
        gchar *end = nullptr;
        errno = 0;
        gdouble number;
        if (field.integer)
          {
            const gint64 value = g_ascii_strtoll (text, &end, 10);
            number = value;
          }
        else number = g_ascii_strtod (text, &end);
        const bool consumed = end && end != text;
        while (end && g_ascii_isspace (*end)) ++end;
        /* ERANGE can also mean a representable double subnormal. A complete
         * finite nonzero result is usable; overflow/underflow-to-zero is not. */
        const bool range_error = errno == ERANGE && (field.integer || number == 0 || !isfinite (number));
        if (!consumed || *end || range_error || !isfinite (number) ||
            number < field.minimum || number > field.maximum ||
            (convolution && !field.integer && legacy && fabs (number) > std::numeric_limits<float>::max ()) ||
            (convolution && field.argument == 6 && (number == 0 || (legacy && static_cast<float> (number) == 0))))
          {
            painter_dialog_error (state, _("Enter a finite number within the indicated range. Use a dot for decimals; integer flags must be whole numbers. The divisor must be nonzero and representable at this image precision."));
            if (!state->closed) gtk_widget_grab_focus (field.entry);
            gimp_value_array_unref (args);
            return FALSE;
          }
        if (preserve && field.initial == text) continue;
        GValue *value = gimp_value_array_index (args, field.argument);
        if (preserve)
          {
            const gdouble old = field.element >= 0 ? (field.integer ? channels[field.element] : matrix[field.element]) :
              field.integer ? g_value_get_int (value) : g_value_get_double (value);
            if (!memcmp (&old, &number, sizeof number)) continue;
          }
        changed = true;
        if (field.element < 0)
          {
            if (field.integer) g_value_set_int (value, static_cast<gint> (number));
            else g_value_set_double (value, number);
          }
        else if (field.integer)
          { channels[field.element] = static_cast<gint32> (number); channels_changed = true; }
        else
          { matrix[field.element] = number; matrix_changed = true; }
      }
  for (const auto& field : state->flags)
    if (field.route == route)
      {
        const bool active = painter_filter_flag_active (field);
        /* A noncanonical stored integer has the same meaning as the native
         * control. Preserve it unless the user changes that meaning. */
        if (preserve && active == field.initial) continue;
        if (field.element < 0) g_value_set_int (gimp_value_array_index (args, field.argument), active ? 1 : 0);
        else { channels[field.element] = active ? 1 : 0; channels_changed = true; }
        changed = true;
      }
  for (const auto& field : state->enums)
    if (field.route == route)
      {
        const gint value = painter_filter_enum_value (field);
        if (preserve && value == field.initial) continue;
        g_value_set_int (gimp_value_array_index (args, field.argument), value);
        changed = true;
      }
  if (matrix_changed) gimp_value_set_double_array (gimp_value_array_index (args, 4), matrix, 25);
  if (channels_changed) gimp_value_set_int32_array (gimp_value_array_index (args, 9), channels, 5);
  if (!changed) { gimp_value_array_unref (args); args = NULL; }
  *result = args;
  return TRUE;
}

static void
painter_filter_isolated_pages (PainterLayerDialog *state)
{
  const auto page = [&] (const gchar *route) {
    state->grid = gtk_grid_new (); state->row = 0;
    gtk_grid_set_row_spacing (GTK_GRID (state->grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (state->grid), 6);
    gtk_stack_add_named (GTK_STACK (state->parameters), state->grid, route);
  };
  const auto help = [&] (const gchar *text, const gchar *name) {
    GtkWidget *label = gtk_label_new (text);
    gtk_label_set_xalign (GTK_LABEL (label), 0);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 65);
    gtk_widget_set_name (label, name);
    gtk_grid_attach (GTK_GRID (state->grid), label, 0, state->row++, 2, 1);
  };
  page ("blinds");
  painter_filter_entry (state, "blinds", 3, -1, true, _("_Angle (0–90):"), "painter-blinds-angle", 0, 90, 30);
  painter_filter_entry (state, "blinds", 4, -1, true, _("_Segments (1–100):"), "painter-blinds-segments", 1, 100, 3);
  GtkWidget *orientation = painter_dialog_field (state, _("_Orientation:"), gtk_combo_box_text_new (), "painter-blinds-orientation");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (orientation), "horizontal", _("Horizontal"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (orientation), "vertical", _("Vertical"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (orientation), 0);
  state->flags.push_back ({orientation, "blinds", 5, -1, false});
  painter_filter_flag (state, "blinds", 6, -1, _("_Transparent background"), "painter-blinds-transparent", false);
  help (_("Requires 8-bit non-linear RGB or grayscale.\nSaved legacy direction and transparency values are retained until their setting changes."), "painter-blinds-help");
  page ("small-tiles");
  painter_filter_entry (state, "small-tiles", 3, -1, true, _("_Tile factor (0–6):"), "painter-small-tiles-factor", 0, 6, 2);
  help (_("The saved legacy range includes 0 and 1.\nRequires 8-bit non-linear RGB or grayscale."), "painter-small-tiles-help");
  page ("retinex");
  painter_filter_entry (state, "retinex", 3, -1, true, _("_Scale (16–256):"), "painter-retinex-scale", 16, 256, 240);
  painter_filter_entry (state, "retinex", 4, -1, true, _("_Number of scales (0–8):"), "painter-retinex-nscales", 0, 8, 3);
  const gchar *distributions[] = {_("Uniform"), _("Low"), _("High")};
  painter_filter_enum (state, "retinex", 5, _("_Distribution:"), "painter-retinex-mode", distributions, 0);
  painter_filter_entry (state, "retinex", 6, -1, false, _("_Dynamic (0–4):"), "painter-retinex-cvar", 0, 4, 1.2);
  help (_("Zero scales is a supported legacy value.\nRequires 8-bit non-linear RGB and a selected region at least 16 × 16 pixels.\nUse a dot for decimals; saved precision is retained."), "painter-retinex-help");
  page ("convmatrix");
  help (_("5 × 5 coefficients, shown in image order (rows top to bottom, columns left to right).\nUse a dot for decimals; scientific notation is accepted."), "painter-convmatrix-matrix-help");
  GtkWidget *matrix = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (matrix), 4);
  gtk_grid_set_column_spacing (GTK_GRID (matrix), 4);
  gtk_grid_attach (GTK_GRID (state->grid), matrix, 0, state->row++, 2, 1);
  for (guint y = 0; y < 5; ++y)
    for (guint x = 0; x < 5; ++x)
      {
        const guint index = x * 5 + y; // The saved old array is x-major.
        gchar *name = g_strdup_printf ("painter-convmatrix-%u", index);
        GtkWidget *entry = painter_filter_entry (state, "convmatrix", 4, index, false, NULL, name,
                                                -G_MAXDOUBLE, G_MAXDOUBLE, index == 12 ? 1 : 0, false);
        gchar *tip = g_strdup_printf (_("Column %u, row %u (saved coefficient %u)"), x + 1, y + 1, index);
        gtk_widget_set_tooltip_text (entry, tip);
        atk_object_set_name (gtk_widget_get_accessible (entry), tip);
        gtk_grid_attach (GTK_GRID (matrix), entry, x, y, 1, 1);
        g_free (tip); g_free (name);
      }
  painter_filter_entry (state, "convmatrix", 6, -1, false, _("_Divisor (nonzero):"), "painter-convmatrix-divisor", -G_MAXDOUBLE, G_MAXDOUBLE, 1);
  painter_filter_entry (state, "convmatrix", 7, -1, false, _("_Offset (byte units):"), "painter-convmatrix-offset", -G_MAXDOUBLE, G_MAXDOUBLE, 0);
  const gchar *borders[] = {_("Extend"), _("Wrap"), _("Clear")};
  painter_filter_enum (state, "convmatrix", 10, _("_Border:"), "painter-convmatrix-border", borders, 2);
  GtkWidget *channels = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (channels), 4);
  gtk_grid_attach (GTK_GRID (state->grid), channels, 0, state->row++, 2, 1);
  const gchar *labels[] = {_("Gray"), _("Red"), _("Green"), _("Blue"), _("Alpha")};
  for (guint i = 0; i < 5; ++i)
    {
      gchar *name = g_strdup_printf ("painter-convmatrix-channel-%u", i);
      GtkWidget *entry = painter_filter_flag (state, "convmatrix", 9, i, labels[i], name, true, false);
      gtk_grid_attach (GTK_GRID (channels), entry, i, 0, 1, 1);
      g_free (name);
    }
  help (_("Select the channels to filter. Saved legacy channel values are retained until a setting changes.\nWithout alpha the legacy executor uses the Extend border.\nLegacy noninteractive execution disables alpha weighting; its saved argument is retained.\nCoefficients, divisor and offset must be finite; 8-bit non-linear images also require float range."), "painter-convmatrix-help");
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
  GimpValueArray *args = NULL;
  guint          count;
  gdouble        first, second;
  const GValue  *a, *b, *c;
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice), "keep");
  state->loaded_revision = gimp_filter_layer_get_definition_revision (layer);
  if (procedure && g_str_has_prefix (procedure, "plug-in-") &&
      painter_filter_isolated (procedure + strlen ("plug-in-")))
    {
      GimpFilterArgumentsSnapshot *snapshot = gimp_filter_layer_snapshot_arguments (layer);
      if (painter_filter_isolated_shape (procedure + strlen ("plug-in-"), snapshot))
        painter_filter_isolated_load (state, procedure + strlen ("plug-in-"), snapshot);
      gimp_filter_arguments_snapshot_free (snapshot);
      goto out;
    }
  args = gimp_filter_layer_dup_args (layer);
  count = args ? gimp_value_array_length (args) : 0;
  if ((!g_strcmp0 (procedure,"plug-in-vinvert") && count == 3) ||
      ((!g_strcmp0 (procedure,"plug-in-max-rgb") || !g_strcmp0 (procedure,"plug-in-threshold-alpha")) &&
       count == 4 && G_VALUE_HOLDS_INT (gimp_value_array_index (args,3))))
    {
      if (count == 4) gtk_spin_button_set_value (GTK_SPIN_BUTTON (state->point_argument),g_value_get_int (gimp_value_array_index (args,3)));
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice),procedure + strlen ("plug-in-"));
      goto out;
    }
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
           painter_filter_number (a, -500, 500, &first) && painter_filter_number (b, -500, 500, &second) &&
           (first > 0 || second > 0) && G_VALUE_HOLDS_INT (c) &&
           g_value_get_int (c) >= 0 && g_value_get_int (c) <= 1)
    {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (state->horizontal), first);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (state->vertical), second);
      gtk_combo_box_set_active (GTK_COMBO_BOX (state->method), g_value_get_int (c));
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice), "gauss");
    }
  else if ((!g_strcmp0 (procedure,"plug-in-gauss-iir") || !g_strcmp0 (procedure,"plug-in-gauss-rle")) &&
           count == 6 && painter_filter_number (a,0,500,&first) && first > 0 &&
           G_VALUE_HOLDS_INT (b) && G_VALUE_HOLDS_INT (c))
    {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (state->radius),first);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (state->horizontal_flag),g_value_get_int (b));
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (state->vertical_flag),g_value_get_int (c));
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice),procedure + strlen ("plug-in-"));
    }
  else if ((!g_strcmp0 (procedure,"plug-in-gauss-iir2") || !g_strcmp0 (procedure,"plug-in-gauss-rle2")) &&
           count == 5 && painter_filter_number (a,-500,500,&first) &&
           painter_filter_number (b,-500,500,&second) && (first > 0 || second > 0))
    {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (state->horizontal),first);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (state->vertical),second);
      gtk_combo_box_set_active (GTK_COMBO_BOX (state->method),!g_strcmp0 (procedure,"plug-in-gauss-rle2"));
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice),procedure + strlen ("plug-in-"));
    }
out:
  g_free (procedure);
  g_clear_pointer (&args, gimp_value_array_unref);
}

static void
painter_filter_response (PainterLayerDialog *state,
                         gint                response)
{
  GtkWidget         *dialog = state->dialog;
  GimpImage         *image = NULL;
  GimpLayer         *layer = NULL;
  GimpValueArray    *args = NULL;
  GError            *error = NULL;
  const gchar       *choice;
  const gchar       *procedure;
  GBytes            *raw = NULL;
  gboolean           success = TRUE;

  if (response != GTK_RESPONSE_OK)
    goto close;
  image = GIMP_IMAGE (g_weak_ref_get (&state->image));
  layer = GIMP_LAYER (g_weak_ref_get (&state->layer));
  if (state->closed || ! image || (state->editing &&
      (! layer || gimp_item_get_image (GIMP_ITEM (layer)) != image ||
       ! gimp_item_is_attached (GIMP_ITEM (layer)))))
    goto close;
  choice = gtk_combo_box_get_active_id (GTK_COMBO_BOX (state->choice));
  if (state->editing && (! state->dirty || ! choice || g_str_equal (choice, "keep")))
    goto close;
  if (! choice || g_str_equal (choice, "keep"))
    procedure = "";
  else if (painter_filter_isolated (choice))
    {
      procedure = !strcmp (choice, "blinds") ? "plug-in-blinds" :
                  !strcmp (choice, "small-tiles") ? "plug-in-small-tiles" :
                  !strcmp (choice, "retinex") ? "plug-in-retinex" : "plug-in-convmatrix";
      if (!painter_filter_isolated_args (state, choice, image, layer, &args)) goto out;
      if (!args) goto close;
    }
  else if (g_str_equal (choice, "edge"))
    {
      procedure = "plug-in-edge";
      args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 1,
        G_TYPE_INT, gimp_image_get_id (image), G_TYPE_INT, 0,
        G_TYPE_DOUBLE, gtk_spin_button_get_value (GTK_SPIN_BUTTON (state->amount)),
        G_TYPE_INT, gtk_combo_box_get_active (GTK_COMBO_BOX (state->wrap)) + 1,
        G_TYPE_INT, gtk_combo_box_get_active (GTK_COMBO_BOX (state->edge)), G_TYPE_NONE);
    }
  else if (g_str_equal (choice,"vinvert") || g_str_equal (choice,"max-rgb") || g_str_equal (choice,"threshold-alpha"))
    {
      procedure = g_str_equal (choice,"vinvert") ? "plug-in-vinvert" :
        g_str_equal (choice,"max-rgb") ? "plug-in-max-rgb" : "plug-in-threshold-alpha";
      args = gimp_value_array_new_from_types (NULL,G_TYPE_INT,1,G_TYPE_INT,gimp_image_get_id (image),G_TYPE_INT,0,
        G_TYPE_INT,gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (state->point_argument)),G_TYPE_NONE);
      if (g_str_equal (choice,"vinvert")) gimp_value_array_truncate (args,3);
    }
  else if (g_str_equal (choice,"gauss-iir") || g_str_equal (choice,"gauss-rle"))
    {
      const gdouble radius = gtk_spin_button_get_value (GTK_SPIN_BUTTON (state->radius));
      if (radius <= 0) { painter_dialog_error (state, _("The blur radius must be greater than zero.")); goto out; }
      procedure = g_str_equal (choice,"gauss-iir") ? "plug-in-gauss-iir" : "plug-in-gauss-rle";
      args = gimp_value_array_new_from_types (NULL,G_TYPE_INT,1,G_TYPE_INT,gimp_image_get_id (image),G_TYPE_INT,0,
        G_TYPE_DOUBLE,radius,G_TYPE_INT,gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (state->horizontal_flag)),
        G_TYPE_INT,gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (state->vertical_flag)),G_TYPE_NONE);
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
      procedure = g_str_equal (choice,"gauss-iir2") ? "plug-in-gauss-iir2" :
                  g_str_equal (choice,"gauss-rle2") ? "plug-in-gauss-rle2" : "plug-in-gauss";
      args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 1,
        G_TYPE_INT, gimp_image_get_id (image), G_TYPE_INT, 0,
        G_TYPE_DOUBLE, horizontal, G_TYPE_DOUBLE, vertical,
        G_TYPE_INT, gtk_combo_box_get_active (GTK_COMBO_BOX (state->method)), G_TYPE_NONE);
      if (strcmp (procedure,"plug-in-gauss")) gimp_value_array_truncate (args,5);
    }
  if (state->editing)
    {
      gchar *old_procedure = gimp_filter_layer_dup_procedure (GIMP_FILTER_LAYER (layer));
      raw = gimp_filter_layer_ref_definition (GIMP_FILTER_LAYER (layer));
      if (!painter_filter_isolated (choice) && ! g_strcmp0 (old_procedure, procedure))
        {
          GimpValueArray *old_args = gimp_filter_layer_dup_args (GIMP_FILTER_LAYER (layer));
          guint count = old_args ? gimp_value_array_length (old_args) : 0;
          /* Preserve the exact existing array shape/types when it is supported,
           * including float arguments and edge's historical five-slot form. */
          if (args && (count == static_cast<guint> (gimp_value_array_length (args)) ||
              (count == 5 && g_str_equal (choice,"edge"))))
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
                  if (count == 5 && g_str_equal (choice,"edge") && gtk_combo_box_get_active (GTK_COMBO_BOX (state->edge)) != 0)
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
            success = gimp_image_add_layer (image, layer, static_cast<GimpLayer *> (GIMP_IMAGE_ACTIVE_PARENT), -1, TRUE);
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
}

static void
painter_filter_choice_received (GtkComboBox *, gpointer data)
{
  painter_dialog_dispatch (data, [] (PainterLayerDialog& state) {
    painter_filter_choice_changed (&state);
  });
}

static void
painter_filter_status_received (GimpFilterLayer *layer, gpointer data)
{
  painter_dialog_dispatch (data, [&] (PainterLayerDialog& state) {
    painter_filter_status (&state, layer);
  });
}

static void
painter_filter_response_received (GtkDialog *, gint response, gpointer data)
{
  painter_dialog_dispatch (data, [&] (PainterLayerDialog& state) {
    painter_filter_response (&state, response);
  });
}

} // namespace

GtkWidget *
painter_filter_layer_dialog_new (GimpImage   *image,
                                 GimpLayer   *layer,
                                 GimpContext *context,
                                 GtkWidget   *parent)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (layer == NULL || GIMP_IS_FILTER_LAYER (layer), NULL);
  return painter_dialog_new (image, layer, context, parent,
    layer ? _("Edit Filter Layer") : _("New Filter Layer"),
    "gimp-painter-filter-layer", _("Filter the visible layers below this layer"),
    [&] (PainterLayerDialog& value) {
      auto *state = &value;
      GtkWidget *main_grid;
      GtkWidget *grid;
      gint row;
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
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice),"gauss-iir",_("Gaussian IIR (radius and integer axis flags)"));
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice),"gauss-rle",_("Gaussian RLE (radius and integer axis flags)"));
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice),"gauss-iir2",_("Gaussian IIR2 (two radii)"));
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice),"gauss-rle2",_("Gaussian RLE2 (two radii)"));
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice),"vinvert",_("Value Invert (Painter compatibility)"));
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice),"max-rgb",_("Maximum/Minimum RGB (Painter compatibility)"));
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice),"threshold-alpha",_("Threshold Alpha (Painter compatibility)"));
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice), "blinds", _("Blinds (Painter compatibility)"));
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice), "small-tiles", _("Small Tiles (Painter compatibility)"));
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice), "retinex", _("Retinex (Painter compatibility)"));
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (state->choice), "convmatrix", _("Convolution Matrix (Painter compatibility)"));
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
      state->horizontal = painter_dialog_spin (state, _("_Horizontal radius:"), "painter-gauss-horizontal", -500, 500, 5, 3);
      state->vertical = painter_dialog_spin (state, _("_Vertical radius:"), "painter-gauss-vertical", -500, 500, 5, 3);
      state->method = painter_dialog_field (state, _("_Method:"), gtk_combo_box_text_new (), "painter-gauss-method");
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->method), "IIR");
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->method), "RLE");
      gtk_combo_box_set_active (GTK_COMBO_BOX (state->method), 0);
      state->grid = grid = gtk_grid_new (); state->row = 0;
      gtk_grid_set_row_spacing (GTK_GRID (grid),8); gtk_grid_set_column_spacing (GTK_GRID (grid),8);
      gtk_stack_add_named (GTK_STACK (state->parameters),grid,"gauss-flags");
      state->radius = painter_dialog_spin (state,_("_Radius:"),"painter-gauss-radius",0,500,5,3);
      state->horizontal_flag = painter_dialog_spin (state,_("_Horizontal flag (0 disables):"),
        "painter-gauss-horizontal-flag",G_MININT,G_MAXINT,1,0);
      state->vertical_flag = painter_dialog_spin (state,_("_Vertical flag (0 disables):"),
        "painter-gauss-vertical-flag",G_MININT,G_MAXINT,1,0);
      state->grid = grid = gtk_grid_new (); state->row = 0;
      gtk_grid_set_row_spacing (GTK_GRID (grid),8); gtk_grid_set_column_spacing (GTK_GRID (grid),8);
      gtk_stack_add_named (GTK_STACK (state->parameters),grid,"point");
      state->point_argument = painter_dialog_spin (state,_("_Integer argument:"),"painter-point-argument",G_MININT,G_MAXINT,1,0);
      painter_dialog_field (state,NULL,gtk_label_new (_("Maximum RGB: positive selects maximum, otherwise minimum.\nThreshold Alpha: alpha byte threshold (normally 0–255).\nValue Invert has no additional argument.")),"painter-point-help");
      painter_filter_isolated_pages (state);
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
          painter_dialog_connect (state, layer, "filter-state-changed", G_CALLBACK (painter_filter_status_received));
        }
      else
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (state->choice), "keep");
      /* Hooks are installed during construction but dispatch only after the
       * whole slot is activated. Retained/reparented children hold no Impl. */
      painter_dialog_connect (state, state->choice, "changed", G_CALLBACK (painter_filter_choice_received));
      painter_dialog_connect (state, state->amount, "value-changed", G_CALLBACK (painter_filter_dirty));
      painter_dialog_connect (state, state->wrap, "changed", G_CALLBACK (painter_filter_dirty));
      painter_dialog_connect (state, state->edge, "changed", G_CALLBACK (painter_filter_dirty));
      painter_dialog_connect (state, state->horizontal, "value-changed", G_CALLBACK (painter_filter_dirty));
      painter_dialog_connect (state, state->vertical, "value-changed", G_CALLBACK (painter_filter_dirty));
      painter_dialog_connect (state, state->method, "changed", G_CALLBACK (painter_filter_dirty));
      painter_dialog_connect (state,state->point_argument,"value-changed",G_CALLBACK (painter_filter_dirty));
      painter_dialog_connect (state,state->radius,"value-changed",G_CALLBACK (painter_filter_dirty));
      painter_dialog_connect (state,state->horizontal_flag,"value-changed",G_CALLBACK (painter_filter_dirty));
      painter_dialog_connect (state,state->vertical_flag,"value-changed",G_CALLBACK (painter_filter_dirty));
      for (const auto& number : state->numbers)
        painter_dialog_connect (state, number.entry, "changed", G_CALLBACK (painter_filter_dirty));
      for (const auto& flag : state->flags)
        painter_dialog_connect (state, flag.widget, GTK_IS_COMBO_BOX (flag.widget) ? "changed" : "toggled", G_CALLBACK (painter_filter_dirty));
      for (const auto& selection : state->enums)
        painter_dialog_connect (state, selection.widget, "changed", G_CALLBACK (painter_filter_dirty));
      painter_dialog_connect (state, state->dialog, "response", G_CALLBACK (painter_filter_response_received));
    },
    [&] (PainterLayerDialog& state) {
      if (layer) painter_filter_status (&state, GIMP_FILTER_LAYER (layer));
      if (state.closed) return;
      painter_filter_choice_changed (&state);
      state.dirty = FALSE;
      if (!state.closed) gtk_widget_show_all (state.grid);
    });
}
