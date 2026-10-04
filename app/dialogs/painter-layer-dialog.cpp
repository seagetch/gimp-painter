/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The Painter layers are independent document types, not drawable effects.
 * This editor only invokes explicitly supported compatibility executors. */
#include "config.h"
#include <cmath>
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
#include "core/gimpprogress.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "operations/layer-modes-legacy/gimpoperationpainterlegacy.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpviewabledialog.h"
#include "painter-layer-dialog.h"
#include "gimp-intl.h"
}
#include "core/gimpfilterparametereditor.hpp"
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/resources.hpp"
#include "painter/gimp-painter-binding.h"

using namespace GimpPainter;

namespace {

/* ASCII entries avoid GtkSpinButton's display rounding and silent clamping.
 * Each field patches only its own saved scalar/array element. */
struct FilterNumberField
{
  GtkWidget *entry;
  std::string route;
  const char *key;
  gint element; // -1: scalar, otherwise an element in a typed array
  bool integer;
  std::string initial;
  gint64 initial_integer = 0;
  gdouble initial_real = 0;
};

struct FilterFlagField
{
  GtkWidget *widget;
  std::string route;
  const char *key;
  gint element;
  bool initial;
};

struct FilterEnumField
{
  GtkWidget *widget;
  std::string route;
  const char *key;
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
  GtkWidget *schema_status = nullptr;
  GtkWidget *progress = nullptr;
  GtkWidget *cancel_filter = nullptr;
  guint64 progress_generation = 0;
  guint64 status_revision = 0;
  GtkWidget *error = nullptr;
  gint       row = 0;
  std::vector<Connection> connections;
  std::vector<FilterNumberField> numbers;
  std::vector<FilterFlagField> flags;
  std::vector<FilterEnumField> enums;
  std::string loaded_route;
  guint64 loaded_revision = 0;
  std::array<std::unique_ptr<FilterEditorSchema>, 4> schemas;
  std::array<std::string, 4> schema_errors;
  std::array<bool, 4> schema_stale {};

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
    auto old_schemas = std::move (schemas);
    schema_stale.fill (true);
    g_weak_ref_set (&image, nullptr);
    g_weak_ref_set (&layer, nullptr);
    old_connections.clear ();
    /* No state is accessed after releasing retained metadata owners. */
    old_schemas = {};
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

static void painter_filter_schema_status (PainterLayerDialog *state);

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
  if (!state->closed) painter_filter_schema_status (state);
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
  const auto revision = ++state->status_revision;
  const auto generation = gimp_filter_layer_get_generation (layer);
  const auto current_render = [&] {
    return !state->closed && revision == state->status_revision &&
           generation == gimp_filter_layer_get_generation (layer);
  };
  state->progress_generation = 0;
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
  if (!current_render () || !state->progress) return;
  GimpFilterLayerProgress current {};
  gimp_filter_layer_get_progress (layer, &current);
  auto progress = ObjectRef<GObject>::retain (G_OBJECT (state->progress));
  auto cancel = ObjectRef<GObject>::retain (G_OBJECT (state->cancel_filter));
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress.get ()), current.value);
  if (!current_render ()) return;
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress.get ()), current.text);
  if (!current_render ()) return;
  gtk_widget_set_visible (GTK_WIDGET (progress.get ()), current.active);
  if (!current_render ()) return;
  gtk_widget_set_sensitive (GTK_WIDGET (cancel.get ()), current.active && current.cancellable);
  if (!current_render ()) return;
  gtk_widget_set_visible (GTK_WIDGET (cancel.get ()), current.active);
  if (!current_render ()) return;
  if (current.active && current.message_revision)
    gtk_label_set_text (GTK_LABEL (status.get ()), current.message_text);
  if (current_render ()) state->progress_generation = current.active ? current.generation : 0;
}

/* Display and editing have separate admission budgets. All values below are
 * borrowed from immutable snapshots; inspecting a saved definition must never
 * materialize an unknown string, vector, array, or boxed value. */
static gsize
painter_filter_bounded_length (const gchar *value, gsize limit)
{
  gsize size = 0;
  while (size < limit && value[size]) ++size;
  return size;
}

class PainterFilterPreview
{
public:
  static constexpr gsize limit = 64 * 1024;
  static constexpr gsize string_limit = 4 * 1024;
  PainterFilterPreview () : text_ (g_string_new (NULL)) {}
  ~PainterFilterPreview () { g_string_free (text_, TRUE); }
  bool full () const { return full_; }
  void omitted () { omitted_ = true; }

  void span (const gchar *value, gsize size)
  {
    /* Repair while appending, rather than expanding an already full preview
     * with g_utf8_make_valid(). Reserve room for the final omission notice. */
    for (gsize at = 0; at < size && !full_;)
      {
        const gunichar ch = g_utf8_get_char_validated (value + at, MIN (size - at, gsize (6)));
        const bool invalid = ch == gunichar (-1) || ch == gunichar (-2) || ch == 0;
        const gchar *piece = invalid ? "\xef\xbf\xbd" : value + at;
        const gsize count = invalid ? 3 : g_utf8_next_char (value + at) - (value + at);
        if (count > limit - sizeof (notice_) + 1 - text_->len)
          { full_ = omitted_ = true; break; }
        g_string_append_len (text_, piece, count);
        at += invalid ? 1 : count;
      }
  }
  void append (const gchar *value)
  {
    if (!value || full_) return;
    const gsize size = painter_filter_bounded_length (value, limit);
    span (value, size);
    if (size == limit) omitted_ = true;
  }
  template<typename... Args> void format (const gchar *format, Args... args)
  {
    if (full_) return;
    gchar buffer[256];
    const gint size = g_snprintf (buffer, sizeof buffer, format, args...);
    if (size < 0) { omitted (); return; }
    span (buffer, MIN (gsize (size), sizeof buffer - 1));
    if (gsize (size) >= sizeof buffer) omitted ();
  }
  void string (const gchar *value)
  {
    if (!value) { append ("NULL"); return; }
    const gsize size = painter_filter_bounded_length (value, string_limit + 1);
    format (size > string_limit ? "(at least %" G_GSIZE_FORMAT " source bytes) " :
                                 "(%" G_GSIZE_FORMAT " source bytes) ", size);
    append ("\"");
    gsize at = 0, rendered = 0;
    while (at < size && at < string_limit && !full_)
      {
        gchar escaped[5];
        const gchar *piece = value + at;
        gsize count, consumed;
        const guchar byte = static_cast<guchar> (value[at]);
        if (byte < 0x20 || byte == 0x7f || byte == '\\' || byte == '"')
          {
            if (byte == '\\' || byte == '"')
              { escaped[0] = '\\'; escaped[1] = byte; escaped[2] = 0; count = 2; }
            else
              { g_snprintf (escaped, sizeof escaped, "\\x%02x", byte); count = 4; }
            piece = escaped; consumed = 1;
          }
        else
          {
            const gunichar ch = g_utf8_get_char_validated (value + at, MIN (size - at, gsize (6)));
            if (ch == gunichar (-1) || ch == gunichar (-2))
              { piece = "\xef\xbf\xbd"; count = 3; consumed = 1; }
            else
              count = consumed = g_utf8_next_char (value + at) - (value + at);
          }
        /* Include quotes and an optional UTF-8 ellipsis in the 4 KiB cap. */
        if (count > string_limit - 5 - rendered) break;
        span (piece, count); rendered += count; at += consumed;
      }
    if (at < size) { append ("…"); omitted (); }
    append ("\"");
  }
  const gchar *finish ()
  {
    if (omitted_ && !finished_) g_string_append (text_, notice_);
    finished_ = true;
    return text_->str;
  }
private:
  static constexpr gchar notice_[] = "\n… Further data is preserved but omitted from this preview.\n";
  GString *text_;
  bool full_ = false, omitted_ = false, finished_ = false;
};

/* The project uses C++14, which requires storage for the odr-used array. */
constexpr gchar PainterFilterPreview::notice_[];

/* A conservative accounting bound for duplicate editable values plus the new
 * FilterArguments model. Fixed allowances include GValue/vector/container and
 * weak-reference storage; this is an admission budget, not a total RSS claim.
 * GBytes and GVariant copies only retain immutable data, so their backing bytes
 * are deliberately not counted as a new allocation. */
struct PainterFilterMaterialization
{
  static constexpr gsize limit = 1024 * 1024;
  gsize remaining = limit;
  guint entries = 512;
  bool charge (gsize size)
  {
    if (size > remaining) return false;
    remaining -= size; return true;
  }
  bool copies (gsize size)
  {
    if (size > remaining / 2) return false;
    return charge (size * 2);
  }
  bool string (const gchar *value)
  {
    if (!value) return true;
    const gsize allowance = remaining / 2;
    const gsize size = painter_filter_bounded_length (value, allowance);
    return size < allowance && copies (size + 1) && charge (64);
  }
};

static bool
painter_filter_materialization_check (const GimpFilterArgumentsSnapshot *snapshot,
                                      PainterFilterMaterialization     &budget,
                                      guint                             depth)
{
  if (!snapshot || depth > 32 || !budget.charge (128)) return false;
  const guint count = gimp_filter_arguments_snapshot_count (snapshot);
  if (count > budget.entries) return false;
  budget.entries -= count;
  for (guint i = 0; i < count; ++i)
    {
      if (!budget.charge (256)) return false;
      const GType type = gimp_filter_arguments_snapshot_type (snapshot, i);
      const guint references = gimp_filter_arguments_snapshot_reference_count (snapshot, i);
      /* Same-route edits share untouched snapshot slots. Reference descriptors
       * need no resolution, so live, expired, and unset provenance survives. */
      if (references > budget.entries) return false;
      budget.entries -= references;
      for (guint j = 0; j < references; ++j)
        if (!budget.charge (256)) return false;
      if (type == GIMP_TYPE_VALUE_ARRAY)
        {
          if (gimp_filter_arguments_snapshot_is_null (snapshot, i)) continue;
          auto *nested = gimp_filter_arguments_snapshot_nested (snapshot, i);
          const bool admitted = painter_filter_materialization_check (nested, budget, depth + 1);
          gimp_filter_arguments_snapshot_free (nested);
          if (!admitted) return false;
          continue;
        }
      if (g_type_is_a (type, G_TYPE_OBJECT) || type == GIMP_TYPE_CORE_OBJECT_ARRAY) continue;
      const GValue *value = gimp_filter_arguments_snapshot_peek_value (snapshot, i);
      if (!value) return false;
      if (type == G_TYPE_STRING)
        { if (!budget.string (g_value_get_string (value))) return false; }
      else if (type == G_TYPE_STRV)
        {
          const auto *strings = static_cast<const gchar *const *> (g_value_get_boxed (value));
          if (!strings) continue;
          if (!budget.copies (sizeof (gchar *))) return false;
          for (guint j = 0; strings[j]; ++j)
            {
              if (!budget.entries || !budget.copies (sizeof (gchar *)) ||
                  !budget.string (strings[j])) return false;
              --budget.entries;
            }
        }
      else if (type == GIMP_TYPE_ARRAY || type == GIMP_TYPE_INT32_ARRAY || type == GIMP_TYPE_DOUBLE_ARRAY)
        {
          const auto *array = static_cast<const GimpArray *> (g_value_get_boxed (value));
          if (!array) continue;
          const gsize width = type == GIMP_TYPE_INT32_ARRAY ? sizeof (gint32) :
                              type == GIMP_TYPE_DOUBLE_ARRAY ? sizeof (gdouble) : 1;
          if ((array->length && !array->data) || array->length % width ||
              !budget.copies (array->length) || !budget.charge (2 * (sizeof (GimpArray) + 32))) return false;
        }
      else if (type != G_TYPE_BYTES && type != G_TYPE_VARIANT && type != G_TYPE_GTYPE)
        {
          switch (G_TYPE_FUNDAMENTAL (type))
            {
            case G_TYPE_CHAR: case G_TYPE_UCHAR: case G_TYPE_BOOLEAN:
            case G_TYPE_INT: case G_TYPE_UINT: case G_TYPE_LONG: case G_TYPE_ULONG:
            case G_TYPE_INT64: case G_TYPE_UINT64: case G_TYPE_ENUM: case G_TYPE_FLAGS:
            case G_TYPE_FLOAT: case G_TYPE_DOUBLE: break;
            default: return false; // No generic boxed/pointer copying policy.
            }
        }
    }
  return true;
}

static bool
painter_filter_materializable (const GimpFilterArgumentsSnapshot *snapshot,
                               gsize                             *bytes = nullptr)
{
  PainterFilterMaterialization budget;
  const bool admitted = painter_filter_materialization_check (snapshot, budget, 0);
  if (bytes) *bytes = PainterFilterMaterialization::limit - budget.remaining;
  return admitted;
}

static void
painter_filter_preview_scalar (PainterFilterPreview &text, const GValue *value)
{
  const GType type = G_VALUE_TYPE (value);
  if (type == G_TYPE_STRING)
    text.string (g_value_get_string (value));
  else if (type == G_TYPE_STRV)
    {
      const auto *strings = static_cast<const gchar *const *> (g_value_get_boxed (value));
      text.append ("[");
      guint i = 0;
      for (; strings && i < 256 && strings[i] && !text.full (); ++i)
        { if (i) text.append (", "); text.string (strings[i]); }
      if (strings && strings[i]) { text.append (", …"); text.omitted (); }
      text.append ("]");
    }
  else if (type == GIMP_TYPE_ARRAY || type == GIMP_TYPE_DOUBLE_ARRAY || type == GIMP_TYPE_INT32_ARRAY || type == G_TYPE_BYTES)
    {
      gsize length = 0;
      const guchar *data = nullptr;
      if (type == G_TYPE_BYTES)
        data = static_cast<const guchar *> (g_bytes_get_data (static_cast<GBytes *> (g_value_get_boxed (value)), &length));
      else
        {
          const auto *array = static_cast<const GimpArray *> (g_value_get_boxed (value));
          length = array->length; data = array->data;
        }
      const gsize width = type == GIMP_TYPE_DOUBLE_ARRAY ? sizeof (gdouble) :
                          type == GIMP_TYPE_INT32_ARRAY ? sizeof (gint32) : 1;
      text.format ("(%" G_GSIZE_FORMAT " bytes, %" G_GSIZE_FORMAT " elements) [", length, length / width);
      if (length && !data) text.append ("unavailable data; retained");
      for (gsize i = 0; data && i < MIN (length / width, gsize (256)) && !text.full (); ++i)
        {
          if (i) text.append (", ");
          if (type == GIMP_TYPE_DOUBLE_ARRAY)
            {
              gdouble number; gchar printed[G_ASCII_DTOSTR_BUF_SIZE];
              memcpy (&number, data + i * width, sizeof number);
              text.append (g_ascii_dtostr (printed, sizeof printed, number));
            }
          else if (type == GIMP_TYPE_INT32_ARRAY)
            {
              gint32 number; memcpy (&number, data + i * width, sizeof number);
              text.format ("%" G_GINT32_FORMAT, number);
            }
          else text.format ("%02x", data[i]);
        }
      if (length / width > 256) { text.append (", …"); text.omitted (); }
      if (length % width) text.append ("; incomplete trailing element retained");
      text.append ("]");
    }
  else if (type == G_TYPE_VARIANT)
    {
      /* Printing or serializing an arbitrary variant can walk an unbounded
       * tree. Keep the actual variant and show only its bounded type string. */
      text.append ("immutable variant ");
      text.string (g_variant_get_type_string (g_value_get_variant (value)));
      text.append (" (contents retained)");
    }
  else if (type == G_TYPE_GTYPE)
    text.format ("%" G_GSIZE_FORMAT, gsize (g_value_get_gtype (value)));
  else
    {
      gchar number[G_ASCII_DTOSTR_BUF_SIZE];
      switch (G_TYPE_FUNDAMENTAL (type))
        {
        case G_TYPE_CHAR: text.format ("%d", gint (g_value_get_schar (value))); break;
        case G_TYPE_UCHAR: text.format ("%u", guint (g_value_get_uchar (value))); break;
        case G_TYPE_BOOLEAN: text.append (g_value_get_boolean (value) ? "TRUE" : "FALSE"); break;
        case G_TYPE_INT: text.format ("%d", g_value_get_int (value)); break;
        case G_TYPE_UINT: text.format ("%u", g_value_get_uint (value)); break;
        case G_TYPE_LONG: text.format ("%ld", g_value_get_long (value)); break;
        case G_TYPE_ULONG: text.format ("%lu", g_value_get_ulong (value)); break;
        case G_TYPE_INT64: text.format ("%" G_GINT64_FORMAT, g_value_get_int64 (value)); break;
        case G_TYPE_UINT64: text.format ("%" G_GUINT64_FORMAT, g_value_get_uint64 (value)); break;
        case G_TYPE_ENUM: text.format ("%d", g_value_get_enum (value)); break;
        case G_TYPE_FLAGS: text.format ("%u", g_value_get_flags (value)); break;
        case G_TYPE_FLOAT: text.append (g_ascii_dtostr (number, sizeof number, g_value_get_float (value))); break;
        case G_TYPE_DOUBLE: text.append (g_ascii_dtostr (number, sizeof number, g_value_get_double (value))); break;
        default: text.append ("contents retained; no bounded preview for this type"); break;
        }
    }
}

static void
painter_filter_describe (PainterFilterPreview               &text,
                         const GimpFilterArgumentsSnapshot *snapshot,
                         guint                              depth,
                         guint                             *remaining)
{
  const guint count = gimp_filter_arguments_snapshot_count (snapshot);
  guint i = 0;
  for (; i < count && *remaining && !text.full (); ++i)
    {
      const GType type = gimp_filter_arguments_snapshot_type (snapshot, i);
      --*remaining;
      text.format ("%*s%u: ", gint (MIN (depth, 32u) * 2), "", i);
      text.append (g_type_name (type));
      const bool is_null = gimp_filter_arguments_snapshot_is_null (snapshot, i);
      if (is_null) text.append (" = NULL");
      else if (const GValue *value = gimp_filter_arguments_snapshot_peek_value (snapshot, i))
        { text.append (" = "); painter_filter_preview_scalar (text, value); }
      const guint references = gimp_filter_arguments_snapshot_reference_count (snapshot, i);
      if (references || type == GIMP_TYPE_CORE_OBJECT_ARRAY)
        text.format (" (%u references)", references);
      guint j = 0;
      for (; j < references && *remaining && !text.full (); ++j)
        {
          --*remaining;
          GimpFilterArgumentReference ref;
          if (gimp_filter_arguments_snapshot_reference (snapshot, i, j, &ref))
            {
              text.format (" [%u: ", j); text.append (g_type_name (ref.object_type));
              text.format (" #%" G_GINT64_FORMAT " %s]", ref.id,
                           !ref.was_set ? "unset" : ref.expired ? "expired" : "saved");
            }
        }
      if (j < references) { text.append (" […]"); text.omitted (); }
      if (type == GIMP_TYPE_VALUE_ARRAY && !is_null && !text.full ())
        {
          auto *nested = gimp_filter_arguments_snapshot_nested (snapshot, i);
          if (nested)
            {
              text.format (" (%u nested arguments)\n", gimp_filter_arguments_snapshot_count (nested));
              if (depth < 32) painter_filter_describe (text, nested, depth + 1, remaining);
              else text.omitted ();
              gimp_filter_arguments_snapshot_free (nested);
            }
          else text.append (" (nested arguments unavailable)\n");
        }
      else text.append ("\n");
    }
  if (i < count) text.omitted ();
}

static void
painter_filter_details (PainterLayerDialog *state,
                        GimpFilterLayer    *layer)
{
  gboolean                     procedure_truncated = FALSE;
  gchar                       *procedure = gimp_filter_layer_dup_procedure_prefix (layer, 4096, &procedure_truncated);
  GBytes                      *raw = gimp_filter_layer_ref_definition (layer);
  GimpFilterArgumentsSnapshot *snapshot = gimp_filter_layer_snapshot_arguments (layer);
  PainterFilterPreview         text;
  GtkWidget                   *expander;
  GtkWidget                   *scroll;
  GtkWidget                   *view;
  guint                        remaining = 512;
  text.append (_("Procedure: "));
  text.string (procedure);
  if (procedure_truncated)
    { text.append (_(" (name truncated; remaining bytes retained)")); text.omitted (); }
  text.format (_("\nOriginal metadata: %" G_GSIZE_FORMAT " bytes (preserved)\n"), raw ? g_bytes_get_size (raw) : 0);
  if (snapshot) painter_filter_describe (text, snapshot, 0, &remaining);
  else text.append (_("Arguments remain in the original metadata.\n"));
  if (raw && g_bytes_get_size (raw) && !text.full ())
    {
      gsize size;
      const guchar *bytes = static_cast<const guchar *> (g_bytes_get_data (raw, &size));
      text.append (_("\nOriginal metadata (hex preview):\n"));
      for (gsize i = 0; i < MIN (size, gsize (4096)) && !text.full (); ++i)
        text.format ("%02x%s", bytes[i], i % 16 == 15 ? "\n" : " ");
      if (size > 4096) { text.append (_("\nRemaining bytes are preserved.\n")); text.omitted (); }
    }
  expander = gtk_expander_new (_("Saved Definition"));
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (scroll, 420, 160);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  view = gtk_text_view_new ();
  gtk_widget_set_name (view, "painter-filter-definition");
  gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), text.finish (), -1);
  gtk_container_add (GTK_CONTAINER (scroll), view);
  gtk_container_add (GTK_CONTAINER (expander), scroll);
  gtk_grid_attach (GTK_GRID (state->grid), expander, 0, state->row++, 2, 1);
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

static FilterProcedure
painter_filter_route (const char *route)
{
  if (!strcmp (route, "blinds")) return FilterProcedure::blinds;
  if (!strcmp (route, "small-tiles")) return FilterProcedure::small_tiles;
  if (!strcmp (route, "retinex")) return FilterProcedure::retinex;
  if (!strcmp (route, "convmatrix")) return FilterProcedure::convolution;
  throw std::invalid_argument ("Unknown Filter editor route");
}

static FilterEditorField
painter_filter_field (const PainterLayerDialog *state, const std::string& route, const char *key)
{
  const auto id = painter_filter_route (route.c_str ());
  const auto& schema = state->schemas[static_cast<unsigned> (id) - 1];
  /* Saved values remain displayable without metadata. When available, resolve
   * each control through the immutable registered schema's canonical key. */
  return schema ? schema->field (key).legacy : filter_editor_field (id, key);
}

static void
painter_filter_schema_status (PainterLayerDialog *state)
{
  if (state->closed || !state->schema_status) return;
  const char *selected = gtk_combo_box_get_active_id (GTK_COMBO_BOX (state->choice));
  const std::string route = selected ? selected : "keep";
  std::string message;
  bool usable = true;
  if (painter_filter_isolated (route.c_str ()))
    {
      const auto index = static_cast<unsigned> (painter_filter_route (route.c_str ())) - 1;
      usable = state->schemas[index] && !state->schema_stale[index];
      if (state->schema_stale[index])
        message = _("This filter changed while this editor was open. Reopen it to edit. Keeping saved values is still available.");
      else if (!state->schemas[index])
        message = state->schema_errors[index] + " " +
          _("Editing is unavailable. Saved values and completed pixels are preserved.");
      else
        message = _("This filter is available. Unchanged unsupported values are preserved.");
    }
  auto label = ObjectRef<GObject>::retain (G_OBJECT (state->schema_status));
  gtk_label_set_text (GTK_LABEL (label.get ()), message.c_str ());
  if (state->closed || g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (state->choice)), route.c_str ())) return;
  /* Existing definitions always retain a no-op OK path. A new definition
   * cannot Apply settings until its registered metadata is ready. */
  gtk_dialog_set_response_sensitive (GTK_DIALOG (state->dialog), GTK_RESPONSE_OK,
    usable || state->editing);
}

static void
painter_filter_registry_changed (GimpPDB *, GimpProcedure *procedure, gpointer data)
{
  painter_dialog_dispatch (data, [procedure] (PainterLayerDialog& state) {
    for (unsigned index = 0; index < state.schemas.size (); ++index)
      if (filter_editor_procedure_matches (static_cast<FilterProcedure> (index + 1), procedure))
        state.schema_stale[index] = true;
    /* Keep old immutable metadata/owners alive through close. Nothing is
     * released from a registry callback, which may synchronously close us. */
    painter_filter_schema_status (&state);
  });
}

static void
painter_filter_entry_value (FilterNumberField& field, gdouble value)
{
  gchar text[G_ASCII_DTOSTR_BUF_SIZE];
  field.initial_real = value;
  field.initial = g_ascii_dtostr (text, sizeof text, value);
  gtk_entry_set_text (GTK_ENTRY (field.entry), field.initial.c_str ());
}

static void
painter_filter_entry_integer (FilterNumberField& field, gint64 value)
{
  field.initial_integer = value;
  field.initial = std::to_string (value);
  gtk_entry_set_text (GTK_ENTRY (field.entry), field.initial.c_str ());
}

static GtkWidget *
painter_filter_entry (PainterLayerDialog *state, const gchar *route,
                      const char *key, gint element,
                      const gchar *label, const gchar *name, bool attach = true)
{
  const auto legacy = painter_filter_field (state, route, key);
  const bool integer = legacy.type == FilterEditorValueType::integer;
  GtkWidget *entry = gtk_entry_new ();
  gtk_widget_set_name (entry, name);
  gtk_entry_set_width_chars (GTK_ENTRY (entry), element >= 0 ? 8 : 22);
  gtk_entry_set_max_length (GTK_ENTRY (entry), 128);
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  state->numbers.push_back ({entry, route, key, element, integer, ""});
  if (integer) painter_filter_entry_integer (state->numbers.back (), legacy.integer_initial);
  else painter_filter_entry_value (state->numbers.back (),
    legacy.type == FilterEditorValueType::double_array ? (element == 12 ? 1 : 0) : legacy.real_initial);
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
painter_filter_flag (PainterLayerDialog *state, const gchar *route, const char *key, gint element,
                     const gchar *label, const gchar *name, bool initial, bool attach = true)
{
  GtkWidget *widget = gtk_check_button_new_with_mnemonic (label);
  gtk_widget_set_name (widget, name);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), initial);
  state->flags.push_back ({widget, route, key, element, initial});
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
painter_filter_enum (PainterLayerDialog *state, const gchar *route, const char *key,
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
  state->enums.push_back ({widget, route, key, initial});
}

static bool
painter_filter_isolated_shape (const gchar *route, const GimpFilterArgumentsSnapshot *args)
{
  const guint count = args ? gimp_filter_arguments_snapshot_count (args) : 0;
  const auto integer = [&] (guint i) { return i < count && G_VALUE_HOLDS_INT (gimp_filter_arguments_snapshot_peek_value (args, i)); };
  if (!strcmp (route, "blinds"))
    return count == 7 && integer (3) && integer (4) && integer (5) && integer (6);
  if (!integer (0) || !integer (1) || !integer (2)) return false;
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
        const GValue *value = gimp_filter_arguments_snapshot_peek_value (args, painter_filter_field (state, field.route, field.key).saved_slot);
        if (field.integer)
          painter_filter_entry_integer (field, g_value_get_int (value));
        else
          {
            gdouble number;
            if (field.element < 0) number = g_value_get_double (value);
            else
              {
                const auto *array = static_cast<const GimpArray *> (g_value_get_boxed (value));
                memcpy (&number, array->data + field.element * sizeof number, sizeof number);
              }
            painter_filter_entry_value (field, number);
          }
      }
  for (auto& field : state->flags)
    if (field.route == route)
      {
        const GValue *value = gimp_filter_arguments_snapshot_peek_value (args, painter_filter_field (state, field.route, field.key).saved_slot);
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
        field.initial = g_value_get_int (gimp_filter_arguments_snapshot_peek_value (args, painter_filter_field (state, field.route, field.key).saved_slot));
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

struct PainterFilterCommitCheck
{
  PainterLayerDialog *state;
  GimpImage *image;
  GimpLayer *layer;
  unsigned route;
};

static gboolean
painter_filter_commit_current (gpointer data)
{
  const auto& check = *static_cast<const PainterFilterCommitCheck *> (data);
  const auto *state = check.state;
  if (state->closed) return FALSE;
  const char *route = gtk_combo_box_get_active_id (GTK_COMBO_BOX (state->choice));
  return painter_filter_isolated (route) &&
         static_cast<unsigned> (painter_filter_route (route)) - 1 == check.route &&
         (!state->editing || (check.layer && state->loaded_revision ==
           gimp_filter_layer_get_definition_revision (GIMP_FILTER_LAYER (check.layer)))) &&
         !state->schema_stale[check.route] && state->schemas[check.route] &&
         state->schemas[check.route]->current (check.image->gimp);
}

static gboolean
painter_filter_isolated_args (PainterLayerDialog *state, const gchar *route,
                             GimpImage *image, GimpLayer *layer, GimpValueArray **result)
{
  const bool preserve = layer && state->loaded_route == route;
  if (layer && state->loaded_revision != gimp_filter_layer_get_definition_revision (GIMP_FILTER_LAYER (layer)))
    {
      painter_dialog_error (state, _("This filter definition changed while the editor was open. Reopen the editor to use the latest values."));
      return FALSE;
    }
  /* Stage only bounded, explicitly changed values. Do not materialize the saved
   * definition (including Convolution's arbitrary tail) for no-op/Cancel. */
  struct Patch { unsigned slot; gint element; bool integer; gint64 integer_value; gdouble real_value; };
  std::vector<Patch> patches;
  const bool convolution = !strcmp (route, "convmatrix");
  const bool legacy_float = gimp_image_get_precision (image) == GIMP_PRECISION_U8_NON_LINEAR;
  for (const auto& field : state->numbers)
    if (field.route == route)
      {
        const gchar *text = gtk_entry_get_text (GTK_ENTRY (field.entry));
        if (preserve && field.initial == text) continue;
        const auto legacy = painter_filter_field (state, field.route, field.key);
        gchar *end = nullptr;
        errno = 0;
        gint64 integer = 0;
        gdouble real = 0;
        if (field.integer) integer = g_ascii_strtoll (text, &end, 10);
        else real = g_ascii_strtod (text, &end);
        const bool consumed = end && end != text;
        while (end && g_ascii_isspace (*end)) ++end;
        /* A finite binary64 subnormal is representable even when ERANGE was
         * set. Underflow to zero and overflow are rejected for new input. */
        const bool range_error = errno == ERANGE && (field.integer || real == 0 || !std::isfinite (real));
        bool invalid = !consumed || *end || range_error;
        if (field.integer)
          invalid = invalid || integer < legacy.integer_min || integer > legacy.integer_max;
        else
          invalid = invalid || !std::isfinite (real) || real < legacy.real_min || real > legacy.real_max ||
            (convolution && legacy_float && std::fabs (real) > std::numeric_limits<float>::max ()) ||
            (convolution && !strcmp (field.key, "divisor") &&
             (real == 0 || (legacy_float && static_cast<float> (real) == 0)));
        if (invalid)
          {
            painter_dialog_error (state, _("Enter a finite number within the indicated range. Use a dot for decimals; integer flags must be whole numbers. The divisor must be nonzero and representable at this image precision."));
            if (!state->closed) gtk_widget_grab_focus (field.entry);
            return FALSE;
          }
        if (preserve && (field.integer ? integer == field.initial_integer :
                         !memcmp (&real, &field.initial_real, sizeof real))) continue;
        patches.push_back ({legacy.saved_slot, field.element, field.integer, integer, real});
      }
  for (const auto& field : state->flags)
    if (field.route == route)
      {
        const bool active = painter_filter_flag_active (field);
        if (preserve && active == field.initial) continue;
        patches.push_back ({painter_filter_field (state, field.route, field.key).saved_slot, field.element, true,
                            active ? 1 : 0, 0});
      }
  for (const auto& field : state->enums)
    if (field.route == route)
      {
        const gint value = painter_filter_enum_value (field);
        if (preserve && value == field.initial) continue;
        const auto legacy = painter_filter_field (state, field.route, field.key);
        if (value < legacy.integer_min || value > legacy.integer_max)
          {
            painter_dialog_error (state, _("Choose a supported value for the changed setting."));
            return FALSE;
          }
        patches.push_back ({legacy.saved_slot, -1, true, value, 0});
      }
  if (preserve && patches.empty ()) { *result = NULL; return TRUE; }
  const auto index = static_cast<unsigned> (painter_filter_route (route)) - 1;
  if (state->schema_stale[index] || !state->schemas[index] || !state->schemas[index]->current (image->gimp))
    {
      painter_dialog_error (state, _("This filter changed or is unavailable. Reopen it to edit. Saved values and completed pixels are preserved."));
      return FALSE;
    }
  std::unique_ptr<GimpFilterArgumentsSnapshot, decltype (&gimp_filter_arguments_snapshot_free)>
    snapshot (preserve ? gimp_filter_layer_snapshot_arguments (GIMP_FILTER_LAYER (layer)) : nullptr,
              gimp_filter_arguments_snapshot_free);
  GimpValueArray *args = preserve ? nullptr : painter_filter_isolated_defaults (route, image);
  const auto saved_value = [&] (guint slot) {
    return preserve ? gimp_filter_arguments_snapshot_peek_value (snapshot.get (), slot) :
                      gimp_value_array_index (args, slot);
  };
  std::vector<Value> values;
  std::vector<GimpFilterArgumentPatch> edits;
  values.reserve (patches.size () + 2);
  edits.reserve (patches.size () + 2);
  FilterLegacy::Matrix matrix {};
  FilterLegacy::Channels channels {};
  bool matrix_changed = false, channels_changed = false;
  const auto matrix_slot = convolution ? painter_filter_field (state, route, "matrix").saved_slot : 0;
  const auto channel_slot = convolution ? painter_filter_field (state, route, "channels").saved_slot : 0;
  if (convolution)
    {
      memcpy (matrix.data (), gimp_value_get_double_array (saved_value (matrix_slot), NULL), sizeof matrix);
      memcpy (channels.data (), gimp_value_get_int32_array (saved_value (channel_slot), NULL), sizeof channels);
    }
  for (const auto& patch : patches)
    {
      if (patch.element < 0)
        {
          GValue *value;
          if (preserve)
            {
              values.emplace_back (patch.integer ? G_TYPE_INT : G_TYPE_DOUBLE);
              value = values.back ().get ();
              edits.push_back ({patch.slot, value});
            }
          else value = gimp_value_array_index (args, patch.slot);
          if (patch.integer) g_value_set_int (value, static_cast<gint> (patch.integer_value));
          else g_value_set_double (value, patch.real_value);
        }
      else if (patch.integer)
        { channels[patch.element] = static_cast<gint32> (patch.integer_value); channels_changed = true; }
      else
        { matrix[patch.element] = patch.real_value; matrix_changed = true; }
    }
  if (matrix_changed)
    {
      if (preserve)
        {
          values.emplace_back (GIMP_TYPE_DOUBLE_ARRAY);
          gimp_value_set_double_array (values.back ().get (), matrix.data (), matrix.size ());
          edits.push_back ({matrix_slot, values.back ().get ()});
        }
      else gimp_value_set_double_array (gimp_value_array_index (args, matrix_slot), matrix.data (), matrix.size ());
    }
  if (channels_changed)
    {
      if (preserve)
        {
          values.emplace_back (GIMP_TYPE_INT32_ARRAY);
          gimp_value_set_int32_array (values.back ().get (), channels.data (), channels.size ());
          edits.push_back ({channel_slot, values.back ().get ()});
        }
      else gimp_value_set_int32_array (gimp_value_array_index (args, channel_slot), channels.data (), channels.size ());
    }
  if (preserve)
    {
      GError *error = nullptr;
      PainterFilterCommitCheck check {state, image, layer, index};
      const gboolean success = gimp_filter_layer_edit_argument_patch (GIMP_FILTER_LAYER (layer),
        state->loaded_revision, snapshot.get (), edits.size (), edits.data (),
        painter_filter_commit_current, &check, &error);
      if (!success)
        {
          if (!state->closed) painter_dialog_error (state, error ? error->message : _("The saved arguments could not be edited safely."));
          g_clear_error (&error);
          return FALSE;
        }
      *result = nullptr;
      gimp_image_flush (image);
      return TRUE;
    }
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
  painter_filter_entry (state, "blinds", "angle-displacement", -1, _("_Angle (0–90):"), "painter-blinds-angle");
  painter_filter_entry (state, "blinds", "num-segments", -1, _("_Segments (1–100):"), "painter-blinds-segments");
  GtkWidget *orientation = painter_dialog_field (state, _("_Orientation:"), gtk_combo_box_text_new (), "painter-blinds-orientation");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (orientation), "horizontal", _("Horizontal"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (orientation), "vertical", _("Vertical"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (orientation), 0);
  state->flags.push_back ({orientation, "blinds", "orientation", -1, false});
  painter_filter_flag (state, "blinds", "bg-transparent", -1, _("_Transparent background"), "painter-blinds-transparent", false);
  help (_("Requires 8-bit non-linear RGB or grayscale.\nSaved legacy direction and transparency values are retained until their setting changes."), "painter-blinds-help");
  page ("small-tiles");
  painter_filter_entry (state, "small-tiles", "num-tiles", -1, _("_Tile factor (0–6):"), "painter-small-tiles-factor");
  help (_("The saved legacy range includes 0 and 1.\nRequires 8-bit non-linear RGB or grayscale."), "painter-small-tiles-help");
  page ("retinex");
  painter_filter_entry (state, "retinex", "scale", -1, _("_Scale (16–256):"), "painter-retinex-scale");
  painter_filter_entry (state, "retinex", "nscales", -1, _("_Number of scales (0–8):"), "painter-retinex-nscales");
  const gchar *distributions[] = {_("Uniform"), _("Low"), _("High")};
  painter_filter_enum (state, "retinex", "scales-mode", _("_Distribution:"), "painter-retinex-mode", distributions, 0);
  painter_filter_entry (state, "retinex", "cvar", -1, _("_Dynamic (0–4):"), "painter-retinex-cvar");
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
        GtkWidget *entry = painter_filter_entry (state, "convmatrix", "matrix", index, NULL, name, false);
        gchar *tip = g_strdup_printf (_("Column %u, row %u (saved coefficient %u)"), x + 1, y + 1, index);
        gtk_widget_set_tooltip_text (entry, tip);
        atk_object_set_name (gtk_widget_get_accessible (entry), tip);
        gtk_grid_attach (GTK_GRID (matrix), entry, x, y, 1, 1);
        g_free (tip); g_free (name);
      }
  painter_filter_entry (state, "convmatrix", "divisor", -1, _("_Divisor (nonzero):"), "painter-convmatrix-divisor");
  painter_filter_entry (state, "convmatrix", "offset", -1, _("_Offset (byte units):"), "painter-convmatrix-offset");
  const gchar *borders[] = {_("Extend"), _("Wrap"), _("Clear")};
  painter_filter_enum (state, "convmatrix", "border-mode", _("_Border:"), "painter-convmatrix-border", borders, 2);
  GtkWidget *channels = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (channels), 4);
  gtk_grid_attach (GTK_GRID (state->grid), channels, 0, state->row++, 2, 1);
  const gchar *labels[] = {_("Gray"), _("Red"), _("Green"), _("Blue"), _("Alpha")};
  for (guint i = 0; i < 5; ++i)
    {
      gchar *name = g_strdup_printf ("painter-convmatrix-channel-%u", i);
      GtkWidget *entry = painter_filter_flag (state, "convmatrix", "channels", i, labels[i], name, true, false);
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
  return std::isfinite (*out) && *out >= min && *out <= max;
}

/* Keep unsupported shapes selected as "keep", including known procedure names
 * with future/unknown arguments. Merely opening and accepting never normalizes. */
static void
painter_filter_load (PainterLayerDialog *state,
                     GimpFilterLayer    *layer)
{
  gchar          *procedure = gimp_filter_layer_dup_procedure_prefix (layer, 128, NULL);
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
  {
    GimpFilterArgumentsSnapshot *snapshot = gimp_filter_layer_snapshot_arguments (layer);
    const bool bounded = painter_filter_materializable (snapshot);
    gimp_filter_arguments_snapshot_free (snapshot);
    if (!bounded) goto out;
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
      gchar *old_procedure = gimp_filter_layer_dup_procedure_prefix (GIMP_FILTER_LAYER (layer), 128, NULL);
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
      if (painter_filter_isolated (choice))
        {
          PainterFilterCommitCheck check {state, image, layer,
            static_cast<unsigned> (painter_filter_route (choice)) - 1};
          success = gimp_filter_layer_set_definition_checked (GIMP_FILTER_LAYER (layer), procedure, raw, args,
            TRUE, painter_filter_commit_current, &check, &error);
        }
      else success = gimp_filter_layer_edit_definition (GIMP_FILTER_LAYER (layer), procedure, raw, args, &error);
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
          if (painter_filter_isolated (choice))
            {
              PainterFilterCommitCheck check {state, image, layer,
                static_cast<unsigned> (painter_filter_route (choice)) - 1};
              success = gimp_filter_layer_set_definition_checked (GIMP_FILTER_LAYER (layer), procedure, NULL, args,
                FALSE, painter_filter_commit_current, &check, &error);
            }
          else success = gimp_filter_layer_set_definition (GIMP_FILTER_LAYER (layer), procedure, NULL, args, &error);
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
painter_filter_cancel_received (GtkButton *, gpointer data)
{
  painter_dialog_dispatch (data, [] (PainterLayerDialog& state) {
    auto layer = ObjectRef<GObject>::adopt (static_cast<GObject *> (g_weak_ref_get (&state.layer)));
    if (layer && state.progress_generation == gimp_filter_layer_get_generation (GIMP_FILTER_LAYER (layer.get ())))
      gimp_progress_cancel (GIMP_PROGRESS (layer.get ()));
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
      std::size_t descriptor_bytes = 0;
      for (unsigned index = 0; index < state->schemas.size (); ++index)
        {
          try
            {
              auto schema = std::unique_ptr<FilterEditorSchema> (new FilterEditorSchema (
                image->gimp, static_cast<FilterProcedure> (index + 1)));
              if (schema->snapshot_bytes () > FilterEditorSchema::max_bytes - descriptor_bytes)
                throw std::runtime_error (_("Filter settings exceed the total editor memory budget."));
              descriptor_bytes += schema->snapshot_bytes ();
              state->schemas[index] = std::move (schema);
            }
          catch (const std::exception& failure) { state->schema_errors[index] = failure.what (); }
        }
      painter_filter_isolated_pages (state);
      state->grid = main_grid; state->row = row;
      state->schema_status = painter_dialog_field (state, NULL, gtk_label_new (NULL), "painter-filter-schema-status");
      gtk_label_set_line_wrap (GTK_LABEL (state->schema_status), TRUE);
      gtk_label_set_max_width_chars (GTK_LABEL (state->schema_status), 60);
      gtk_label_set_xalign (GTK_LABEL (state->schema_status), 0);
      painter_dialog_connect (state, image->gimp->pdb, "register-procedure", G_CALLBACK (painter_filter_registry_changed));
      painter_dialog_connect (state, image->gimp->pdb, "unregister-procedure", G_CALLBACK (painter_filter_registry_changed));
      state->status = painter_dialog_field (state, NULL, gtk_label_new (NULL), "painter-filter-status");
      gtk_label_set_line_wrap (GTK_LABEL (state->status), TRUE);
      gtk_label_set_max_width_chars (GTK_LABEL (state->status), 60);
      gtk_label_set_xalign (GTK_LABEL (state->status), 0);
      state->progress = painter_dialog_field (state, NULL, gtk_progress_bar_new (), "painter-filter-progress");
      gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (state->progress), TRUE);
      gtk_widget_set_no_show_all (state->progress, TRUE);
      state->cancel_filter = painter_dialog_field (state, NULL,
        gtk_button_new_with_mnemonic (_("_Cancel Update")), "painter-filter-cancel");
      gtk_widget_set_no_show_all (state->cancel_filter, TRUE);
      painter_dialog_connect (state, state->cancel_filter, "clicked", G_CALLBACK (painter_filter_cancel_received));
      gtk_widget_show_all (state->grid);
      if (layer)
        {
          painter_filter_load (state, GIMP_FILTER_LAYER (layer));
          painter_filter_details (state, GIMP_FILTER_LAYER (layer));
          painter_dialog_connect (state, layer, "filter-state-changed", G_CALLBACK (painter_filter_status_received));
          painter_dialog_connect (state, layer, "filter-progress-changed", G_CALLBACK (painter_filter_status_received));
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
