/* SPDX-License-Identifier: GPL-3.0-or-later
 * Preserves afa43fae layer-preset construction semantics. C ABI boundaries own
 * a short-lived C++ transaction, never a fake GObject/delegator implementation.
 */
#include "config.h"
#include <gegl.h>
#include <json-glib/json-glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core-types.h"
#include "gimplayerpreset.h"
#include "gimpcontext.h"
#include "gimpimage.h"
#include "gimpimage-private.h"
#include "gimplayer-new.h"
#include "gimpimage-undo.h"
#include "gimpcontainer.h"
#include "gimpchannel.h"
#include "gimpgrouplayer.h"
#include "gimpundostack.h"
#include "gimpfilterlayer.h"
#include "gimpclonelayer.h"
#include "operations/layer-modes-legacy/gimpoperationpainterlegacy.h"
}
#include "gimp-intl.h"
#include "gimp-painter-type-traits.hpp"
#include "painter/resources.hpp"
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include <limits>
using namespace GimpPainter;
namespace {
using Node = std::unique_ptr<JsonNode, decltype (&json_node_unref)>;
[[noreturn]] void fail (const char *message) { throw std::runtime_error (message); }
JsonNode *member (JsonNode *node, const char *key)
{ return node && JSON_NODE_HOLDS_OBJECT (node) ? json_object_get_member (json_node_get_object (node), key) : nullptr; }
const char *string (JsonNode *node, const char *fallback = nullptr)
{
  if (!node || JSON_NODE_HOLDS_NULL (node)) return fallback;
  if (json_node_get_value_type (node) != G_TYPE_STRING) fail ("Expected a string in layer preset");
  return json_node_get_string (node);
}
bool equal (const char *a, const char *b) { return a && std::strcmp (a, b) == 0; }
double number (JsonNode *node)
{
  if (!node || (json_node_get_value_type (node) != G_TYPE_INT64 &&
                json_node_get_value_type (node) != G_TYPE_DOUBLE)) fail ("Expected a number in layer preset");
  double value = json_node_get_double (node);
  if (!std::isfinite (value)) fail ("Non-finite layer preset number");
  return value;
}
int integer (JsonNode *node)
{
  double value = number (node);
  if (value != std::floor (value) || value < G_MININT || value > G_MAXINT)
    fail ("Layer preset integer is out of range");
  return static_cast<int> (value);
}
bool matches (JsonNode *node, GimpLayer *source)
{
  if (!node || !JSON_NODE_HOLDS_OBJECT (node) || !GIMP_IS_LAYER (source)) return false;
  const char *type = string (member (node, "type"), "any");
  bool normal = !GIMP_IS_GROUP_LAYER (source) && !GIMP_IS_CLONE_LAYER (source) && !GIMP_IS_FILTER_LAYER (source);
  if (!(equal (type, "any") || (equal (type, "normal") && normal) ||
        (equal (type, "group") && GIMP_IS_GROUP_LAYER (source)) ||
        (equal (type, "clone") && GIMP_IS_CLONE_LAYER (source)) ||
        (equal (type, "filter") && GIMP_IS_FILTER_LAYER (source)))) return false;
  JsonNode *alpha = member (node, "alpha");
  if (alpha && (json_node_get_value_type (alpha) != G_TYPE_BOOLEAN ||
                json_node_get_boolean (alpha) != gimp_drawable_has_alpha (GIMP_DRAWABLE (source)))) return false;
  return true;
}
int offset_delta (int destination, int origin)
{
  gint64 delta = static_cast<gint64> (destination) - origin;
  if (delta < G_MININT || delta > G_MAXINT) fail ("Layer preset offset delta exceeds native integer range");
  return static_cast<int> (delta);
}
struct Bounds { int x = 0, y = 0, w = 0, h = 0; };
struct Planned {
  ObjectRef<GimpLayer> layer;
  GimpLayer *parent = nullptr;
  int index = 0;
  bool original = false;
  Bounds bounds;
  GimpLayerMode mode = static_cast<GimpLayerMode> (-1);
  double opacity = -1;
};
GimpLayerMode mode (JsonNode *node, GimpLayer *source)
{
  if (!node || JSON_NODE_HOLDS_NULL (node)) return static_cast<GimpLayerMode> (-1);
  if (json_node_get_value_type (node) != G_TYPE_STRING)
    {
      GimpLayerMode value;
      if (!gimp_painter_layer_mode_from_legacy (integer (node), &value)) fail ("Unknown numeric legacy layer mode");
      return value;
    }
  const char *name = string (node);
  if (equal (name, "source")) return gimp_layer_get_mode (source);
  static const char *names[] = {"normal", "dissolve", "behind", "multiply", "screen", "overlay", "difference", "addition", "subtract", "darken-only", "lighten-only", "hue", "saturation", "color", "value", "divide", "dodge", "burn", "hardlight", "softlight", "grain-extract", "grain-merge", "color-erase", "erase", "replace", "anti-erase", "src-in", "dst-in", "src-out", "dst-out"};
  for (guint i = 0; i < G_N_ELEMENTS (names); ++i)
    if (std::string (names[i]) + "-mode" == name)
      { GimpLayerMode result; if (gimp_painter_layer_mode_from_legacy (i, &result)) return result; }
  /* Old GLib::Enum looks up exact nicks only. In particular bundled test2's
   * "Multiply" is not a nick: leave the source unchanged/default new layer. */
  return static_cast<GimpLayerMode> (-1);
}
Bounds bounds (JsonNode *node, GimpImage *image, GimpLayer *source)
{
  Bounds b;
  if (!node || JSON_NODE_HOLDS_NULL (node)) return b;
  if (JSON_NODE_HOLDS_ARRAY (node))
    {
      JsonArray *a = json_node_get_array (node);
      if (json_array_get_length (a) != 4) fail ("Layer boundary requires four coordinates");
      b.x = integer (json_array_get_element (a, 0)); b.y = integer (json_array_get_element (a, 1));
      gint64 x2 = integer (json_array_get_element (a, 2)), y2 = integer (json_array_get_element (a, 3));
      if (x2 < b.x || y2 < b.y || x2 - b.x > GIMP_MAX_IMAGE_SIZE || y2 - b.y > GIMP_MAX_IMAGE_SIZE)
        fail ("Invalid layer boundary dimensions");
      b.w = x2 - b.x; b.h = y2 - b.y;
    }
  else
    {
      const char *name = string (node);
      if (equal (name, "source"))
        { b.x = gimp_item_get_offset_x (GIMP_ITEM (source)); b.y = gimp_item_get_offset_y (GIMP_ITEM (source));
          b.w = gimp_item_get_width (GIMP_ITEM (source)); b.h = gimp_item_get_height (GIMP_ITEM (source)); }
      else if (equal (name, "selection"))
        {
          gint x2, y2;
          gimp_item_bounds (GIMP_ITEM (gimp_image_get_mask (image)), &b.x, &b.y, &x2, &y2);
          /* gimp_item_bounds reports x/y/width/height, including full extent
           * for an empty selection, matching old channel cached bounds. */
          b.w = x2; b.h = y2;
        }
      else if (equal (name, "full"))
        { b.w = gimp_image_get_width (image); b.h = gimp_image_get_height (image); }
      else fail ("Unknown layer preset boundary");
    }
  return b;
}
void install_filter (GimpLayer *layer, JsonNode *filter)
{
  if (!filter || !JSON_NODE_HOLDS_OBJECT (filter)) fail ("Filter layer has no filter definition");
  const char *procedure = string (member (filter, "name"));
  if (!procedure || !*procedure) fail ("Filter layer has no procedure name");
  JsonNode *arguments = member (filter, "args");
  if (!arguments || !JSON_NODE_HOLDS_ARRAY (arguments)) fail ("Filter arguments must be an array");
  JsonArray *array = json_node_get_array (arguments);
  guint count = json_array_get_length (array);
  if (count > 65536) fail ("Too many filter arguments");
  std::unique_ptr<GimpValueArray, decltype (&gimp_value_array_unref)> values (gimp_value_array_new (count), gimp_value_array_unref);
  for (guint i = 0; i < count; ++i)
    {
      JsonNode *arg = json_array_get_element (array, i);
      if (!JSON_NODE_HOLDS_OBJECT (arg)) fail ("Filter argument must be an object");
      const char *type = string (member (arg, "type"), "");
      JsonNode *value = member (arg, "value");
      GValue v = G_VALUE_INIT;
      if (i < 3 && (equal (procedure, "plug-in-edge") || equal (procedure, "plug-in-gauss"))) { g_value_init (&v, G_TYPE_INT); g_value_set_int (&v, 0); }
      else if (equal (type, "FLOAT")) { float f = number (value); if (!std::isfinite (f)) fail ("Filter float is out of range"); g_value_init (&v, G_TYPE_DOUBLE); g_value_set_double (&v, f); }
      else if (equal (type, "INT32")) { int n = integer (value); g_value_init (&v, G_TYPE_INT); g_value_set_int (&v, n); }
      else if (equal (type, "INT8")) { int n = integer (value); g_value_init (&v, G_TYPE_UINT); g_value_set_uint (&v, static_cast<guint> (n)); }
      else if (equal (type, "STRING")) { const char *s = string (value); g_value_init (&v, G_TYPE_STRING); g_value_set_string (&v, s); }
      /* Unknown/placeholder/RGB/INT8ARRAY were uninitialized GValues in the old
       * model. Null slots preserve that shape; raw JSON retains the value too.
       * Only the native exact Edge/Gauss executor may interpret typed slots. */
      gimp_value_array_append (values.get (), G_VALUE_TYPE (&v) ? &v : nullptr);
      if (G_VALUE_TYPE (&v)) g_value_unset (&v);
    }
  gchar *json = json_to_string (filter, FALSE);
  std::unique_ptr<GBytes, decltype (&g_bytes_unref)> raw (g_bytes_new_take (json, std::strlen (json)), g_bytes_unref);
  GError *error = nullptr;
  if (!gimp_filter_layer_set_definition (GIMP_FILTER_LAYER (layer), procedure, raw.get (), values.get (), &error))
    { std::string message = GimpPainter::take_error_message (error, "Unable to install filter definition"); throw std::runtime_error (message); }
}
struct Planner {
  GimpImage *image; GimpLayer *source;
  std::vector<Planned> plan;
  GimpContext *context = nullptr;
  GCancellable *cancellable = nullptr;
  bool applying = false;
  void layers (JsonNode *node, GimpLayer *parent, int index, unsigned depth = 0)
  {
    if (!node || !JSON_NODE_HOLDS_ARRAY (node)) fail ("Replacement layer must be an array");
    if (depth > 32) fail ("Layer preset hierarchy exceeds 32 levels");
    JsonArray *array = json_node_get_array (node);
    for (guint i = 0; i < json_array_get_length (array); ++i, ++index)
      {
        if (applying && cancellable && g_cancellable_is_cancelled (cancellable)) fail ("Layer preset application cancelled");
        if (plan.size () >= 4096) fail ("Layer preset exceeds 4096 nodes");
        JsonNode *object = json_array_get_element (array, i);
        if (!JSON_NODE_HOLDS_OBJECT (object)) fail ("Replacement layer must be an object");
        const char *target = string (member (object, "target"), "new");
        const char *type = string (member (object, "type"), "normal");
        const char *name = string (member (object, "name"));
        const char *content = string (member (object, "source"));
        GimpLayer *content_source = content && (equal (content, "source") || content[0] == '#') ? source : nullptr;
        Planned p; p.parent = parent; p.index = index; p.bounds = bounds (member (object, "boundary"), image, source);
        p.mode = mode (member (object, "mode"), source);
        JsonNode *opacity = member (object, "opacity");
        if (opacity && !JSON_NODE_HOLDS_NULL (opacity))
          p.opacity = json_node_get_value_type (opacity) == G_TYPE_STRING && equal (string (opacity), "source") ? gimp_layer_get_opacity (source) : number (opacity);
        if (equal (target, "source"))
          {
            p.original = true; p.layer = ObjectRef<GimpLayer>::retain (source);
          }
        else if (equal (target, "new"))
          {
            int w = p.bounds.w > 0 ? p.bounds.w : gimp_image_get_width (image);
            int h = p.bounds.h > 0 ? p.bounds.h : gimp_image_get_height (image);
            if (p.opacity < 0 || p.opacity > 1) p.opacity = 1;
            if (static_cast<int> (p.mode) < 0)
              p.mode = equal (type, "filter") ? GIMP_LAYER_MODE_PAINTER_REPLACE : GIMP_LAYER_MODE_PAINTER_NORMAL;
            GimpLayer *created = nullptr;
            if (equal (type, "normal"))
              created = content_source ? GIMP_LAYER (gimp_item_duplicate (GIMP_ITEM (content_source), GIMP_TYPE_LAYER)) :
                gimp_layer_new (image, w, h, gimp_image_get_layer_format (image, TRUE), name ? name : _("New Layer"), p.opacity, p.mode);
            else if (equal (type, "group"))
              { created = gimp_group_layer_new (image); gimp_object_set_name (GIMP_OBJECT (created), name ? name : _("New Group Layer")); }
            else if (equal (type, "clone"))
              created = gimp_clone_layer_new (image, content_source, w, h, name ? name : _("New Clone Layer"), p.opacity, p.mode);
            else if (equal (type, "filter"))
              created = gimp_filter_layer_new (image, w, h, name ? name : _("New Filter Layer"), p.opacity, p.mode);
            else fail ("Unknown layer preset layer type");
            if (!created) fail ("Unable to allocate layer preset layer");
            p.layer = ObjectRef<GimpLayer>::sink (created);
            if (equal (type, "filter")) install_filter (created, member (object, "filter"));
          }
        else fail ("Unknown layer preset target");
        const auto position = plan.size ();
        plan.push_back (std::move (p));
        GimpLayer *layer = plan[position].layer.get ();
        if (applying)
          {
            if (plan[position].original)
              {
                if (!gimp_image_reorder_item (image, GIMP_ITEM (layer), GIMP_ITEM (parent), index, TRUE, _("Apply layer preset")))
                  fail ("Unable to move preset source layer");
              }
            else if (!gimp_image_add_layer (image, layer, parent, index, TRUE))
              fail ("Unable to insert preset layer");
          }
        if (!plan[position].original && equal (type, "group"))
          {
            JsonNode *children = member (object, "children");
            if (children) layers (children, layer, 0, depth + 1);
          }
        if (applying)
          {
            const Planned& current = plan[position];
            if (static_cast<int> (current.mode) >= 0) gimp_layer_set_mode (layer, current.mode, TRUE);
            const Bounds& b = current.bounds;
            if (b.w && b.h)
              {
                int x = gimp_item_get_offset_x (GIMP_ITEM (layer)), y = gimp_item_get_offset_y (GIMP_ITEM (layer));
                if (b.w != gimp_item_get_width (GIMP_ITEM (layer)) || b.h != gimp_item_get_height (GIMP_ITEM (layer)))
                  gimp_item_resize (GIMP_ITEM (layer), context, GIMP_FILL_TRANSPARENT, b.w, b.h, offset_delta (x, b.x), offset_delta (y, b.y));
                x = gimp_item_get_offset_x (GIMP_ITEM (layer)); y = gimp_item_get_offset_y (GIMP_ITEM (layer));
                if (x != b.x || y != b.y) gimp_item_translate (GIMP_ITEM (layer), offset_delta (b.x, x), offset_delta (b.y, y), TRUE);
              }
            if (current.opacity >= 0 && current.opacity <= 1) gimp_layer_set_opacity (layer, current.opacity, TRUE);
          }
      }
  }
};
/* Record into isolated undo stacks until success. A failure pops only this
 * transaction, keeping the user's prior Undo AND Redo histories intact even
 * with a tiny Undo memory budget. No gimp_image_undo_free(image) is used. */
class Transaction {
  GimpImage *image; GimpImagePrivate *priv;
  GimpUndoStack *old_undo, *old_redo, *undo, *redo;
  int dirty, export_dirty, freeze;
  gint64 dirty_time;
  GList *selection;
  bool finished = false;
  void restore () noexcept
  {
    priv->undo_stack = old_undo; priv->redo_stack = old_redo;
    priv->group_count = 0; priv->pushing_undo_group = GIMP_UNDO_GROUP_NONE;
    priv->undo_freeze_count = freeze;
  }
public:
  explicit Transaction (GimpImage *i, const char *name) : image (i), priv (GIMP_IMAGE_GET_PRIVATE (i)),
    old_undo (priv->undo_stack), old_redo (priv->redo_stack),
    undo (gimp_undo_stack_new (i)), redo (gimp_undo_stack_new (i)),
    dirty (priv->dirty), export_dirty (priv->export_dirty), freeze (priv->undo_freeze_count), dirty_time (priv->dirty_time),
    selection (g_list_copy (gimp_image_get_selected_layers (i)))
  {
    priv->undo_stack = undo; priv->redo_stack = redo; priv->undo_freeze_count = 0;
    g_object_freeze_notify (G_OBJECT (image));
    gimp_image_undo_group_start (image, GIMP_UNDO_GROUP_MISC, name);
  }
  ~Transaction ()
  {
    if (!finished)
      {
        priv->group_count = 0; priv->pushing_undo_group = GIMP_UNDO_GROUP_NONE;
        gimp_image_undo (image);
        restore ();
        priv->dirty = dirty; priv->export_dirty = export_dirty; priv->dirty_time = dirty_time;
        gimp_image_set_selected_layers (image, selection);
      }
    gimp_undo_free (GIMP_UNDO (undo), GIMP_UNDO_MODE_UNDO);
    gimp_undo_free (GIMP_UNDO (redo), GIMP_UNDO_MODE_REDO);
    g_object_unref (undo); g_object_unref (redo); g_list_free (selection);
    g_object_thaw_notify (G_OBJECT (image));
  }
  void commit (const char *name)
  {
    auto *group = gimp_undo_stack_peek (undo);
    gimp_container_remove (undo->undos, GIMP_OBJECT (group));
    restore ();
    if (!freeze)
      {
        gimp_image_undo_group_start (image, GIMP_UNDO_GROUP_MISC, name);
        /* Publish the staged group directly. Nesting it inside an empty group
         * would prevent native Undo from reversing its children for Redo. */
        GimpUndo *empty = gimp_undo_stack_peek (old_undo);
        gimp_container_remove (old_undo->undos, GIMP_OBJECT (empty));
        gimp_undo_free (empty, GIMP_UNDO_MODE_UNDO); g_object_unref (empty);
        --priv->dirty; --priv->export_dirty;
        gimp_undo_stack_push_undo (old_undo, group);
        gimp_image_undo_group_end (image);
      }
    else { gimp_undo_free (group, GIMP_UNDO_MODE_UNDO); g_object_unref (group); }
    finished = true;
  }
};
}
gboolean gimp_layer_preset_matches (GimpLayerPreset *preset, GimpLayer *layer)
{
  return boundary<gboolean> (nullptr, FALSE, [&] {
    if (!GIMP_IS_LAYER_PRESET (preset)) return FALSE;
    return matches (member (preset->root, "source-layer"), layer) ? TRUE : FALSE;
  });
}
gboolean gimp_layer_preset_apply_full (GimpLayerPreset *preset, GimpContext *context, GimpLayer *source, GCancellable *cancellable, GError **error)
{
  return boundary<gboolean> (error, FALSE, [&] {
    if (!GIMP_IS_LAYER_PRESET (preset) || !GIMP_IS_CONTEXT (context) || !GIMP_IS_LAYER (source)) fail ("Layer preset requires one selected source layer");
    auto preset_lease = ObjectRef<GObject>::retain (G_OBJECT (preset));
    auto context_lease = ObjectRef<GObject>::retain (G_OBJECT (context));
    auto source_lease = ObjectRef<GimpLayer>::retain (source);
    const char *preset_name = gimp_object_get_name (preset);
    const std::string name (preset_name ? preset_name : _("Apply layer preset"));
    GimpImage *image = gimp_item_get_image (GIMP_ITEM (source));
    if (!image || !gimp_item_is_attached (GIMP_ITEM (source)) || gimp_context_get_image (context) != image)
      fail ("Layer preset source must belong to the current image");
    auto image_lease = ObjectRef<GObject>::retain (G_OBJECT (image));
    if (gimp_image_get_undo_group_count (image) || gimp_image_has_pending_paint (image) || gimp_image_get_floating_selection (image)) fail ("Finish the active image transaction before applying a layer preset");
    Node root (gimp_layer_preset_dup_json (preset), json_node_unref);
    if (integer (member (root.get (), "version")) != 1) fail ("Unsupported layer preset version");
    if (!matches (member (root.get (), "source-layer"), source)) fail ("Selected layer does not match this layer preset");
    GimpLayer *parent = GIMP_LAYER (gimp_item_get_parent (GIMP_ITEM (source)));
    auto parent_lease = ObjectRef<GimpLayer>::retain (parent);
    GimpContainer *container = parent ? gimp_viewable_get_children (GIMP_VIEWABLE (parent)) : gimp_image_get_layers (image);
    int index = gimp_container_get_child_index (container, GIMP_OBJECT (source));
    /* Validate the entire program before touching the image, then evaluate
     * it again in legacy depth-first order against the live source. Repeated
     * source nodes are legal ordered edits: later source-valued fields and
     * duplicates must observe earlier edits, not an arbitrary initial snapshot. */
    Planner validation {image, source, {}, nullptr, nullptr, false};
    validation.layers (member (root.get (), "replacement-layer"), parent, index);
    const bool empty = validation.plan.empty ();
    validation.plan.clear ();
    /* Construction and probe cleanup may call native observers. Keep the old
     * parent alive and reject a changed insertion target before starting Undo. */
    if (!gimp_item_is_attached (GIMP_ITEM (source)) ||
        gimp_item_get_image (GIMP_ITEM (source)) != image ||
        gimp_context_get_image (context) != image ||
        gimp_item_get_parent (GIMP_ITEM (source)) != GIMP_ITEM (parent) ||
        index < 0 || gimp_container_get_child_index (container, GIMP_OBJECT (source)) != index)
      fail ("Layer preset source changed during preflight");
    if (gimp_image_get_undo_group_count (image) || gimp_image_has_pending_paint (image) || gimp_image_get_floating_selection (image)) fail ("Finish the active image transaction before applying a layer preset");
    if (empty) return TRUE;
    Transaction transaction (image, name.c_str ());
    Planner application {image, source, {}, context, cancellable, true};
    application.layers (member (root.get (), "replacement-layer"), parent, index);
    if (cancellable && g_cancellable_is_cancelled (cancellable)) fail ("Layer preset application cancelled");
    transaction.commit (name.c_str ());
    gimp_image_flush (image);
    return TRUE;
  });
}

gboolean gimp_layer_preset_apply (GimpLayerPreset *preset, GimpContext *context, GimpLayer *source, GError **error)
{ return gimp_layer_preset_apply_full (preset, context, source, nullptr, error); }
