/* SPDX-License-Identifier: GPL-3.0-or-later
 * CloneLayer semantics derived from gimp-painter afa43fae (2017 seagetch).
 */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core-types.h"
#include "gimp.h"
#include "gimpclonelayer.h"
#include "gimpclonelayerundo.h"
#include "gimpcontainer.h"
#include "gimpimage.h"
#include "gimpimage-undo.h"
#include "gimplayermask.h"
#include "gimppickable.h"
#include "gegl/gimp-babl.h"
#include "gegl/gimp-gegl-utils.h"
}
#include "painter/binding-store.hpp"
#include "gimp-painter-type-traits.hpp"
#include "painter/connection.hpp"
#include "painter/gimp-painter-binding.h"
#include "painter/resources.hpp"
#include "painter/source.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <vector>

using namespace GimpPainter;

static void gimp_clone_layer_pickable_init (GimpPickableInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GimpCloneLayer, gimp_clone_layer, GIMP_TYPE_LAYER,
                        G_IMPLEMENT_INTERFACE (GIMP_TYPE_PICKABLE, gimp_clone_layer_pickable_init))
G_DEFINE_TYPE (GimpCloneLayerUndo, gimp_clone_layer_undo, GIMP_TYPE_ITEM_UNDO)

namespace {
struct CloneImpl;
struct CloneSlot : SlotSpec<GimpCloneLayer, CloneImpl> {};
bool depends_on (GimpLayer *, GimpLayer *, std::set<GimpLayer *>&);
struct Flag
{
  explicit Flag (bool& flag) : flag (flag), previous (flag) { flag = true; }
  ~Flag () { flag = previous; }
  bool& flag;
  bool previous;
};

struct Callback
{
  explicit Callback (GimpCloneLayer *owner)
    : owner (ObjectRef<GObject>::retain (G_OBJECT (owner))),
      generation (BindingStore::require (G_OBJECT (owner)).generation ()) {}
  WeakRef<GObject> owner;
  std::uint64_t generation;
};
void callback_free (gpointer data, GClosure *) { delete static_cast<Callback *> (data); }

template<class F> void visit_callback (gpointer data, F function) noexcept
{
  boundary_void (nullptr, [&] {
    auto *callback = static_cast<Callback *> (data);
    auto owner = callback->owner.lock ();
    if (!owner) return;
    auto *store = BindingStore::find (owner.get ());
    if (store && store->accepts (callback->generation)) store->with<CloneSlot> (function);
  });
}

GimpLayer *find_name (GimpContainer *container, const char *name, std::set<GimpContainer *>& visited)
{
  if (!container || !visited.insert (container).second) return nullptr;
  const int count = gimp_container_get_n_children (container);
  for (int i = 0; i < count; ++i)
    {
      GimpObject *object = gimp_container_get_child_by_index (container, i);
      if (!GIMP_IS_LAYER (object)) continue;
      /* Legacy pre-order: compare parent before descending, then next sibling. */
      if (g_strcmp0 (gimp_object_get_name (object), name) == 0) return GIMP_LAYER (object);
      if (auto *found = find_name (gimp_viewable_get_children (GIMP_VIEWABLE (object)), name, visited))
        return found;
    }
  return nullptr;
}

/* Preserve the byte arithmetic, including its historical rounding. */
unsigned multiply (unsigned a, unsigned b)
{ unsigned t = a * b + 0x80; return ((t >> 8) + t) >> 8; }
unsigned multiply3 (unsigned a, unsigned b, unsigned c)
{ unsigned t = a * b * c + 0x7f5b; return ((t >> 7) + t) >> 16; }
const std::array<guint32, 4096>& dissolve_seeds ()
{
  static const std::array<guint32, 4096> seeds = [] {
    std::array<guint32, 4096> result;
    GRand *random = g_rand_new_with_seed (314159265);
    for (auto& seed : result) seed = g_rand_int (random);
    g_rand_free (random);
    return result;
  } ();
  return seeds;
}

struct ReferenceState
{
  WeakRef<GimpLayer> source;
  WeakRef<GObject> source_image;
  ObjectRef<GObject> buffer;
  std::string pending_name, record_name;
  bool pending = false, expired = false, had_source = false, allow_name_lookup = true;
  gint x = 0, y = 0, previous_x = 0, previous_y = 0, previous_w = 0, previous_h = 0;
};

struct CloneImpl
{
  explicit CloneImpl (GimpCloneLayer *owner) : owner (owner) {}
  ~CloneImpl () noexcept = default;
  void close () noexcept
  {
    ++reference_generation;
    pending_refresh.close ();
    owner_image_connection.close ();
    owner_image_changed_connection.close ();
    source_image_connection.close ();
    source_image_changed_connection.close ();
    owner_image.reset ();
    source_image.reset ();
    update_connection.close ();
    freeze_connection.close ();
    name_connection.close ();
    disconnect_connection.close ();
    source.reset ();
    if (mirrored_freeze)
      { mirrored_freeze = false; gimp_viewable_preview_thaw (GIMP_VIEWABLE (owner)); }
  }
  GimpLayer *get_source ()
  {
    if (!owner_image.lock ())
      { BindingStore::require (G_OBJECT (owner)).close (); return nullptr; }
    if (has_pending_name && allow_name_lookup && !resolving)
      {
        Flag resolving_guard (resolving);
        GimpImage *image = gimp_item_get_image (GIMP_ITEM (owner));
        std::set<GimpContainer *> visited;
        GimpLayer *found = image ? find_name (gimp_image_get_layers (image), pending_name.c_str (), visited) : nullptr;
        if (found)
          {
            /* Clear only successful lazy resolution. Direct assignment itself
             * deliberately does not clear an outstanding name. */
            has_pending_name = false;
            pending_name.clear ();
            set_source (found);
          }
      }
    auto ref = source.lock ();
    return ref ? GIMP_LAYER (ref.get ()) : nullptr;
  }
  void set_name (const char *name)
  {
    has_pending_name = name != nullptr;
    allow_name_lookup = true;
    pending_name = name ? name : "";
  }
  gchar *dup_name () const
  {
    if (has_pending_name) return g_strdup (pending_name.c_str ());
    auto ref = source.lock ();
    return g_strdup (ref ? gimp_object_get_name (ref.get ()) : record_name.c_str ());
  }
  void remember_name ()
  {
    auto ref = source.lock ();
    if (ref) { const char *name = gimp_object_get_name (ref.get ()); record_name = name ? name : ""; }
  }
  ReferenceState capture () const;
  bool restore (const ReferenceState& state);
  void set_source (GimpLayer *layer, bool emit_update = true);
  void attach_owner_image ();
  void attach_source_image ();
  void watch_owner_image ();
  void defer_refresh ();
  void update (GimpDrawable *drawable, gint x, gint y, gint width, gint height, bool resolve_name = true);
  void mirror_freeze (GObject *object)
  {
    std::set<GimpLayer *> seen;
    const bool cyclic = depends_on (GIMP_LAYER (object), GIMP_LAYER (owner), seen);
    bool frozen = !cyclic && gimp_viewable_preview_is_frozen (GIMP_VIEWABLE (object));
    if (frozen == mirrored_freeze) return;
    mirrored_freeze = frozen;
    if (frozen) gimp_viewable_preview_freeze (GIMP_VIEWABLE (owner));
    else gimp_viewable_preview_thaw (GIMP_VIEWABLE (owner));
  }
  GimpCloneLayer *owner;              // owner owns this slot
  WeakRef<GObject> source, owner_image, source_image;
  Connection owner_image_connection, owner_image_changed_connection;
  Connection source_image_connection, source_image_changed_connection;
  Connection update_connection, freeze_connection, name_connection, disconnect_connection;
  std::uint64_t reference_generation = 0;
  Source pending_refresh;
  std::string pending_name, record_name;
  bool allow_name_lookup = true;
  bool source_expired = false, restoring = false, editing_reference = false;
  bool has_pending_name = false, resolving = false, updating = false, mirrored_freeze = false;
  gint previous_x = 0, previous_y = 0, previous_width = 0, previous_height = 0;
};

void source_update (GimpDrawable *source, gint x, gint y, gint w, gint h, gpointer data)
{ visit_callback (data, [&] (CloneImpl& impl) { impl.update (source, x, y, w, h); }); }
void source_frozen (GObject *source, GParamSpec *, gpointer data)
{ visit_callback (data, [&] (CloneImpl& impl) { impl.mirror_freeze (source); }); }
void source_disconnected (GimpObject *, gpointer data)
{ visit_callback (data, [] (CloneImpl& impl) { impl.set_source (nullptr); if (!impl.source.lock ()) impl.source_expired = true; }); }
void owner_image_disconnected (GimpObject *, gpointer data)
{ visit_callback (data, [] (CloneImpl& impl) { BindingStore::require (G_OBJECT (impl.owner)).close (); }); }
void owner_image_changed (GObject *, GParamSpec *, gpointer data)
{ visit_callback (data, [] (CloneImpl& impl) { impl.attach_owner_image (); }); }
void source_image_changed (GObject *, GParamSpec *, gpointer data)
{ visit_callback (data, [] (CloneImpl& impl) { impl.attach_source_image (); }); }
void source_renamed (GimpObject *, gpointer data)
{ visit_callback (data, [] (CloneImpl& impl) { impl.remember_name (); }); }
Connection connect (GimpCloneLayer *owner, const ObjectRef<GObject>& emitter,
                    const char *signal, GCallback callback)
{
  std::unique_ptr<Callback> token (new Callback (owner));
  auto connection = Connection::connect (emitter, signal, callback, token.get (), callback_free);
  token.release ();
  return connection;
}
void CloneImpl::attach_owner_image ()
{
  auto image = ObjectRef<GObject>::retain (G_OBJECT (gimp_item_get_image (GIMP_ITEM (owner))));
  WeakRef<GObject> weak (image);
  Connection connection;
  if (image) connection = connect (owner, image, "disconnect", G_CALLBACK (owner_image_disconnected));
  owner_image_connection = std::move (connection);
  owner_image = std::move (weak);
}
void CloneImpl::watch_owner_image ()
{
  owner_image_changed_connection = connect (owner, ObjectRef<GObject>::retain (G_OBJECT (owner)),
                                             "notify::image", G_CALLBACK (owner_image_changed));
  attach_owner_image ();
}
void CloneImpl::attach_source_image ()
{
  auto current_source = source.lock ();
  auto image = ObjectRef<GObject>::retain (current_source ?
    G_OBJECT (gimp_item_get_image (GIMP_ITEM (current_source.get ()))) : nullptr);
  WeakRef<GObject> weak (image);
  Connection connection;
  if (image) connection = connect (owner, image, "disconnect", G_CALLBACK (source_disconnected));
  source_image_connection = std::move (connection);
  source_image = std::move (weak);
  if (updating)
    { ++reference_generation; defer_refresh (); }
}
void CloneImpl::set_source (GimpLayer *layer, bool emit_update)
{
  if (layer && !GIMP_IS_LAYER (layer))
    throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Clone source must be a layer");
  if (!owner_image.lock ())
    {
      BindingStore::require (G_OBJECT (owner)).close ();
      throw Error (GIMP_PAINTER_ERROR_CLOSED, "Clone owning image is closed");
    }
  /* Stage allocations and connections before replacing the live reference. */
  auto ref = ObjectRef<GObject>::retain (G_OBJECT (layer));
  WeakRef<GObject> replacement (ref);
  Connection new_update, new_freeze, new_name, new_disconnect, new_image_changed, new_image_disconnect;
  WeakRef<GObject> new_image;
  if (layer)
    {
      new_update = connect (owner, ref, "update", G_CALLBACK (source_update));
      new_freeze = connect (owner, ref, "notify::frozen", G_CALLBACK (source_frozen));
      new_name = connect (owner, ref, "name-changed", G_CALLBACK (source_renamed));
      new_disconnect = connect (owner, ref, "disconnect", G_CALLBACK (source_disconnected));
      new_image_changed = connect (owner, ref, "notify::image", G_CALLBACK (source_image_changed));
      auto image = ObjectRef<GObject>::retain (G_OBJECT (gimp_item_get_image (GIMP_ITEM (layer))));
      new_image = WeakRef<GObject> (image);
      if (image) new_image_disconnect = connect (owner, image, "disconnect", G_CALLBACK (source_disconnected));
    }
  update_connection = std::move (new_update);
  freeze_connection = std::move (new_freeze);
  name_connection = std::move (new_name);
  disconnect_connection = std::move (new_disconnect);
  source_image_changed_connection = std::move (new_image_changed);
  source_image_connection = std::move (new_image_disconnect);
  source_image = std::move (new_image);
  source = std::move (replacement);
  const auto generation = ++reference_generation;
  const auto binding_generation = BindingStore::require (G_OBJECT (owner)).generation ();
  const auto current = [&] {
    auto *store = BindingStore::find (G_OBJECT (owner));
    return reference_generation == generation && store && store->accepts (binding_generation);
  };
  source_expired = false;
  if (mirrored_freeze)
    { mirrored_freeze = false; gimp_viewable_preview_thaw (GIMP_VIEWABLE (owner)); }
  if (!current () || !layer) return; // legacy detach keeps cached pixels
  remember_name ();
  previous_x = gimp_item_get_offset_x (GIMP_ITEM (layer));
  previous_y = gimp_item_get_offset_y (GIMP_ITEM (layer));
  previous_width = gimp_item_get_width (GIMP_ITEM (layer));
  previous_height = gimp_item_get_height (GIMP_ITEM (layer));
  mirror_freeze (G_OBJECT (layer));
  if (!current ()) return;
  /* Metadata restoration deliberately skips this legacy side effect. */
  if (!emit_update) return;
  /* This is an observable legacy side effect, not just a private refresh. */
  gimp_drawable_update (GIMP_DRAWABLE (layer), 0, 0, previous_width, previous_height);
  if (current () && updating) defer_refresh ();
}

void CloneImpl::defer_refresh ()
{
  if (pending_refresh.active ()) return;
  auto token = std::make_shared<Callback> (owner);
  pending_refresh = Source::idle (nullptr, G_PRIORITY_DEFAULT_IDLE, [token] {
    visit_callback (token.get (), [] (CloneImpl& impl) {
      auto ref = impl.source.lock ();
      if (ref) impl.update (GIMP_DRAWABLE (ref.get ()), 0, 0, -1, -1);
    });
    return false;
  });
}

bool depends_on (GimpLayer *layer, GimpLayer *target, std::set<GimpLayer *>& seen)
{
  if (layer == target) return true;
  if (!seen.insert (layer).second) return false;
  if (GIMP_IS_CLONE_LAYER (layer))
    {
      auto *store = BindingStore::find (G_OBJECT (layer));
      if (store)
        {
          auto ref = store->read<CloneSlot> ([] (const CloneImpl& impl) { return impl.source.lock (); });
          if (ref && depends_on (GIMP_LAYER (ref.get ()), target, seen)) return true;
        }
    }
  GimpContainer *children = gimp_viewable_get_children (GIMP_VIEWABLE (layer));
  if (children)
    for (gint i = 0; i < gimp_container_get_n_children (children); ++i)
      if (depends_on (GIMP_LAYER (gimp_container_get_child_by_index (children, i)), target, seen)) return true;
  return false;
}

bool project (GimpLayer *layer, GeglBuffer *destination, const GeglRectangle& rect,
              const std::function<bool ()>& current)
{
  /* Flushing a group can synchronously emit updates and replace/close the
   * clone's source. Check the reference generation before borrowing metadata. */
  gimp_pickable_flush (GIMP_PICKABLE (layer));
  if (!current ()) return false;
  GimpDrawable *drawable = GIMP_DRAWABLE (layer);
  GimpImage *image = gimp_item_get_image (GIMP_ITEM (layer));
  GimpLayerMask *mask = gimp_layer_get_mask (layer);
  const bool show = mask && gimp_layer_get_show_mask (layer);
  const bool apply = mask && gimp_layer_get_apply_mask (layer);
  const bool gray = gimp_drawable_is_gray (drawable);
  const bool indexed = gimp_drawable_is_indexed (drawable);
  const bool alpha = gimp_drawable_has_alpha (drawable);
  const bool byte = gimp_drawable_get_component_type (drawable) == GIMP_COMPONENT_TYPE_U8;
  const bool dissolve = gimp_layer_get_mode (layer) == GIMP_LAYER_MODE_DISSOLVE;
  gboolean visible[MAX_CHANNELS] = { TRUE, TRUE, TRUE, TRUE };
  if (image) gimp_image_get_visible_array (image, visible);
  auto source = ObjectRef<GObject>::adopt (G_OBJECT (gimp_drawable_get_buffer_with_effects (drawable)));
  auto mask_buffer = ObjectRef<GObject>::retain (mask ? G_OBJECT (gimp_drawable_get_buffer (GIMP_DRAWABLE (mask))) : nullptr);
  const Babl *space = babl_format_get_space (gimp_drawable_get_format (drawable));
  /* Legacy u8 arithmetic stays byte-exact. New higher precision preserves
   * the source TRC and uses double intermediates, including DOUBLE images. */
  const Babl *format = gimp_babl_format (gray ? GIMP_GRAY : GIMP_RGB,
                     gimp_babl_precision (byte ? GIMP_COMPONENT_TYPE_U8 : GIMP_COMPONENT_TYPE_DOUBLE,
                                          gimp_drawable_get_trc (drawable)),
                     TRUE, space);
  const Babl *mask_format = babl_format (byte ? "Y u8" : "Y double");
  const size_t channels = gray ? 2 : 4;
  const unsigned opacity = static_cast<unsigned> (gimp_layer_get_opacity (layer) * 255.999);
  const auto multiply_size = [] (size_t a, size_t b) {
    if (a > std::numeric_limits<size_t>::max () / b)
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Clone projection row size overflow");
    return a * b;
  };
  const size_t samples = multiply_size (static_cast<size_t> (rect.width), channels);
  const size_t total_bytes = multiply_size (static_cast<size_t> (rect.width),
                                            (channels + 1) * (byte ? 1 : sizeof (double)));
  if (total_bytes > 64u * 1024u * 1024u)
    throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Clone projection row exceeds 64 MiB budget");
  std::vector<guchar> byte_row (byte ? samples : 0), byte_mask (byte ? rect.width : 0);
  std::vector<double> double_row (byte ? 0 : samples), double_mask (byte ? 0 : rect.width);
  void *row = byte ? static_cast<void *> (byte_row.data ()) : double_row.data ();
  void *mask_row = byte ? static_cast<void *> (byte_mask.data ()) : double_mask.data ();
  for (int y = rect.y; y < rect.y + rect.height; ++y)
    {
      GeglRectangle line = { rect.x, y, rect.width, 1 };
      gegl_buffer_get (GEGL_BUFFER (source.get ()), &line, 1.0, format, row, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      if (mask && (show || apply))
        gegl_buffer_get (GEGL_BUFFER (mask_buffer.get ()), &line, 1.0, mask_format, mask_row, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      GRand *random = dissolve ? g_rand_new_with_seed (dissolve_seeds ()[y % 4096]) : nullptr;
      if (random) for (int x = 0; x < rect.x; ++x) g_rand_int (random);
      for (int x = 0; x < rect.width; ++x)
        {
          if (byte)
            {
              auto *p = byte_row.data () + static_cast<size_t> (x) * channels;
              unsigned a = p[channels - 1], m = apply ? byte_mask[x] : 255;
              if (show)
                { for (size_t c = 0; c < channels - 1; ++c) p[c] = byte_mask[x]; a = 255; }
              else if (indexed)
                { a = alpha ? (multiply3 (opacity, a, m) > 127 ? 255 : 0) : 255; }
              else
                {
                  if (dissolve)
                    a = g_rand_int_range (random, 0, 255) >= static_cast<int> (opacity * a * m / (255 * 255)) ? 0 : 255;
                  else a = apply ? (alpha ? multiply3 (opacity, a, m) : multiply (opacity, m)) : multiply (opacity, a);
                  for (size_t c = 0; c < channels - 1; ++c) if (!visible[c]) p[c] = 0;
                  if (!visible[channels - 1]) a = 0;
                }
              p[channels - 1] = a;
            }
          else
            {
              auto *p = double_row.data () + static_cast<size_t> (x) * channels;
              auto *m = double_mask.data ();
              if (show)
                { for (size_t c = 0; c < channels - 1; ++c) p[c] = m[x]; p[channels - 1] = 1.0f; }
              else
                {
                  p[channels - 1] *= gimp_layer_get_opacity (layer) * (apply ? m[x] : 1.0);
                  if (dissolve) p[channels - 1] = g_rand_int_range (random, 0, 255) >= p[channels - 1] * 255 ? 0 : 1;
                  for (size_t c = 0; c < channels; ++c) if (!visible[c]) p[c] = 0;
                }
            }
        }
      if (random) g_rand_free (random);
      gegl_buffer_set (destination, &line, 0, format, row, GEGL_AUTO_ROWSTRIDE);
    }
  return true;
}

void CloneImpl::update (GimpDrawable *drawable, gint x, gint y, gint width, gint height, bool resolve_name)
{
  if (updating || restoring) return;
  /* Legacy update_size() calls get_source(): an update can trigger a pending
   * name resolution too, including the update emitted by a direct setter. */
  if (resolve_name && has_pending_name && !resolving) get_source ();
  auto ref = source.lock ();
  if (!ref || ref.get () != G_OBJECT (drawable)) return;
  std::set<GimpLayer *> seen;
  if (depends_on (GIMP_LAYER (drawable), GIMP_LAYER (owner), seen)) return;
  Flag guard (updating);
  const auto generation = reference_generation;
  const int w = gimp_item_get_width (GIMP_ITEM (drawable));
  const int h = gimp_item_get_height (GIMP_ITEM (drawable));
  if (w != gimp_item_get_width (GIMP_ITEM (owner)) || h != gimp_item_get_height (GIMP_ITEM (owner)))
    {
      /* Legacy update_size() calls the parent resize directly: when attached
       * its cache resize records a DrawableModUndo, even during undo replay.
       * Keep the measured undo order/offset effects rather than silently
       * substituting a history-free derived-cache resize. */
      auto buffer = ObjectRef<GObject>::adopt (G_OBJECT (gegl_buffer_new (GEGL_RECTANGLE (0, 0, w, h),
                                                         gimp_drawable_get_format (GIMP_DRAWABLE (owner)))));
      gegl_buffer_copy (gimp_drawable_get_buffer (GIMP_DRAWABLE (owner)), nullptr, GEGL_ABYSS_NONE,
                        GEGL_BUFFER (buffer.get ()), nullptr);
      gimp_drawable_set_buffer (GIMP_DRAWABLE (owner), !editing_reference && gimp_item_is_attached (GIMP_ITEM (owner)),
                                nullptr, GEGL_BUFFER (buffer.get ()));
      if (reference_generation != generation) return;
      /* The old automatic path passed NULL context. Its mask resize was
       * rejected, leaving the clone-owned mask dimensions and offset intact.
       * Preserve that result without deliberately invoking an invalid API. */
    }
  if (reference_generation != generation) return;
  const int sx = gimp_item_get_offset_x (GIMP_ITEM (drawable));
  const int sy = gimp_item_get_offset_y (GIMP_ITEM (drawable));
  if (previous_width != w || previous_height != h)
    {
      gimp_item_set_offset (GIMP_ITEM (owner),
                            gimp_item_get_offset_x (GIMP_ITEM (owner)) + sx - previous_x,
                            gimp_item_get_offset_y (GIMP_ITEM (owner)) + sy - previous_y);
      if (reference_generation != generation) return;
      previous_width = w; previous_height = h;
    }
  previous_x = sx; previous_y = sy;
  GeglRectangle requested = { x, y, width < 0 ? w : width, height < 0 ? h : height };
  GeglRectangle extent = { 0, 0, w, h }, clipped;
  if (!gegl_rectangle_intersect (&clipped, &requested, &extent)) return;
  if (reference_generation != generation) return;
  auto projected = ObjectRef<GObject>::adopt (G_OBJECT (gegl_buffer_new (&clipped,
    gimp_drawable_get_format (GIMP_DRAWABLE (owner)))));
  if (!project (GIMP_LAYER (drawable), GEGL_BUFFER (projected.get ()), clipped,
                [&] { return reference_generation == generation; })) return;
  if (reference_generation != generation) return;
  gegl_buffer_copy (GEGL_BUFFER (projected.get ()), &clipped, GEGL_ABYSS_NONE,
                    gimp_drawable_get_buffer (GIMP_DRAWABLE (owner)), &clipped);
  /* GeglBuffer changed invalidates inherited gimp:buffer-source-validate;
   * drawable update propagates the local ROI to parent/image projections. */
  gimp_drawable_update (GIMP_DRAWABLE (owner), clipped.x, clipped.y, clipped.width, clipped.height);
}

ReferenceState CloneImpl::capture () const
{
  ReferenceState state;
  auto ref = source.lock ();
  auto source_lease = ObjectRef<GimpLayer>::adopt (ref ? GIMP_LAYER (ref.release ()) : nullptr);
  state.had_source = static_cast<bool> (source_lease);
  state.source = WeakRef<GimpLayer> (source_lease);
  state.source_image = WeakRef<GObject> (source_image.lock ());
  auto *buffer = gimp_drawable_get_buffer (GIMP_DRAWABLE (owner));
  if (!buffer) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Clone has no pixel buffer");
  state.buffer = ObjectRef<GObject>::adopt (G_OBJECT (gimp_gegl_buffer_dup (buffer)));
  state.pending_name = pending_name;
  state.record_name = record_name;
  state.pending = has_pending_name;
  state.allow_name_lookup = allow_name_lookup;
  state.expired = source_expired;
  state.x = gimp_item_get_offset_x (GIMP_ITEM (owner));
  state.y = gimp_item_get_offset_y (GIMP_ITEM (owner));
  state.previous_x = previous_x; state.previous_y = previous_y;
  state.previous_w = previous_width; state.previous_h = previous_height;
  return state;
}

bool CloneImpl::restore (const ReferenceState& state)
{
  std::string restored_pending = state.pending_name, restored_record = state.record_name;
  auto& store = BindingStore::require (G_OBJECT (owner));
  const auto binding_generation = store.generation ();
  Flag edit (editing_reference);
  auto restored_source = state.source.lock ();
  auto restored_image = state.source_image.lock ();
  if (!restored_image) restored_source.reset ();
  {
    Flag restore_guard (restoring);
    const auto expected = reference_generation + 1;
    set_source (restored_source.get ());
    if (reference_generation != expected || !store.accepts (binding_generation))
      {
        if (store.accepts (binding_generation)) defer_refresh ();
        return false;
      }
    pending_name.swap (restored_pending);
    if (!restored_source) record_name.swap (restored_record);
    has_pending_name = state.pending;
    allow_name_lookup = state.allow_name_lookup;
    source_expired = state.expired || (state.had_source && !restored_source);
    previous_x = state.previous_x; previous_y = state.previous_y;
    previous_width = state.previous_w; previous_height = state.previous_h;
    GeglRectangle bounds = { state.x, state.y, 0, 0 };
    gimp_drawable_set_buffer_full (GIMP_DRAWABLE (owner), FALSE, nullptr,
                                  GEGL_BUFFER (state.buffer.get ()), &bounds, TRUE);
    if (reference_generation != expected || !store.accepts (binding_generation))
      {
        if (store.accepts (binding_generation)) defer_refresh ();
        return false;
      }
  }
  /* A live restored reference reflects its source's current pixels. Avoid
   * resolving a pending name merely as a side effect of replay: the complete
   * pending+live distinction is restored until a real update/getter occurs. */
  const auto restored_generation = reference_generation;
  if (restored_source) update (GIMP_DRAWABLE (restored_source.get ()), 0, 0, -1, -1, false);
  return reference_generation == restored_generation && store.accepts (binding_generation);
}

struct ReferenceUndoImpl
{
  ReferenceState saved;
  Connection source_image_changed_connection;
  void watch_source (GObject *owner);
  void close () noexcept
  {
    source_image_changed_connection.close ();
    saved.source.reset (); saved.source_image.reset (); saved.buffer.reset ();
    saved.pending_name.clear (); saved.record_name.clear ();
  }
};
struct ReferenceUndoSlot : SlotSpec<GimpCloneLayerUndo, ReferenceUndoImpl> {};
struct UndoCallback
{
  explicit UndoCallback (GObject *object) : owner (ObjectRef<GObject>::retain (object)),
    generation (BindingStore::require (object).generation ()) {}
  WeakRef<GObject> owner;
  std::uint64_t generation;
};
void undo_callback_free (gpointer data, GClosure *) { delete static_cast<UndoCallback *> (data); }
void undo_source_image_changed (GObject *source, GParamSpec *, gpointer data)
{
  boundary_void (nullptr, [&] {
    auto *callback = static_cast<UndoCallback *> (data);
    auto owner = callback->owner.lock ();
    if (!owner) return;
    auto *store = BindingStore::find (owner.get ());
    if (!store || !store->accepts (callback->generation)) return;
    store->with<ReferenceUndoSlot> ([&] (ReferenceUndoImpl& impl) {
      auto current = impl.saved.source.lock ();
      if (G_OBJECT (current.get ()) == source)
        impl.saved.source_image = WeakRef<GObject> (ObjectRef<GObject>::retain (
          G_OBJECT (gimp_item_get_image (GIMP_ITEM (source)))));
    });
  });
}
void ReferenceUndoImpl::watch_source (GObject *owner)
{
  auto source = saved.source.lock ();
  Connection connection;
  if (source)
    {
      std::unique_ptr<UndoCallback> token (new UndoCallback (owner));
      connection = Connection::connect (ObjectRef<GObject>::retain (G_OBJECT (source.get ())),
        "notify::image", G_CALLBACK (undo_source_image_changed), token.get (), undo_callback_free);
      token.release ();
    }
  source_image_changed_connection = std::move (connection);
}
void reference_undo_constructed (GObject *object)
{
  G_OBJECT_CLASS (gimp_clone_layer_undo_parent_class)->constructed (object);
  auto *undo = GIMP_CLONE_LAYER_UNDO (object);
  if (!undo->binding_failed)
    undo->binding_failed = !boundary<gboolean> (nullptr, FALSE, [&] {
      GimpItem *item = GIMP_ITEM_UNDO (object)->item;
      if (!GIMP_IS_CLONE_LAYER (item) || GIMP_UNDO (object)->undo_type != GIMP_UNDO_CLONE_LAYER_SOURCE)
        throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Clone reference Undo requires a CloneLayer");
      auto& store = BindingStore::require (object);
      store.initialize<ReferenceUndoSlot> ([&] (ReferenceUndoImpl& snapshot) {
        snapshot.saved = BindingStore::require (G_OBJECT (item)).with<CloneSlot> (
          [] (CloneImpl& impl) { return impl.capture (); });
      });
      store.activate ();
      store.with<ReferenceUndoSlot> ([&] (ReferenceUndoImpl& impl) { impl.watch_source (object); });
      return TRUE;
    });
  if (undo->binding_failed) gimp_painter_binding_close (object, nullptr);
}
void reference_undo_dispose (GObject *object)
{
  gimp_painter_binding_close (object, nullptr);
  G_OBJECT_CLASS (gimp_clone_layer_undo_parent_class)->dispose (object);
}
void reference_undo_free (GimpUndo *undo, GimpUndoMode mode)
{
  gimp_painter_binding_close (G_OBJECT (undo), nullptr);
  GIMP_UNDO_CLASS (gimp_clone_layer_undo_parent_class)->free (undo, mode);
}
void reference_undo_pop (GimpUndo *undo, GimpUndoMode mode, GimpUndoAccumulator *accum)
{
  boundary_void (nullptr, [&] {
    auto *store = BindingStore::find (G_OBJECT (undo));
    if (!store || store->state () != BindingStore::State::active) return;
    const auto generation = store->generation ();
    store->with<ReferenceUndoSlot> ([&] (ReferenceUndoImpl& snapshot) {
      auto target = ObjectRef<GObject>::retain (G_OBJECT (GIMP_ITEM_UNDO (undo)->item));
      auto *target_store = target ? BindingStore::find (target.get ()) : nullptr;
      if (!target_store || target_store->state () != BindingStore::State::active) return;
      GIMP_UNDO_CLASS (gimp_clone_layer_undo_parent_class)->pop (undo, mode, accum);
      if (!store->accepts (generation)) return;
      target_store->with<CloneSlot> ([&] (CloneImpl& impl) {
        ReferenceState current = impl.capture ();
        if (impl.restore (snapshot.saved))
          { snapshot.saved = std::move (current); snapshot.watch_source (G_OBJECT (undo)); }
      });
    });
  });
}
gint64 reference_undo_memsize (GimpObject *object, gint64 *gui)
{
  return boundary<gint64> (nullptr, 0, [&] {
    gint64 memory = BindingStore::require (G_OBJECT (object)).read<ReferenceUndoSlot> (
      [] (const ReferenceUndoImpl& impl) -> gint64 {
        auto *buffer = GEGL_BUFFER (impl.saved.buffer.get ());
        return impl.saved.pending_name.capacity () + impl.saved.record_name.capacity () +
          (buffer ? static_cast<gint64> (gegl_buffer_get_width (buffer)) * gegl_buffer_get_height (buffer) *
                    babl_format_get_bytes_per_pixel (gegl_buffer_get_format (buffer)) : 0);
      });
    return memory + GIMP_OBJECT_CLASS (gimp_clone_layer_undo_parent_class)->get_memsize (object, gui);
  });
}

void constructed (GObject *object)
{
  if (G_OBJECT_CLASS (gimp_clone_layer_parent_class)->constructed)
    G_OBJECT_CLASS (gimp_clone_layer_parent_class)->constructed (object);
  auto *layer = GIMP_CLONE_LAYER (object);
  if (!layer->binding_failed)
    layer->binding_failed = !boundary<gboolean> (nullptr, FALSE, [&] {
      auto& store = BindingStore::require (object);
      store.initialize<CloneSlot> ([] (CloneImpl&) {}); // require the registered slot
      store.activate ();
      store.with<CloneSlot> ([] (CloneImpl& impl) { impl.watch_owner_image (); });
      return TRUE;
    });
  if (layer->binding_failed) gimp_painter_binding_close (object, nullptr);
}
void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *spec)
{
  if (property_id == 1) g_value_set_boolean (value, GIMP_CLONE_LAYER (object)->binding_failed);
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
}
void dispose (GObject *object)
{
  gimp_painter_binding_close (object, nullptr);
  G_OBJECT_CLASS (gimp_clone_layer_parent_class)->dispose (object);
}
GimpItem *duplicate (GimpItem *item, GType type)
{
  return boundary<GimpItem *> (nullptr, nullptr, [&] {
    GimpItem *copy = GIMP_ITEM_CLASS (gimp_clone_layer_parent_class)->duplicate (item, type);
    if (GIMP_IS_CLONE_LAYER (copy))
      {
        auto *original = GIMP_CLONE_LAYER (item);
        GimpLayer *source = gimp_clone_layer_get_source (original);
        gimp_clone_layer_set_source (GIMP_CLONE_LAYER (copy), source);
        /* Retain pending/live/expired metadata and protected stable-ID policy.
         * The legacy getter above still performs allowed lazy resolution. */
        std::unique_ptr<GimpCloneLayerReference, decltype (&gimp_clone_layer_reference_free)>
          reference (gimp_clone_layer_dup_reference (original, nullptr), gimp_clone_layer_reference_free);
        if (reference)
          BindingStore::require (G_OBJECT (copy)).with<CloneSlot> ([&] (CloneImpl& impl) {
            impl.has_pending_name = reference->pending_name != nullptr;
            impl.pending_name = reference->pending_name ? reference->pending_name : "";
            impl.record_name = reference->source_name ? reference->source_name : "";
            impl.source_expired = reference->source_expired;
            impl.allow_name_lookup = reference->allow_name_lookup;
          });
      }
    return copy;
  });
}
gboolean content_locked (GimpItem *item, GimpItem **locked)
{ if (locked) *locked = item; return TRUE; }
gdouble opacity_at (GimpPickable *, gint, gint) { return 0.0; }
void scale (GimpItem *, gint, gint, gint, gint, GimpInterpolationType, GimpProgress *) {}
void flip (GimpItem *, GimpContext *, GimpOrientationType, gdouble, gboolean) {}
void rotate (GimpItem *, GimpContext *, GimpRotationType, gdouble, gdouble, gboolean) {}
void transform (GimpItem *, GimpContext *, const GimpMatrix3 *, GimpTransformDirection,
                GimpInterpolationType, GimpTransformResize, GimpProgress *) {}
} // namespace

static void gimp_clone_layer_undo_class_init (GimpCloneLayerUndoClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = reference_undo_constructed;
  G_OBJECT_CLASS (klass)->dispose = reference_undo_dispose;
  GIMP_OBJECT_CLASS (klass)->get_memsize = reference_undo_memsize;
  GIMP_UNDO_CLASS (klass)->pop = reference_undo_pop;
  GIMP_UNDO_CLASS (klass)->free = reference_undo_free;
}
static void gimp_clone_layer_undo_init (GimpCloneLayerUndo *undo)
{
  undo->binding_failed = !boundary<gboolean> (nullptr, FALSE, [&] {
    BindingStore::ensure (G_OBJECT (undo)).emplace<ReferenceUndoSlot> ();
    return TRUE;
  });
}

static void gimp_clone_layer_class_init (GimpCloneLayerClass *klass)
{
  auto *object = G_OBJECT_CLASS (klass);
  auto *item = GIMP_ITEM_CLASS (klass);
  object->constructed = constructed;
  object->get_property = get_property;
  g_object_class_install_property (object, 1,
    g_param_spec_boolean ("binding-failed", "Binding failed", "C++ state construction failed; object is inert",
                           FALSE, G_PARAM_READABLE));
  object->dispose = dispose;
  item->duplicate = duplicate;
  item->is_content_locked = content_locked;
  item->scale = scale;
  item->flip = flip;
  item->rotate = rotate;
  item->transform = transform;
  GIMP_VIEWABLE_CLASS (klass)->default_icon_name = "edit-copy";
  GIMP_VIEWABLE_CLASS (klass)->default_name = "Clone Layer";
}
static void gimp_clone_layer_init (GimpCloneLayer *layer)
{
  layer->binding_failed = !boundary<gboolean> (nullptr, FALSE, [&] {
    BindingStore::ensure (G_OBJECT (layer)).emplace<CloneSlot> (layer);
    return TRUE;
  });
}
static void gimp_clone_layer_pickable_init (GimpPickableInterface *iface)
{ iface->get_opacity_at = opacity_at; }

GimpLayer *gimp_clone_layer_new (GimpImage *image, GimpLayer *source, gint width, gint height,
                                const gchar *name, gdouble opacity, GimpLayerMode mode)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (image), nullptr);
  g_return_val_if_fail (!source || GIMP_IS_LAYER (source), nullptr);
  return boundary<GimpLayer *> (nullptr, nullptr, [&] {
    if (width <= 0) width = source ? gimp_item_get_width (GIMP_ITEM (source)) : gimp_image_get_width (image);
    if (height <= 0) height = source ? gimp_item_get_height (GIMP_ITEM (source)) : gimp_image_get_height (image);
    const int x = source ? gimp_item_get_offset_x (GIMP_ITEM (source)) : 0;
    const int y = source ? gimp_item_get_offset_y (GIMP_ITEM (source)) : 0;
    auto *layer = GIMP_CLONE_LAYER (gimp_drawable_new (GIMP_TYPE_CLONE_LAYER, image, name,
                                                      x, y, width, height, gimp_image_get_layer_format (image, TRUE)));
    if (!layer) return static_cast<GimpLayer *> (nullptr);
    auto partial = ObjectRef<GObject>::adopt (G_OBJECT (layer));
    if (layer->binding_failed) return static_cast<GimpLayer *> (nullptr);
    BindingStore::require (G_OBJECT (layer)).with<CloneSlot> ([] (CloneImpl&) {});
    gimp_layer_set_opacity (GIMP_LAYER (layer), opacity, FALSE);
    gimp_layer_set_mode (GIMP_LAYER (layer), mode, FALSE);
    BindingStore::require (G_OBJECT (layer)).with<CloneSlot> ([&] (CloneImpl& impl) { impl.set_source (source); });
    return GIMP_LAYER (partial.release ());
  });
}
gboolean gimp_clone_layer_set_source_full (GimpCloneLayer *layer, GimpLayer *source, GError **error)
{
  return boundary<gboolean> (error, FALSE, [&] {
    if (!GIMP_IS_CLONE_LAYER (layer)) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected CloneLayer");
    BindingStore::require (G_OBJECT (layer)).with<CloneSlot> ([&] (CloneImpl& impl) { impl.set_source (source); });
    return TRUE;
  });
}
void gimp_clone_layer_set_source (GimpCloneLayer *layer, GimpLayer *source)
{ gimp_clone_layer_set_source_full (layer, source, nullptr); }
GimpLayer *gimp_clone_layer_get_source (GimpCloneLayer *layer)
{
  return boundary<GimpLayer *> (nullptr, nullptr, [&] {
    if (!GIMP_IS_CLONE_LAYER (layer)) return static_cast<GimpLayer *> (nullptr);
    return BindingStore::require (G_OBJECT (layer)).with<CloneSlot> ([] (CloneImpl& impl) { return impl.get_source (); });
  });
}
gboolean gimp_clone_layer_set_source_name_full (GimpCloneLayer *layer, const gchar *name, GError **error)
{
  return boundary<gboolean> (error, FALSE, [&] {
    if (!GIMP_IS_CLONE_LAYER (layer)) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected CloneLayer");
    BindingStore::require (G_OBJECT (layer)).with<CloneSlot> ([&] (CloneImpl& impl) { impl.set_name (name); });
    return TRUE;
  });
}
void gimp_clone_layer_set_source_by_name (GimpCloneLayer *layer, const gchar *name)
{ gimp_clone_layer_set_source_name_full (layer, name, nullptr); }
gchar *gimp_clone_layer_dup_source_name (GimpCloneLayer *layer)
{
  return boundary<gchar *> (nullptr, nullptr, [&] {
    if (!GIMP_IS_CLONE_LAYER (layer)) return static_cast<gchar *> (nullptr);
    return BindingStore::require (G_OBJECT (layer)).read<CloneSlot> ([] (const CloneImpl& impl) { return impl.dup_name (); });
  });
}

GimpCloneSourceState gimp_clone_layer_get_source_state (GimpCloneLayer *layer)
{
  return boundary<GimpCloneSourceState> (nullptr, GIMP_CLONE_SOURCE_NONE, [&] {
    if (!GIMP_IS_CLONE_LAYER (layer)) return GIMP_CLONE_SOURCE_NONE;
    return BindingStore::require (G_OBJECT (layer)).read<CloneSlot> ([] (const CloneImpl& impl) {
      if (impl.has_pending_name) return GIMP_CLONE_SOURCE_PENDING;
      if (impl.source.lock ()) return GIMP_CLONE_SOURCE_LIVE;
      return impl.source_expired ? GIMP_CLONE_SOURCE_EXPIRED : GIMP_CLONE_SOURCE_NONE;
    });
  });
}

namespace {
class ReferenceEditGroup
{
public:
  ReferenceEditGroup (GimpImage *image, const char *description)
    : image_ (ObjectRef<GObject>::retain (G_OBJECT (image)))
  {
    if (image) begun_ = gimp_image_undo_group_start (image, GIMP_UNDO_GROUP_ITEM_PROPERTIES, description);
  }
  ~ReferenceEditGroup () noexcept
  { if (begun_) gimp_image_undo_group_end (GIMP_IMAGE (image_.get ())); }
private:
  ObjectRef<GObject> image_;
  bool begun_ = false;
};

gboolean edit_reference (GimpCloneLayer *layer, GimpLayer *source, const gchar *name,
                         bool by_name, const gchar *description, GError **error)
{
  return boundary<gboolean> (error, FALSE, [&] {
    if (!GIMP_IS_CLONE_LAYER (layer) || (!by_name && source && !GIMP_IS_LAYER (source)))
      throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected CloneLayer and layer source");
    std::string copied_name = name ? name : "";
    BindingStore::require (G_OBJECT (layer)).with<CloneSlot> ([&] (CloneImpl& impl) {
      if (!gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)))
        throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Clone has no pixel buffer");
      auto& store = BindingStore::require (G_OBJECT (layer));
      const auto generation = store.generation ();
      const auto reference_generation = impl.reference_generation;
      if (impl.restoring)
        throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Reference edit during Undo replay");
      const bool attached = gimp_item_is_attached (GIMP_ITEM (layer));
      /* Other clones can resize synchronously when this clone's buffer changes.
       * Keep those legacy cache records in the same user-visible operation. */
      ReferenceEditGroup group (attached ? gimp_item_get_image (GIMP_ITEM (layer)) : nullptr,
                                 description ? description : "Clone layer source");
      if (!store.accepts (generation) || impl.reference_generation != reference_generation)
        throw Error (GIMP_PAINTER_ERROR_CLOSED, "Clone changed while starting reference edit");
      if (attached)
        {
          GimpUndo *undo = gimp_image_undo_push (gimp_item_get_image (GIMP_ITEM (layer)),
            GIMP_TYPE_CLONE_LAYER_UNDO, GIMP_UNDO_CLONE_LAYER_SOURCE,
            description ? description : "Clone layer source",
            GimpDirtyMask (GIMP_DIRTY_ITEM | GIMP_DIRTY_ITEM_META | GIMP_DIRTY_DRAWABLE),
            "item", layer, nullptr);
          if (undo && GIMP_CLONE_LAYER_UNDO (undo)->binding_failed)
            throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Clone reference Undo construction failed");
        }
      if (!store.accepts (generation))
        throw Error (GIMP_PAINTER_ERROR_CLOSED, "Clone closed during reference Undo creation");
      if (impl.reference_generation != reference_generation)
        throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Clone source replaced during Undo creation");
      Flag edit (impl.editing_reference);
      if (by_name) impl.set_name (name ? copied_name.c_str () : nullptr);
      else impl.set_source (source);
    });
    return TRUE;
  });
}
}
gboolean gimp_clone_layer_set_source_with_undo (GimpCloneLayer *layer, GimpLayer *source,
                                               const gchar *description, GError **error)
{ return edit_reference (layer, source, nullptr, false, description, error); }
gboolean gimp_clone_layer_set_source_name_with_undo (GimpCloneLayer *layer, const gchar *name,
                                                    const gchar *description, GError **error)
{ return edit_reference (layer, nullptr, name, true, description, error); }

GimpCloneLayerReference *gimp_clone_layer_dup_reference (GimpCloneLayer *layer, GError **error)
{
  return boundary<GimpCloneLayerReference *> (error, nullptr, [&] {
    if (!GIMP_IS_CLONE_LAYER (layer)) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected CloneLayer");
    return BindingStore::require (G_OBJECT (layer)).read<CloneSlot> ([] (const CloneImpl& impl) {
      std::unique_ptr<GimpCloneLayerReference, decltype (&gimp_clone_layer_reference_free)>
        result (g_new0 (GimpCloneLayerReference, 1), gimp_clone_layer_reference_free);
      auto source = impl.source.lock ();
      auto source_image = impl.source_image.lock ();
      const bool expired = impl.source_expired || (source && !source_image);
      if (!source_image) source.reset ();
      result->source = source ? GIMP_LAYER (source.release ()) : nullptr;
      result->pending_name = impl.has_pending_name ? g_strdup (impl.pending_name.c_str ()) : nullptr;
      const char *name = result->source ? gimp_object_get_name (result->source) : impl.record_name.c_str ();
      result->source_name = g_strdup (name ? name : "");
      result->source_expired = expired;
      result->allow_name_lookup = impl.allow_name_lookup;
      result->state = impl.has_pending_name ? GIMP_CLONE_SOURCE_PENDING :
        result->source ? GIMP_CLONE_SOURCE_LIVE : expired ? GIMP_CLONE_SOURCE_EXPIRED : GIMP_CLONE_SOURCE_NONE;
      return result.release ();
    });
  });
}
void gimp_clone_layer_reference_free (GimpCloneLayerReference *reference)
{
  if (!reference) return;
  g_clear_object (&reference->source);
  g_free (reference->pending_name);
  g_free (reference->source_name);
  g_free (reference);
}

gboolean gimp_clone_layer_restore_reference (GimpCloneLayer *layer,
                                             const GimpCloneLayerReference *reference,
                                             GError **error)
{
  return boundary<gboolean> (error, FALSE, [&] {
    if (!GIMP_IS_CLONE_LAYER (layer) || !reference ||
        (reference->source && !GIMP_IS_LAYER (reference->source)))
      throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected CloneLayer reference metadata");
    const GimpCloneSourceState expected_state = reference->pending_name ? GIMP_CLONE_SOURCE_PENDING :
      reference->source ? GIMP_CLONE_SOURCE_LIVE : reference->source_expired ? GIMP_CLONE_SOURCE_EXPIRED : GIMP_CLONE_SOURCE_NONE;
    if (reference->state != expected_state || (reference->source && reference->source_expired))
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Inconsistent Clone reference metadata");
    std::string pending = reference->pending_name ? reference->pending_name : "";
    std::string recorded = reference->source_name ? reference->source_name : "";
    BindingStore::require (G_OBJECT (layer)).with<CloneSlot> ([&] (CloneImpl& impl) {
      auto& store = BindingStore::require (G_OBJECT (layer));
      const auto generation = store.generation ();
      const auto expected = impl.reference_generation + 1;
      Flag restoring (impl.restoring);
      impl.set_source (reference->source, false);
      if (!store.accepts (generation) || impl.reference_generation != expected)
        throw Error (GIMP_PAINTER_ERROR_CLOSED, "Clone changed while restoring metadata");
      impl.pending_name.swap (pending);
      if (!reference->source) impl.record_name.swap (recorded);
      impl.has_pending_name = reference->pending_name != nullptr;
      impl.allow_name_lookup = reference->allow_name_lookup;
      impl.source_expired = reference->source_expired;
    });
    return TRUE;
  });
}
