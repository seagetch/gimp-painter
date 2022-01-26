/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpCloneLayer
 * Copyright (C) 2017 seagetch <sigetch@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "base/delegators.hpp"
#include "base/glib-cxx-types.hpp"
__DECLARE_GTK_CLASS__(GObject, G_TYPE_OBJECT);
#include "base/glib-cxx-impl.hpp"
#include "base/glib-cxx-utils.hpp"

extern "C" {
#include "config.h"

#include <string.h>

#include <gegl.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"

#include "core-types.h"
#include "core-enums.h"

#include "base/tile.h"
#include "base/tile-manager.h"
#include "base/tile-manager-private.h"
#include "base/pixel-region.h"

#include "gimp.h"
#include "pdb/gimppdb.h"

#include "gimpimage.h"
#include "gimpimage-undo.h"
#include "gimpitem.h"
#include "gimpdrawable-private.h" /* eek */
#include "gimpdrawablestack.h"
#include "gimppickable.h"
#include "gimpprojectable.h"
#include "gimpprojection.h"
#include "gimpcontext.h"
#include "gimpprogress.h"
#include "gimpcontainer.h"
#include "gimpgrouplayer.h"

#include "gimp-intl.h"
#include "gimplayer.h"
#include "gimplayermask.h"

#include "gegl/gimp-gegl-utils.h"
#include "paint-funcs/paint-funcs.h"

#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "core/gimpparamspecs.h"
#include "plug-in/gimptemporaryprocedure.h"
}

#include "gimpclonelayer.h"
#include "gimpclonelayerundo.h"


namespace GLib {

struct CloneLayer : virtual public ImplBase, virtual public CloneLayerInterface
{
  GimpLayer*       source_layer;

  /*  hackish temp states to make the projection/tiles stuff work  */
  gint             reallocate_width;
  gint             reallocate_height;
  gint             prev_w, prev_h;
  gint             prev_x, prev_y;

  CXXPointer<Delegators::Connection> parent_changed_conn;
  CXXPointer<Delegators::Connection> update_conn;
  CXXPointer<Delegators::Connection> freeze_conn;
  CString          source_name;

  static void class_init(Traits<GimpCloneLayer>::Class* klass);
  template<typename IFaceClass> static void iface_init(IFaceClass* klass);

  CloneLayer(GObject* o);
  virtual ~CloneLayer();

  // Inherited methods
  virtual void            constructed  ();

  virtual gint64          get_memsize  (gint64*          gui_size);

  virtual gboolean        get_size     (gint*            width,
                                        gint*            height);

  virtual GimpItem      * duplicate    (GType            new_type);
  virtual void            convert      (GimpImage*       dest_image);
  virtual void            translate    (gint             offset_x,
                                        gint             offset_y,
                                        gboolean         push_undo);
  virtual void            scale        (gint             new_width,
                                        gint             new_height,
                                        gint             new_offset_x,
                                        gint             new_offset_y,
                                        GimpInterpolationType  interp_type,
                                        GimpProgress*    progress);
  virtual void            resize       (GimpContext*     context,
                                        gint             new_width,
                                        gint             new_height,
                                        gint             offset_x,
                                        gint             offset_y);
  virtual void            flip         (GimpContext*     context,
                                        GimpOrientationType flip_type,
                                        gdouble          axis,
                                        gboolean         clip_result);
  virtual void            rotate       (GimpContext*     context,
                                        GimpRotationType rotate_type,
                                        gdouble          center_x,
                                        gdouble          center_y,
                                        gboolean         clip_result);
  virtual void            transform    (GimpContext*     context,
                                        const GimpMatrix3* matrix,
                                        GimpTransformDirection direction,
                                        GimpInterpolationType  interpolation_type,
                                        gint             recursion_level,
                                        GimpTransformResize clip_result,
                                        GimpProgress*    progress);

  virtual gint64      estimate_memsize (gint             width,
                                        gint             height);

  virtual gint         get_opacity_at  (gint             x,
                                        gint             y);

  virtual void            update       ();
  virtual void            update_size  (gint w, gint h);

  virtual void            set_source   (GimpLayer* source);
  virtual GimpLayer*      get_source   ();
  virtual void            set_source_by_name (const gchar* name);

  virtual gboolean        is_editable  ();

  // Event handlers
  virtual void       on_parent_changed (GimpViewable* viewable,
                                        GimpViewable* parent);

  virtual void        on_source_update (GimpDrawable* drawable,
                                        gint          x,
                                        gint          y,
                                        gint          width,
                                        gint          height);
  virtual void        on_source_frozen (GObject*      object,
                                        GParamSpec*   pspec);
private:
  void                invalidate_layer ();
};


extern const char gimp_clone_layer_name[] = "GimpCloneLayer";
using Class = NewGClass<gimp_clone_layer_name,
                     UseCStructs<GimpLayer, GimpCloneLayer>,
                     CloneLayer, GimpPickable>;

static Class class_instance;

static GimpUndo *
gimp_image_undo_push_clone_layer_convert (GimpImage      *image,
                                          const gchar    *undo_desc,
                                          GimpCloneLayer *group)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (Class::Traits::is_instance(group), NULL);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (group)), NULL);

  return gimp_image_undo_push (image, Traits<GimpCloneLayerUndo>::get_type(),
                               GIMP_UNDO_GROUP_LAYER_CONVERT, undo_desc,
                               GimpDirtyMask(GIMP_DIRTY_ITEM | GIMP_DIRTY_DRAWABLE),
                               "item", group,
                               NULL);
}


#define bind_to_class(klass, method, impl)  Class::__(&klass->method).bind<&impl::method>()
#define _override(method)  Class::__(&klass->method).bind<&Impl::method>()

void CloneLayer::class_init(Traits<GimpCloneLayer>::Class *klass)
{
  typedef CloneLayer Impl;
  g_print("CloneLayer::class_init\n");
  class_instance.with_class(klass)->
      as_class<GObject>([](GObjectClass* klass){
        _override (constructed);

      })->
      as_class<GimpObject>([](GimpObjectClass* klass) {
        _override (get_memsize);

      })->
      as_class<GimpViewable>([](GimpViewableClass* klass){
        klass->default_stock_id = "gtk-duplicate";
        _override (get_size);

      })->
      as_class<GimpItem>([](GimpItemClass* klass) {
        _override (duplicate);
        _override (translate);
        _override (scale);
        _override (resize);
        _override (flip);
        _override (rotate);
        _override (transform);
      //_override (convert);
        _override (is_editable);

        klass->default_name         = _("Clone Layer");
        klass->rename_desc          = C_("undo-type", "Rename Clone Layer");
        klass->translate_desc       = C_("undo-type", "Move Clone Layer");
        klass->scale_desc           = C_("undo-type", "Scale Clone Layer");
        klass->resize_desc          = C_("undo-type", "Resize Clone Layer");
        klass->flip_desc            = C_("undo-type", "Flip Clone Layer");
        klass->rotate_desc          = C_("undo-type", "Rotate Clone Layer");
        klass->transform_desc       = C_("undo-type", "Transform Clone Layer");

      })->
      as_class<GimpDrawable>([](GimpDrawableClass* klass) {
        _override (estimate_memsize);
      });
}

template<>
void CloneLayer::iface_init<GimpPickableInterface>(GimpPickableInterface* klass)
{
  typedef CloneLayer Impl;
  g_print("CloneLayer::iface_init<GimpPickableInterface>\n");
  
  _override (get_opacity_at);
}

}; // namespace

GLib::CloneLayer::~CloneLayer()
{
}

GLib::CloneLayer::CloneLayer(GObject* o) : ImplBase(o)
{
//  parent_changed_conn = g_signal_connect_delegator (G_OBJECT(g_object), "parent-changed",
//                                                    Delegators::delegator(this, &GLib::CloneLayer::on_parent_changed));
  update_conn         = NULL;
  freeze_conn         = NULL;
  prev_x              = 0;
  prev_y              = 0;
  prev_w              = 0;
  prev_h              = 0;
}

void GLib::CloneLayer::constructed ()
{
  on_parent_changed(GIMP_VIEWABLE(g_object), NULL);
}


void GLib::CloneLayer::set_source(GimpLayer* layer)
{
  if (source_layer) {
    update_conn = NULL;
    source_layer = NULL;
  }
  if (layer) {
    update_conn = g_signal_connect_delegator (G_OBJECT(layer), "update",
        Delegators::delegator(this, &GLib::CloneLayer::on_source_update));
    freeze_conn = g_signal_connect_delegator (G_OBJECT(layer), "notify::frozen", 
        Delegators::delegator(this, &GLib::CloneLayer::on_source_frozen));
    source_layer = layer;
    auto src  = ref(layer);
    gint w, h;
    w = src [gimp_item_get_width] ();
    h = src [gimp_item_get_height] ();
    prev_x = src [gimp_item_get_offset_x] ();
    prev_y = src [gimp_item_get_offset_y] ();
    prev_w = w;
    prev_h = h;

    gimp_drawable_update(GIMP_DRAWABLE(layer), 0, 0, w, h);
  }
}

GimpLayer*
GLib::CloneLayer::get_source()
{
  if (source_name) {
    std::function<GimpLayer*(GimpContainer*)> finder = [&](GimpContainer* _container)->GimpLayer* {
      auto container = ref(_container);
      GimpLayer* result = NULL;
      gint n_children = container [gimp_container_get_n_children] ();
      for (int i = 0; i < n_children; i ++) {
        auto layer = ref(container[gimp_container_get_child_by_index](i));
        if (strcmp(layer [gimp_object_get_name](), source_name) == 0) {
          result = layer;
          return result;
        }
        GimpContainer* children = layer [gimp_viewable_get_children] ();
        if (children && children != _container) {
          result = finder (children);
          if (result)
            return result;
        }
      }
      return NULL;
    };
    auto image = ref(g_object) [gimp_item_get_image] ();
    GimpLayer* src = finder(ref(image) [gimp_image_get_layers] ());

    if (src) {
      source_name = NULL;
      set_source(src);
    }
  }
  return source_layer;
}

void GLib::CloneLayer::set_source_by_name(const gchar* name)
{
  g_print("CloneLayer::set_source_by_name(%s)\n", name);
  source_name = g_strdup(name);
}

gint64 GLib::CloneLayer::get_memsize (gint64     *gui_size)
{
  gint64                 memsize = 0;

  return memsize + GIMP_OBJECT_CLASS (Class::parent_class)->get_memsize (GIMP_OBJECT(g_object),
                                                                  gui_size);
}

gboolean GLib::CloneLayer::get_size (gint         *width,
                                     gint         *height)
{
  if (reallocate_width  != 0 &&
      reallocate_height != 0) {
    nullable(width)  = reallocate_width;
    nullable(height) = reallocate_height;
    return TRUE;
  }

  gboolean result = GIMP_VIEWABLE_CLASS (Class::parent_class)->get_size (GIMP_VIEWABLE(g_object), width, height);
  return result;
}

GimpItem* GLib::CloneLayer::duplicate (GType     new_type)
{
  GimpItem *new_item;

  g_return_val_if_fail (g_type_is_a (new_type, GIMP_TYPE_DRAWABLE), NULL);

  new_item = GIMP_ITEM_CLASS (Class::parent_class)->duplicate (GIMP_ITEM(g_object), new_type);
  if (CloneLayerInterface::is_instance(new_item)) {
    dynamic_cast<CloneLayer*>(CloneLayerInterface::cast(new_item))->set_source(get_source());
  }
  return new_item;
}

void GLib::CloneLayer::convert (GimpImage *dest_image)
{
  GIMP_ITEM_CLASS (Class::parent_class)->convert (GIMP_ITEM(g_object), dest_image);
}

void GLib::CloneLayer::translate (gint      offset_x,
                                  gint      offset_y,
                                  gboolean  push_undo)
{
  GIMP_ITEM_CLASS(Class::parent_class)->translate(GIMP_ITEM(g_object), offset_x, offset_y, push_undo);
//  invalidate_layer();
}

void GLib::CloneLayer::scale (gint                   new_width,
                                 gint                   new_height,
                                 gint                   new_offset_x,
                                 gint                   new_offset_y,
                                 GimpInterpolationType  interpolation_type,
                                 GimpProgress          *progress)
{
  GIMP_ITEM_CLASS(Class::parent_class)->scale(GIMP_ITEM(g_object), new_width, new_height, new_offset_x, new_offset_y, interpolation_type, progress);
//  invalidate_layer();
}

void GLib::CloneLayer::resize (GimpContext *context,
                               gint         new_width,
                               gint         new_height,
                               gint         offset_x,
                               gint         offset_y)
{
  GIMP_ITEM_CLASS(Class::parent_class)->resize(GIMP_ITEM(g_object), context, new_width, new_height, offset_x, offset_y);
//  invalidate_layer();
}

void GLib::CloneLayer::flip (GimpContext         *context,
                             GimpOrientationType  flip_type,
                             gdouble              axis,
                             gboolean             clip_result)
{
  GIMP_ITEM_CLASS(Class::parent_class)->flip(GIMP_ITEM(g_object), context, flip_type, axis, clip_result);
//  invalidate_layer();
}

void GLib::CloneLayer::rotate (GimpContext      *context,
                                  GimpRotationType  rotate_type,
                                  gdouble           center_x,
                                  gdouble           center_y,
                                  gboolean          clip_result)
{
  GIMP_ITEM_CLASS(Class::parent_class)->rotate(GIMP_ITEM(g_object), context, rotate_type, center_x, center_y, clip_result);
//  invalidate_layer();
}

void GLib::CloneLayer::transform (GimpContext            *context,
                                  const GimpMatrix3      *matrix,
                                  GimpTransformDirection  direction,
                                  GimpInterpolationType   interpolation_type,
                                  gint                    recursion_level,
                                  GimpTransformResize     clip_result,
                                  GimpProgress           *progress)
{
  GIMP_ITEM_CLASS(Class::parent_class)->transform(GIMP_ITEM(g_object), context, matrix, direction, interpolation_type, recursion_level, clip_result, progress);
//  invalidate_layer();
}

gint64 GLib::CloneLayer::estimate_memsize (gint                width,
                                           gint                height)
{
  return GIMP_DRAWABLE_CLASS (Class::parent_class)->estimate_memsize (GIMP_DRAWABLE(g_object),
                                                                      width,
                                                                      height);
}

gint GLib::CloneLayer::get_opacity_at (gint          x,
                                       gint          y)
{
  /* Only consider child layers as having content */
  return 0;
}


/*  priv functions  */

void GLib::CloneLayer::update () {
}

void GLib::CloneLayer::update_size (gint width, gint height) {
  if (!get_source())
    return;

  resize(NULL, width, height, 0, 0);
}

gboolean GLib::CloneLayer::is_editable()
{
  return FALSE;
}

void GLib::CloneLayer::invalidate_layer ()
{
  auto self         = ref(g_object);
  auto image        = ref( self [gimp_item_get_image] () );
  gint width        = self [gimp_item_get_width] ();
  gint height       = self [gimp_item_get_height] ();
  gint offset_x     = self [gimp_item_get_offset_x] ();
  gint offset_y     = self [gimp_item_get_offset_y] ();

  GimpViewable*  parent = gimp_viewable_get_parent(GIMP_VIEWABLE(g_object));
  if (parent) {
    auto proj = ref( GIMP_PROJECTABLE(parent) );
    proj [gimp_projectable_invalidate] (offset_x, offset_y, width, height);
    proj [gimp_projectable_flush] (TRUE);
  }

  image [gimp_image_invalidate] (offset_x, offset_y, width, height, 0);
  auto projection = ref(image [gimp_image_get_projection] ());
  projection [gimp_projection_flush_now] ();
  image [gimp_image_flush] ();
}


void GLib::CloneLayer::on_parent_changed (GimpViewable  *viewable,
                                             GimpViewable  *parent)
{
//  g_print("on_parent_changed: %p ==> parent=%s\n", viewable, parent? gimp_object_get_name(parent): NULL);
  // Check for loop
}

void GLib::CloneLayer::on_source_update (GimpDrawable* _source,
                                         gint            x,
                                         gint            y,
                                         gint            width,
                                         gint            height)
{
  g_print ("%s (%s) %d, %d (%d, %d)\n",
           G_STRFUNC, gimp_object_get_name (g_object),
           x, y, width, height);

  auto self   = ref(g_object);
  auto source = ref(_source);
  gint src_width  = source [gimp_item_get_width]  ();
  gint src_height = source [gimp_item_get_height] ();

  update_size(src_width, src_height);


  /*  the projection speaks in image coordinates, transform to layer
   *  coordinates when emitting our own update signal.
   */
  gint offset_x = self [gimp_item_get_offset_x]();
  gint offset_y = self [gimp_item_get_offset_y]();
  gint src_offset_x = source [gimp_item_get_offset_x]();
  gint src_offset_y = source [gimp_item_get_offset_y]();

  source_layer = source;
  if (prev_w != src_width || prev_h != src_height) {
    gint src_offset_diff_x = src_offset_x - prev_x;
    gint src_offset_diff_y = src_offset_y - prev_y;
    self [gimp_item_set_offset] (offset_x + src_offset_diff_x, offset_y + src_offset_diff_y);
    prev_w = src_width;
    prev_h = src_height;
  }
  prev_x = src_offset_x;
  prev_y = src_offset_y;

  PixelRegion destPR;
  gint swidth, sheight;
  swidth  = self [gimp_item_get_width] ();
  sheight = self [gimp_item_get_height] ();

  if (swidth < width || sheight < height) {
    g_print("src(w,h)=%d,%d, self(w,h)=%d,%d\n", width, height, swidth, sheight);
    width  = MIN(width,  swidth);
    height = MIN(height, sheight);
  }

  TileManager* dest_tiles = self [gimp_drawable_get_tiles] ();
  if (dest_tiles->width != swidth || dest_tiles->height != sheight) {
    g_print("size is invalid. w,h should be %d, %d but is %d, %d.\n", swidth, sheight, dest_tiles->width, dest_tiles->height);
    if (dest_tiles->width < width)
      width = dest_tiles->width;
    if (dest_tiles->height < height)
      height = dest_tiles->height;
    return;
  }
  g_print("Copy=%d,%d\n", width, height);
  pixel_region_init (&destPR, dest_tiles, x, y, width, height, TRUE);
/*
  GimpImageType self_type   = self [gimp_drawable_type] ();
  GimpImageType source_type = source [gimp_drawable_type] ();

  if (self_type == source_type) {
    PixelRegion srcPR;
    TileManager* src_tiles  = source [gimp_drawable_get_tiles] ();
    g_print("x,y,w,h=%d,%d,%d,%d\n", x, y, width, height);
    if (src_tiles && dest_tiles) {
      pixel_region_init (&srcPR,  src_tiles,  x, y, width, height, FALSE);
      copy_region_nocow(&srcPR, &destPR);
    }

  } else {
    source [gimp_drawable_project_region] (x, y, width, height, &destPR, FALSE);
  }
*/
  source [gimp_drawable_project_region] (x, y, width, height, &destPR, FALSE);

  self [gimp_drawable_update] (x, y, width, height);
}

void GLib::CloneLayer::on_source_frozen(GObject* object, GParamSpec* spec)
{
  auto self     = ref(g_object);
  auto viewable = ref(object);

  bool frozen = viewable [gimp_viewable_preview_is_frozen] ();
  if (frozen)
    self [gimp_viewable_preview_freeze] ();
  else
    self [gimp_viewable_preview_thaw] ();
}

/*  public functions  */
GimpLayer *
CloneLayerInterface::new_instance (GimpImage            *image,
                                   GimpLayer            *source,
                                   gint                  width,
                                   gint                  height,
                                   const gchar          *name,
                                   gdouble               opacity,
                                   GimpLayerModeEffects  mode)
{
  GimpImageType   type;
  GimpCloneLayer* cloned;
  gint x, y;

  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);

  type = gimp_image_base_type_with_alpha (image);

  if (source) {
    auto src = GLib::ref(source);
    if (width <= 0)
      width  = src [gimp_item_get_width] ();
    if (height <= 0)
      height = src [gimp_item_get_height] ();
    src [gimp_item_get_offset] (&x, &y);
  } else {
    auto img = GLib::ref(image);
    if (width <= 0)
      width  = img [gimp_image_get_width] ();
    if (height <= 0)
      height = img [gimp_image_get_height] ();
    x = y = 0;
  }

  g_print("create clone instance type=%lx\n", GLib::Class::Traits::get_type());
  cloned = GLib::Class::Traits::cast (gimp_drawable_new (GLib::Class::Traits::get_type(),
                                                         image, name,
                                                         x, y, width, height,
                                                         type));
  GimpLayer* layer = GIMP_LAYER(cloned);
  opacity = CLAMP (opacity, GIMP_OPACITY_TRANSPARENT, GIMP_OPACITY_OPAQUE);

  layer->opacity = opacity;
  layer->mode    = mode;

  CloneLayerInterface::cast(cloned)->set_source(source);

  g_print("new_instance::cloned=%p[layer=%p]\n", cloned, layer);

  return layer;
}

CloneLayerInterface* CloneLayerInterface::cast(gpointer obj)
{
  if (!is_instance(obj))
    return NULL;
  return dynamic_cast<CloneLayerInterface*>(GLib::Class::get_private(obj));
}

bool CloneLayerInterface::is_instance(gpointer obj)
{
  return GLib::Class::Traits::is_instance(obj);
}

GType gimp_clone_layer_get_type() {
  return GLib::Class::Traits::get_type();
}

GimpLayer*
gimp_clone_layer_new            (GimpImage            *image,
                                 GimpLayer            *source,
                                 gint                  width,
                                 gint                  height,
                                 const gchar          *name,
                                 gdouble               opacity,
                                 GimpLayerModeEffects  mode)
{
  return CloneLayerInterface::new_instance(image, source, width, height, name, opacity, mode);
}

void
gimp_clone_layer_set_source    (GimpCloneLayer* layer, GimpLayer* source)
{
  if (CloneLayerInterface::is_instance(layer))
    CloneLayerInterface::cast(layer)->set_source(source);
}

GimpLayer*
gimp_clone_layer_get_source    (GimpCloneLayer* layer)
{
  if (CloneLayerInterface::is_instance(layer))
    return CloneLayerInterface::cast(layer)->get_source();
  return NULL;
}

void
gimp_clone_layer_set_source_by_name    (GimpCloneLayer* layer, const gchar* name)
{
  if (CloneLayerInterface::is_instance(layer))
    CloneLayerInterface::cast(layer)->set_source_by_name(name);
}
