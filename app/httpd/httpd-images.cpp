/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <algorithm>
#include <stdexcept>
#include "httpd-resource.hpp"
extern "C"
{
#include "libgimpbase/gimpbase.h"
#include "core/gimp.h"
#include "core/gimp-gui.h"
#include "core/gimpcontainer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-new.h"
#include "core/gimpitem.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpdrawable.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpclonelayer.h"
#include "core/gimpfilterlayer.h"
}
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
namespace Http
{
static std::vector<GimpObject *>
children (GimpContainer *c)
{
  std::vector<GimpObject *> r;
  if (c)
    gimp_container_foreach (
        c,
        [] (gpointer p, gpointer d) {
          static_cast<std::vector<GimpObject *> *> (d)->push_back (GIMP_OBJECT (p));
        },
        &r);
  return r;
}
static GimpContainer *
child_container (Gimp *g, GimpObject *o)
{
  if (! o)
    return g->images;
  if (GIMP_IS_IMAGE (o))
    return gimp_image_get_layers (GIMP_IMAGE (o));
  return gimp_viewable_get_children (GIMP_VIEWABLE (o));
}
static gint
object_id (GimpObject *o)
{
  return GIMP_IS_IMAGE (o) ? gimp_image_get_id (GIMP_IMAGE (o)) :
                             gimp_item_get_id (GIMP_ITEM (o));
}
static const char *
object_name (GimpObject *o)
{
  return GIMP_IS_IMAGE (o) ? gimp_image_get_display_name (GIMP_IMAGE (o)) :
                             gimp_object_get_name (o);
}
static Json
bounds (int x, int y, int w, int h)
{
  auto n = array ();
  for (gint64 v : { gint64 (x), gint64 (y), gint64 (x) + w, gint64 (y) + h })
    append (n.get (), number (v));
  return n;
}
static void
dimensions (gint64 w, gint64 h)
{
  if (w < 1 || h < 1 || w > 32768 || h > 32768 || w * h > 64 * 1024 * 1024)
    throw Failure (413,
                   "Image dimensions exceed the 64-megapixel request limit");
}
static int
enum_value (GType t, JsonNode *n, int fallback)
{
  if (! n)
    return fallback;
  auto  s = text (n);
  auto *c = static_cast<GEnumClass *> (g_type_class_ref (t));
  auto *v = g_enum_get_value_by_nick (c, s.c_str ());
  if (! v)
    v = g_enum_get_value_by_name (c, s.c_str ());
  int out = v ? v->value : fallback;
  g_type_class_unref (c);
  if (! v)
    throw Failure (400, "Unknown enum name: " + s);
  return out;
}
class Images final : public Resource
{
  Json
  info (Gimp *g, GimpObject *o)
  {
    auto n = object ();
    if (! o)
      {
        for (auto *i : children (g->images))
          set (n.get (), std::to_string (object_id (i)).c_str (),
               string (object_name (i)));
        return n;
      }
    set (n.get (), "name", string (object_name (o)));
    set (n.get (), "id", number (object_id (o)));
    if (GIMP_IS_IMAGE (o))
      {
        auto *i = GIMP_IMAGE (o);
        set (n.get (), "type", string ("image"));
        set (n.get (), "boundary",
             bounds (0, 0, gimp_image_get_width (i), gimp_image_get_height (i)));
        auto ch = object ();
        for (auto *c : children (gimp_image_get_layers (i)))
          set (ch.get (), std::to_string (object_id (c)).c_str (),
               string (object_name (c)));
        set (n.get (), "children", std::move (ch));
      }
    else
      {
        auto       *i    = GIMP_ITEM (o);
        auto       *l    = GIMP_LAYER (o);
        const char *type = GIMP_IS_FILTER_LAYER (o) ? "filter" :
                           GIMP_IS_CLONE_LAYER (o)  ? "clone" :
                           GIMP_IS_GROUP_LAYER (o)  ? "group" :
                                                      "normal";
        set (n.get (), "type", string (type));
        set (n.get (), "boundary",
             bounds (gimp_item_get_offset_x (i), gimp_item_get_offset_y (i),
                     gimp_item_get_width (i), gimp_item_get_height (i)));
        set (n.get (), "mode", layer_mode_name (gimp_layer_get_mode (l)));
        set (n.get (), "opacity", real (gimp_layer_get_opacity (l)));
        set (n.get (), "visible", boolean (gimp_item_get_visible (i)));
        set (n.get (), "alpha", boolean (gimp_drawable_has_alpha (GIMP_DRAWABLE (o))));
        if (auto *container = child_container (g, o))
          {
            auto names = array (), ids = object ();
            for (auto *child : children (container))
              {
                append (names.get (), string (object_name (child)));
                set (ids.get (), std::to_string (object_id (child)).c_str (),
                     string (object_name (child)));
              }
            set (n.get (), "children", std::move (names));
            set (n.get (), "child-ids", std::move (ids));
          }
        if (GIMP_IS_CLONE_LAYER (o))
          {
            auto *source = gimp_clone_layer_get_source (GIMP_CLONE_LAYER (o));
            set (n.get (), "source",
                 source ? number (gimp_item_get_id (GIMP_ITEM (source))) : null ());
            auto *name = gimp_clone_layer_dup_source_name (GIMP_CLONE_LAYER (o));
            set (n.get (), "source-name", string (name));
            g_free (name);
          }
        if (GIMP_IS_FILTER_LAYER (o))
          {
            auto *p = gimp_filter_layer_dup_procedure (GIMP_FILTER_LAYER (o));
            set (n.get (), "procedure", string (p));
            g_free (p);
          }
      }
    return n;
  }
  Response
  pixels (Gimp *g, GimpObject *o, bool preview)
  {
    if (! o)
      throw Failure (404, "Image or layer required");
    int w = GIMP_IS_IMAGE (o) ? gimp_image_get_width (GIMP_IMAGE (o)) :
                                gimp_item_get_width (GIMP_ITEM (o));
    int h = GIMP_IS_IMAGE (o) ? gimp_image_get_height (GIMP_IMAGE (o)) :
                                gimp_item_get_height (GIMP_ITEM (o));
    if (preview)
      {
        double scale = std::min (1., 128. / std::max (w, h));
        w            = std::max (1, int (w * scale));
        h            = std::max (1, int (h * scale));
      }
    dimensions (w, h);
    auto *p = gimp_viewable_get_pixbuf (GIMP_VIEWABLE (o),
                                        gimp_get_user_context (g), w, h, nullptr);
    if (! p)
      throw Failure (403, "Cannot render this item");
    gchar  *bytes = nullptr;
    gsize   size  = 0;
    GError *e     = nullptr;
    bool    ok    = gdk_pixbuf_save_to_buffer (p, &bytes, &size,
                                         preview ? "jpeg" : "png", &e, nullptr);
    if (! ok)
      {
        std::string m = e ? e->message : "Cannot encode pixels";
        g_clear_error (&e);
        throw Failure (500, m);
      }
    Response out;
    out.content_type = preview ? "image/jpeg" : "image/png";
    out.body.assign (bytes, size);
    g_free (bytes);
    return out;
  }
  Response
  create (Gimp *g, GimpObject *o, const Request &r, bool data)
  {
    GimpImage         *image = o ? (GIMP_IS_IMAGE (o) ? GIMP_IMAGE (o) :
                                                        gimp_item_get_image (GIMP_ITEM (o))) :
                                   nullptr;
    GimpLayer         *layer = nullptr;
    ObjectRef<GObject> image_guard, layer_guard;
    if (data)
      {
        if (r.content_type.compare (0, 6, "image/") != 0)
          throw Failure (415, "Expected image media type");
        GError *e = nullptr;
        auto *loader = gdk_pixbuf_loader_new_with_mime_type (r.content_type.c_str (), &e);
        if (! loader)
          {
            g_clear_error (&e);
            throw Failure (415, "Unsupported image media type");
          }
        bool huge = false;
        g_signal_connect (loader, "size-prepared",
                          G_CALLBACK (+[] (GdkPixbufLoader *l, gint w, gint h, gpointer p) {
                            if (w < 1 || h < 1 || w > 32768 || h > 32768 ||
                                gint64 (w) * h > 64 * 1024 * 1024)
                              {
                                *static_cast<bool *> (p) = true;
                                gdk_pixbuf_loader_set_size (l, 1, 1);
                              }
                          }),
                          &huge);
        bool ok = gdk_pixbuf_loader_write (
            loader, reinterpret_cast<const guint8 *> (r.body.data ()), r.body.size (), &e);
        if (ok)
          ok = gdk_pixbuf_loader_close (loader, &e);
        else
          gdk_pixbuf_loader_close (loader, nullptr);
        auto *pix = ok ? gdk_pixbuf_loader_get_pixbuf (loader) : nullptr;
        ObjectRef<GObject> p = pix ? ObjectRef<GObject>::retain (G_OBJECT (pix)) :
                                     ObjectRef<GObject> ();
        g_object_unref (loader);
        g_clear_error (&e);
        if (huge)
          throw Failure (413, "Image dimensions exceed limit");
        if (! p)
          throw Failure (400, "Invalid encoded image");
        if (! image)
          {
            image = gimp_image_new_from_pixbuf (g, GDK_PIXBUF (p.get ()), "Imported image");
            if (! image)
              throw Failure (500, "Cannot create image");
            image_guard = ObjectRef<GObject>::adopt (G_OBJECT (image));
          }
        else
          layer = gimp_layer_new_from_pixbuf (GDK_PIXBUF (p.get ()), image,
                                              gimp_image_get_layer_format (image, TRUE),
                                              "Imported layer", 1.,
                                              GIMP_LAYER_MODE_PAINTER_NORMAL);
      }
    else
      {
        auto  n = parse (r.body);
        auto *b = member (n.get (), "boundary");
        if (! JSON_NODE_HOLDS_ARRAY (b) ||
            json_array_get_length (json_node_get_array (b)) != 4)
          throw Failure (400, "boundary needs four integers");
        gint64 v[4];
        for (int j = 0; j < 4; ++j)
          {
            v[j] = integer (json_array_get_element (json_node_get_array (b), j));
            if (v[j] < G_MININT || v[j] > G_MAXINT)
              throw Failure (400, "Boundary out of range");
          }
        auto w = v[2] - v[0], h = v[3] - v[1];
        dimensions (w, h);
        if (! image)
          {
            auto type = static_cast<GimpImageBaseType> (enum_value (
                GIMP_TYPE_IMAGE_BASE_TYPE, member (n.get (), "color-mode", false), GIMP_RGB));
            image = gimp_image_new (g, w, h, type, GIMP_PRECISION_U8_NON_LINEAR);
            if (! image)
              throw Failure (500, "Cannot create image");
            image_guard = ObjectRef<GObject>::adopt (G_OBJECT (image));
          }
        else
          {
            auto   name    = text (member (n.get (), "name"));
            auto   type    = text (member (n.get (), "type"));
            double opacity = member (n.get (), "opacity", false) ?
                                 numeric (member (n.get (), "opacity")) :
                                 1.;
            if (opacity < 0 || opacity > 1)
              throw Failure (400, "Opacity outside 0..1");
            bool alpha   = member (n.get (), "alpha", false) ?
                               truth (member (n.get (), "alpha")) :
                               true;
            bool visible = member (n.get (), "visible", false) ?
                               truth (member (n.get (), "visible")) :
                               true;
            auto mode    = static_cast<GimpLayerMode> (
                layer_mode (member (n.get (), "mode", false)));
            if (type == "normal")
              layer = gimp_layer_new (image, w, h,
                                      gimp_image_get_layer_format (image, alpha),
                                      name.c_str (), opacity, mode);
            else if (type == "group")
              {
                layer = gimp_group_layer_new (image);
                gimp_object_set_name (GIMP_OBJECT (layer), name.c_str ());
                gimp_layer_set_opacity (layer, opacity, FALSE);
                gimp_layer_set_mode (layer, mode, FALSE);
              }
            else if (type == "filter")
              layer = gimp_filter_layer_new (image, w, h, name.c_str (), opacity, mode);
            else if (type == "clone")
              {
                GimpLayer *source = nullptr;
                if (auto *s = member (n.get (), "source", false))
                  {
                    auto id = integer (s);
                    auto *i = id > 0 && id <= G_MAXINT ? gimp_item_get_by_id (g, id) : nullptr;
                    if (! i || ! GIMP_IS_LAYER (i) || gimp_item_get_image (i) != image)
                      throw Failure (400, "Invalid clone source");
                    source = GIMP_LAYER (i);
                  }
                layer = gimp_clone_layer_new (image, source, w, h,
                                              name.c_str (), opacity, mode);
              }
            else
              throw Failure (400, "Unknown layer type");
            if (layer)
              {
                gimp_item_set_offset (GIMP_ITEM (layer), v[0], v[1]);
                gimp_item_set_visible (GIMP_ITEM (layer), visible, FALSE);
              }
          }
      }
    if (layer)
      {
        layer_guard = ObjectRef<GObject>::sink (G_OBJECT (layer));
        auto *parent = o && GIMP_IS_LAYER (o) ? gimp_layer_get_parent (GIMP_LAYER (o)) : nullptr;
        int index = o && GIMP_IS_LAYER (o) ? gimp_item_get_index (GIMP_ITEM (o)) : 0;
        if (! gimp_image_add_layer (image, layer, parent, index, TRUE))
          throw Failure (500, "Layer insertion failed");
        gimp_image_flush (image);
      }
    else if (o)
      throw Failure (500, "Layer creation failed");
    if (image_guard)
      {
        // Match native PDB ownership: a first display takes the initial reference;
        // headless creation transfers it to the image-delete/display-new protocol.
        // A service registry must not also own that consumable creation reference.
        if (! gimp_create_display (g, image, gimp_unit_pixel (), 1., nullptr))
          image_guard.release ();
      }
    auto n = object ();
    set (n.get (), "result", boolean (true));
    set (n.get (), "image", number (gimp_image_get_id (image)));
    if (! layer)
      {
        auto *selected = gimp_image_get_selected_layers (image);
        if (selected)
          layer = GIMP_LAYER (selected->data);
      }
    if (layer)
      {
        set (n.get (), "layer", number (gimp_item_get_id (GIMP_ITEM (layer))));
        set (n.get (), "drawable", number (gimp_item_get_id (GIMP_ITEM (layer))));
      }
    return reply (std::move (n), 201);
  }

public:
  Response
  handle (Gimp *g, const Request &r) override
  {
    if (r.method != "GET" && r.method != "PUT")
      throw Failure (405, "Images support GET and PUT");
    GimpObject *o         = nullptr;
    std::string operation = "info";
    auto parts = segments (r.path.substr (std::string ("/api/v1/images").size ()));
    for (size_t j = 0; j < parts.size (); ++j)
      {
        const auto &part = parts[j];
        if (part[0] == '#')
          {
            if (j + 1 != parts.size ())
              throw Failure (400, "Operation must be the final path component");
            operation = part.substr (1);
            break;
          }
        GimpObject *found        = nullptr;
        char       *end          = nullptr;
        auto        id           = g_ascii_strtoll (part.c_str (), &end, 10);
        bool        numeric_name = end != part.c_str () && *end == '\0';
        for (auto *child : children (child_container (g, o)))
          if (numeric_name ? object_id (child) == id : part == object_name (child))
            found = child;
        if (! found)
          throw Failure (404, "Image tree item not found: " + part);
        o = found;
      }
    // A viewable renderer or insertion observer may synchronously release the
    // last external owner. Keep both the item and its weakly-linked image alive.
    auto  target_lease = ObjectRef<GObject>::retain (G_OBJECT (o));
    auto *owning_image = o ? (GIMP_IS_IMAGE (o) ? GIMP_IMAGE (o) :
                                                  gimp_item_get_image (GIMP_ITEM (o))) :
                             nullptr;
    auto  image_lease  = ObjectRef<GObject>::retain (G_OBJECT (owning_image));
    if (operation != "info" && operation != "data" && operation != "preview")
      throw Failure (404, "Unknown image operation");
    if (r.method == "GET")
      return operation == "info" ? reply (info (g, o)) :
                                   pixels (g, o, operation == "preview");
    if (operation == "preview")
      throw Failure (405, "Preview is read-only");
    return create (g, o, r, operation == "data");
  }
};
std::unique_ptr<Resource>
images_resource ()
{
  return std::unique_ptr<Resource> (new Images);
}
}
}
