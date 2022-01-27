/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpFilterLayer
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
#include "base/pixel-region.h"

#include "gimp.h"

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
}

#include "gimpfilterlayer.h"
#include "pdb/pdb-cxx-utils.hpp"

namespace GLib {

struct FilterLayer : virtual public ImplBase, virtual public FilterLayerInterface
{
  bool             projected_tiles_updated;
  gdouble          filter_progress;
  CXXPointer<ProcedureRunner> runner;
  CXXPointer<ProcedureRunner> new_runner;
  GList*           updates;
  int              last_index;

  bool             waiting_process_stack;
  bool             layer_projected_once;
  bool             waiting_for_runner;
  GMutex           mutex;
  GMutex           m_updates;
  bool             loaded;

  struct Rectangle {
    gint x, y;
    gint width, height;
    Rectangle(gint x_, gint y_, gint w_, gint h_) : x(x_), y(y_), width(w_), height(h_) {};
  };

  CXXPointer<Delegators::Connection> child_update_conn;
  CXXPointer<Delegators::Connection> parent_changed_conn;
  CXXPointer<Delegators::Connection> reorder_conn;
  CXXPointer<Delegators::Connection> visibility_changed_conn;

  FilterLayer(GObject* o);
  virtual ~FilterLayer();

  static void class_init(Traits<GimpFilterLayer>::Class* klass);
  template<typename IFaceClass>
  static void iface_init(IFaceClass* klass);

  static gboolean notify_filter_end_callback(FilterLayer* filter) {
    filter->notify_filter_end();
    return FALSE;
  }

  // Inherited methods
  virtual void            constructed  ();

  virtual gint64          get_memsize  (gint64          *gui_size);

  virtual GimpItem      * duplicate    (GType            new_type);
  virtual void            convert      (GimpImage       *dest_image);
  virtual void            translate    (gint             offset_x,
                                        gint             offset_y,
                                        gboolean         push_undo);
  virtual void            scale        (gint             new_width,
                                        gint             new_height,
                                        gint             new_offset_x,
                                        gint             new_offset_y,
                                        GimpInterpolationType  interp_type,
                                        GimpProgress    *progress);
  virtual void            resize       (GimpContext     *context,
                                        gint             new_width,
                                        gint             new_height,
                                        gint             offset_x,
                                        gint             offset_y);
  virtual void            flip         (GimpContext     *context,
                                        GimpOrientationType flip_type,
                                        gdouble          axis,
                                        gboolean         clip_result);
  virtual void            rotate       (GimpContext     *context,
                                        GimpRotationType rotate_type,
                                        gdouble          center_x,
                                        gdouble          center_y,
                                        gboolean         clip_result);
  virtual void            transform    (GimpContext     *context,
                                        const GimpMatrix3 *matrix,
                                        GimpTransformDirection direction,
                                        GimpInterpolationType  interpolation_type,
                                        gint             recursion_level,
                                        GimpTransformResize clip_result,
                                        GimpProgress    *progress);

  virtual gint64      estimate_memsize (gint             width,
                                        gint             height);

  virtual void         project_region (gint          x,
                                       gint          y,
                                       gint          width,
                                       gint          height,
                                       PixelRegion  *projPR,
                                       gboolean      combine);
  virtual gint         get_opacity_at (gint             x,
                                       gint             y);

  virtual void            filter_reset ();
  virtual void            notify_filter_end ();
  virtual void            update       ();
  virtual void            update_size  ();
  virtual gboolean        is_editable  ();

  virtual void            set_procedure(const char* proc_name, GArray* args = NULL);
  virtual const char*     get_procedure();
  virtual GArray*         get_procedure_args();
  virtual GValue          get_procedure_arg(int index);
  virtual void            set_procedure_arg(int index, GValue value);
  virtual void            mark_as_loaded();

  virtual bool            is_waiting_to_be_processed() {
    return runner && runner->is_running() ||
           projected_tiles_updated ||
           waiting_process_stack ||
           updates;
  };

  // For GimpProgress Interface
  virtual GimpProgress*   start        (const gchar* message,
                                        gboolean     cancelable);
  virtual void            end          ();
  virtual gboolean        is_active    ();
  virtual void            set_text     (const gchar* message);
  virtual void            set_value    (gdouble      percentage);
  virtual gdouble         get_value    ();
  virtual void            pulse        ();
  virtual gboolean        message      (Gimp*                gimp,
                                        GimpMessageSeverity severity,
                                        const gchar*        domain,
                                        const gchar*       message);

  // Event handlers
  virtual void       on_parent_changed (GimpViewable  *viewable,
                                        GimpViewable  *parent);

  virtual void         on_stack_update (GimpDrawableStack *stack,
                                        gint               x,
                                        gint               y,
                                        gint               width,
                                        gint               height,
                                        GimpItem          *item);
  virtual void        on_reorder       (GimpContainer     *container,
                                        GimpLayer         *layer,
                                        gint               index);
  virtual void   on_visibility_changed (GimpFilterLayer*   layer);

private:
  void                invalidate_area  (gint               x,
                                        gint               y,
                                        gint               width,
                                        gint               height);
  void                invalidate_whole_area ();
  void                invalidate_layer ();

  bool try_waiting_for_runner() {
    bool result;
    g_mutex_lock(&mutex);
    result = !waiting_for_runner;
    if (result)
      waiting_for_runner = true;
    g_mutex_unlock(&mutex);
    return result;
  }

  void set_waiting_for_runner(bool v) {
    g_mutex_lock(&mutex);
    waiting_for_runner = v;
    g_mutex_unlock(&mutex);
  }
};


static void delete_rectangle(void* rect){
  delete (GLib::FilterLayer::Rectangle*)rect;
}



extern const char gimp_filter_layer_name[] = "GimpFilterLayer";
using Class = NewGClass<gimp_filter_layer_name,
                     UseCStructs<GimpLayer, GimpFilterLayer>,
                     FilterLayer,
                     GimpProgress, GimpPickable>;
static Class class_instance;
#define _override(method)  Class::__(&klass->method).bind<&Impl::method>()

void FilterLayer::class_init(Traits<GimpFilterLayer>::Class *this_class)
{
  typedef FilterLayer Impl;
  g_print("FilterLayer::class_init\n");
  class_instance.with_class (this_class)->
      as_class <GObject> ([](GObjectClass* klass) {
        _override(constructed);

      })->
      as_class <GimpObject> ([](GimpObjectClass* klass) {
        _override (get_memsize);

      })->
      as_class <GimpViewable> ([](GimpViewableClass* klass) {
        klass->default_stock_id = "gimp-display-filter";

      })->
      as_class <GimpItem> ([](GimpItemClass* klass) {
        _override (duplicate);
        _override (translate);
        _override (scale);
        _override (resize);
        _override (flip);
        _override (rotate);
        _override (transform);
        _override (is_editable);
        klass->default_name   = _("Filter Layer");
        klass->rename_desc    = C_("undo-type", "Rename Filter Layer");
        klass->translate_desc = C_("undo-type", "Move Filter Layer");
        klass->scale_desc     = C_("undo-type", "Scale Filter Layer");
        klass->resize_desc    = C_("undo-type", "Resize Filter Layer");
        klass->flip_desc      = C_("undo-type", "Flip Filter Layer");
        klass->rotate_desc    = C_("undo-type", "Rotate Filter Layer");
        klass->transform_desc = C_("undo-type", "Transform Filter Layer");
        //  F::__(&item_class->convert            ).bind<&Impl::convert>();

      })->
      as_class<GimpDrawable> ([](GimpDrawableClass* klass) {
        _override (project_region);
        _override (estimate_memsize);
      });
}

template<>
void FilterLayer::iface_init<GimpPickableInterface>(GimpPickableInterface* klass)
{
  typedef FilterLayer Impl;
  g_print("FilterLayer::iface_init<GimpPickableInterface>\n");
  
  _override (get_opacity_at);
}

template<>
void FilterLayer::iface_init<GimpProgressInterface>(GimpProgressInterface* klass)
{
  typedef GLib::FilterLayer Impl;
  _override (start);
  _override (end);
  _override (is_active);
  _override (set_text);
  _override (set_value);
  _override (get_value);
  _override (pulse);
  _override (message);
}

}; // namespace

GLib::FilterLayer::FilterLayer(GObject* o) : GLib::ImplBase(o) {
  g_print("FilterLayer::FilterLayer\n");
  child_update_conn   = NULL;
  parent_changed_conn = g_signal_connect_delegator (G_OBJECT(g_object), "parent-changed",
                                                    Delegators::delegator(this, &GLib::FilterLayer::on_parent_changed));
  visibility_changed_conn = g_signal_connect_delegator (G_OBJECT(g_object), "visibility-changed",
                                Delegators::delegator(this, &GLib::FilterLayer::on_visibility_changed));
  reorder_conn        = NULL;
  updates             = NULL;
  filter_progress     = 0;
  runner              = NULL;
  projected_tiles_updated = false;
  waiting_process_stack = false;
  last_index          = -1;

  layer_projected_once = false;
  waiting_for_runner  = false;
  loaded              = false;

  g_mutex_init(&mutex);
  g_mutex_init(&m_updates);
}

GLib::FilterLayer::~FilterLayer()
{
}

void GLib::FilterLayer::constructed ()
{
  on_parent_changed(GIMP_VIEWABLE(g_object), NULL);
}

gint64 GLib::FilterLayer::get_memsize (gint64     *gui_size)
{
  gint64                 memsize = 0;

  return memsize + GIMP_OBJECT_CLASS (Class::parent_class)->get_memsize (GIMP_OBJECT(g_object),
                                                                  gui_size);
}

GimpItem* GLib::FilterLayer::duplicate (GType     new_type)
{
  GimpItem *new_item;

  g_return_val_if_fail (g_type_is_a (new_type, GIMP_TYPE_DRAWABLE), NULL);

  new_item = GIMP_ITEM_CLASS (Class::parent_class)->duplicate (GIMP_ITEM(g_object), new_type);

  if (GIMP_IS_FILTER_LAYER (new_item)) {
    FilterLayerInterface::cast(new_item)->set_procedure(get_procedure(), get_procedure_args());
  }

  return new_item;
}

void GLib::FilterLayer::convert (GimpImage *dest_image)
{
  GList                 *list;
  GIMP_ITEM_CLASS (Class::parent_class)->convert (GIMP_ITEM(g_object), dest_image);
}

void GLib::FilterLayer::translate (gint      offset_x,
                                     gint      offset_y,
                                     gboolean  push_undo)
{
  GimpLayerMask         *mask;
  GList                 *list;

  GIMP_ITEM_CLASS(Class::parent_class)->translate(GIMP_ITEM(g_object), offset_x, offset_y, push_undo);
  filter_reset();
  invalidate_layer();
}

void GLib::FilterLayer::scale (gint                   new_width,
                                 gint                   new_height,
                                 gint                   new_offset_x,
                                 gint                   new_offset_y,
                                 GimpInterpolationType  interpolation_type,
                                 GimpProgress          *progress)
{
  GimpItem*              item = GIMP_ITEM(g_object);
  GIMP_ITEM_CLASS(Class::parent_class)->scale(GIMP_ITEM(g_object), new_width, new_height, new_offset_x, new_offset_y, interpolation_type, progress);
  filter_reset();
  invalidate_layer();
}

void GLib::FilterLayer::resize (GimpContext *context,
                                  gint         new_width,
                                  gint         new_height,
                                  gint         offset_x,
                                  gint         offset_y)
{
  GimpItem*              item = GIMP_ITEM(g_object);
  GIMP_ITEM_CLASS(Class::parent_class)->resize(GIMP_ITEM(g_object), context, new_width, new_height, offset_x, offset_y);
  filter_reset();
  invalidate_layer();
}

void GLib::FilterLayer::flip (GimpContext         *context,
                                GimpOrientationType  flip_type,
                                gdouble              axis,
                                gboolean             clip_result)
{
  filter_reset();
  invalidate_layer();
}

void GLib::FilterLayer::rotate (GimpContext      *context,
                                  GimpRotationType  rotate_type,
                                  gdouble           center_x,
                                  gdouble           center_y,
                                  gboolean          clip_result)
{
  GimpItem*              item = GIMP_ITEM(g_object);
  GIMP_ITEM_CLASS(Class::parent_class)->rotate(GIMP_ITEM(g_object), context, rotate_type, center_x, center_y, clip_result);
  filter_reset();
  invalidate_layer();
}

void GLib::FilterLayer::transform (GimpContext            *context,
                                     const GimpMatrix3      *matrix,
                                     GimpTransformDirection  direction,
                                     GimpInterpolationType   interpolation_type,
                                     gint                    recursion_level,
                                     GimpTransformResize     clip_result,
                                     GimpProgress           *progress)
{
  GimpItem*              item = GIMP_ITEM(g_object);
  GIMP_ITEM_CLASS(Class::parent_class)->transform(GIMP_ITEM(g_object), context, matrix, direction, interpolation_type, recursion_level, clip_result, progress);
  filter_reset();
  invalidate_layer();
}

gint64 GLib::FilterLayer::estimate_memsize (gint                width,
                                              gint                height)
{
  GList                 *list;
  GimpImageBaseType      base_type;
  gint64                 memsize = 0;
  base_type = (GimpImageBaseType)(GIMP_IMAGE_TYPE_BASE_TYPE (gimp_drawable_type (GIMP_DRAWABLE(g_object))));

  memsize += gimp_projection_estimate_memsize (base_type, width, height);

  return memsize + GIMP_DRAWABLE_CLASS (Class::parent_class)->estimate_memsize (GIMP_DRAWABLE(g_object),
                                                                         width,
                                                                         height);
}

void GLib::FilterLayer::project_region (gint          x,
                                          gint          y,
                                          gint          width,
                                          gint          height,
                                          PixelRegion  *projPR,
                                          gboolean      combine)
{
//  g_print("project_region:%s\n", gimp_object_get_name(GIMP_OBJECT(g_object)));

// Shorcut skip when image is loaded.
  {
    GLib::synchronized locker(&m_updates);
    if (!layer_projected_once && loaded) {
      g_print("ignores initial loading.\n");
      loaded = false;
      layer_projected_once = true;
      waiting_process_stack = false;
      g_list_free_full(updates, delete_rectangle);
      updates = NULL;

      locker.exit();

      GIMP_DRAWABLE_CLASS(Class::parent_class)->project_region (GIMP_DRAWABLE(g_object), x, y, width, height, projPR, combine);
      return;
    }
  }

  auto           self         = ref( GIMP_ITEM(g_object) );
  GimpLayerMask *mask         = self [gimp_layer_get_mask] ();
  const gchar*   name         = self [gimp_object_get_name] ();
  gint           x1           = x;
  gint           y1           = y;
  gint           offset_x     = self [gimp_item_get_offset_x] ();
  gint           offset_y     = self [gimp_item_get_offset_y] ();
  gint           w            = self [gimp_item_get_width] ();
  gint           h            = self [gimp_item_get_height] ();
  gint           parent_off_x = 0;
  gint           parent_off_y = 0;
  GimpViewable*  parent       = gimp_viewable_get_parent(GIMP_VIEWABLE(g_object));

  if (parent)
    ref(parent) [gimp_item_get_offset] (&parent_off_x, &parent_off_y);


  bool updates_remained = false;
  if (!waiting_process_stack) {
    // Region is copied to projected tiles
    for (GList* list = updates; list; ) {
      GLib::synchronized locker(&m_updates);
      GList* next = g_list_next(list);
      auto rect = (Rectangle*)list->data;

      // 3-1 required to mark when part of the projected_tiles are updated since last runner->run call.
      if (x1 + offset_x - parent_off_x <= rect->x &&
          y1 + offset_y - parent_off_y <= rect->y &&
          rect->x + rect->width  <= x1 + offset_x - parent_off_x + width &&
          rect->y + rect->height <= y1 + offset_y - parent_off_y + height) {

        projected_tiles_updated = true;
        updates = g_list_delete_link (updates, list);
        delete rect;
      }
      list = next;
    }

    {
      synchronized locker(&m_updates);
      if (!layer_projected_once) {
        projected_tiles_updated = true;
        layer_projected_once    = true;

        g_list_free_full(updates, delete_rectangle);
        updates = NULL;
      }
    }

    updates_remained = updates;
  }

  // Filter effects is applied to updated layer image
  if (projected_tiles_updated &&
      !waiting_process_stack && !updates_remained) {
    bool trial = false;
    if (!runner) trial = try_waiting_for_runner();
    else         trial = runner->preserve();
    if ( trial ) {
      TileManager* tiles   = self [gimp_drawable_get_tiles] ();
      PixelRegion  srcPR, destPR;

      //projected_tiles: 1-5 Be aware of boundary. project_region's image size and size of projected_tiles differ.
      gint twidth  = w;
      gint theight = h;

      gint swidth  = tile_manager_width(projPR->tiles);
      gint sheight = tile_manager_height(projPR->tiles);

      Rectangle source_area = {
          0, 0, swidth, sheight
      };
      Rectangle dest_area = {
          0 + parent_off_x - offset_x,
          0 + parent_off_y - offset_y,
          swidth, sheight
      };

      if (dest_area.x < 0) {
        source_area.width += dest_area.x;
        dest_area.  width += dest_area.x;
        source_area.x     -= dest_area.x;
        dest_area.  x     -= dest_area.x;
      }

      if (dest_area.y < 0) {
        source_area.height += dest_area.y;
        dest_area.  height += dest_area.y;
        source_area.y      -= dest_area.y;
        dest_area.  y      -= dest_area.y;
      }

      if (dest_area.x + dest_area.width > twidth) {
        dest_area.width    = twidth  - dest_area.x;
        source_area.width  = dest_area.width;
      }

      if (dest_area.y + dest_area.height > theight) {
        dest_area.height = theight - dest_area.y;
        source_area.height = dest_area.height;
      }

      pixel_region_init (&srcPR, projPR->tiles,
                         source_area.x, source_area.y, source_area.width, source_area.height, FALSE);

      pixel_region_init (&destPR,       tiles,
                         dest_area.x, dest_area.y, dest_area.width, dest_area.height, TRUE);

      copy_region_nocow(&srcPR, &destPR);

      if (runner) {

        bool result = runner->run(GIMP_ITEM(g_object));
        g_print("--->%s: start runner=%d\n", self [gimp_object_get_name] (), result );
        auto  image   = ref(self [gimp_item_get_image] () );
        int   iwidth  = image [gimp_image_get_width] ();
        int   iheight = image [gimp_image_get_height] ();
        image [gimp_image_invalidate] (0, 0, iwidth, iheight, 0);
        image [gimp_image_flush] ();

      } else {
        g_print("--->%s: no runner\n",self [gimp_object_get_name] () );
        g_timeout_add(300, (GSourceFunc)notify_filter_end_callback, this);
      }

      projected_tiles_updated = false;
    } else
      projected_tiles_updated = true;
  }

  if (!runner || is_waiting_to_be_processed()) {
    return;
  }
//  g_print("--->%s: do parent process\n",self [gimp_object_get_name] () );

  GIMP_DRAWABLE_CLASS(Class::parent_class)->project_region (self, x, y, width, height, projPR, combine);
}

void GLib::FilterLayer::set_procedure(const char* proc_name, GArray* _args)
{
  if (runner) {
    runner = NULL;
    g_print("FilterLayer::set_procedure(%s), Cleanup existing runner.\n", proc_name);
  }
  auto           pdb   = ref( gimp_item_get_image(GIMP_ITEM(g_object))->gimp->pdb );
  auto           args  = ref<GValue>(_args);
  GimpProcedure* proc    = pdb [gimp_pdb_lookup_procedure] (proc_name);
  if (proc) {
    ProcedureRunner* r     = new ProcedureRunner(proc, GIMP_PROGRESS(g_object));

    if (args) {
      for (int i = 0; i < proc->num_args; i ++)
        r->set_arg(i, args[i]);
    }

    runner     = r;
    filter_reset();
    invalidate_layer();
  }
}

const char* GLib::FilterLayer::get_procedure()
{
  if (runner)
    return runner->get_procedure_name();
  return "";
}

GArray* GLib::FilterLayer::get_procedure_args()
{
  if (runner) {
    GValueArray* varray = runner->get_args();

    GArray *array = g_array_sized_new (FALSE, TRUE, sizeof (GValue), varray->n_values);
    g_array_set_clear_func (array, (GDestroyNotify) g_value_unset);
    g_array_append_vals(array, varray->values, varray->n_values);
    return array;
  }
  return  NULL;
}

GValue GLib::FilterLayer::get_procedure_arg(int index)
{
  if (runner)
    runner->get_arg(index);
  return  G_VALUE_INIT;
}

void GLib::FilterLayer::set_procedure_arg(int index, GValue value)
{
  if (runner)
    runner->set_arg(index, value);
}

void GLib::FilterLayer::mark_as_loaded()
{
  loaded = true;
}

gint GLib::FilterLayer::get_opacity_at (gint          x,
                                          gint          y)
{
  /* Only consider child layers as having content */
  return 0;
}


/*  priv functions  */

void GLib::FilterLayer::filter_reset () {
  g_print("FilterLayer::filter_reset\n");
//  gimp_progress_cancel(GIMP_PROGRESS(g_object));
  g_list_free_full(updates, delete_rectangle);
  updates = NULL;
  layer_projected_once = false;
}


void GLib::FilterLayer::notify_filter_end()
{
  auto self   = ref(g_object);
  gint width  = self [gimp_item_get_width] ();
  gint height = self [gimp_item_get_height] ();

  self [gimp_drawable_update] (0, 0, width, height);
  auto image  = ref( self [gimp_item_get_image]() );
  auto parent = ref( self [gimp_viewable_get_parent]() );
  set_waiting_for_runner(false);
  auto projection = ref( parent ?
      parent [gimp_group_layer_get_projection] () :
      image [gimp_image_get_projection] () );
  if (projection) {
    projection [gimp_projection_flush_now] ();
    image [gimp_image_flush] ();
  }
}


void GLib::FilterLayer::update () {
  update_size ();
}

void GLib::FilterLayer::update_size () {
  invalidate_layer();
  // TODO: need filter reset if boundary size is changed.
}

gboolean GLib::FilterLayer::is_editable()
{
  return FALSE;
}

//////////////////////////////////////////////////////////////////////////
// GimpProgress Interface
GimpProgress* GLib::FilterLayer::start(const gchar* message,
                                         gboolean     cancelable)
{
  g_print("FilterLayer::start: %s\n", message);
  return GIMP_PROGRESS(g_object);
}

void GLib::FilterLayer::end()
{
//  g_print("%s: FilterLayer::end\n", ref(g_object) [gimp_object_get_name] () );
  runner->on_end();
  if (new_runner) {
//    g_print("%s: New runner exists. run again.\n", ref(g_object) [gimp_object_get_name] () );
    runner      = std::move(new_runner);
    filter_reset();
    return;
  }

  notify_filter_end();
}

gboolean GLib::FilterLayer::is_active()
{
  return runner && runner->is_running();
//  return filter_active != NULL;
}

void GLib::FilterLayer::set_text(const gchar* message)
{
  g_print("%s: FilterLayer::set_text: %s\n", ref(g_object) [gimp_object_get_name](), message);
}

void GLib::FilterLayer::set_value(gdouble      percentage)
{
  filter_progress = percentage;
}

gdouble GLib::FilterLayer::get_value()
{
  return filter_progress;
}

void GLib::FilterLayer::pulse()
{
  g_print("FilterLayer::pulse\n");
}

gboolean GLib::FilterLayer::message(Gimp*                gimp,
                                      GimpMessageSeverity severity,
                                      const gchar*        domain,
                                      const gchar*       message)
{
  g_print("FilterLayer::message: %s / %s\n", domain, message);
  return TRUE;
}

//////////////////////////////////////////////////////////////////////////
// Internal methods
void GLib::FilterLayer::invalidate_area (gint               x,
                                         gint               y,
                                         gint               width,
                                         gint               height)
{
//  g_print("%s: invalidate_area(%d, %d  -  %d,%d)\n", ref(g_object)[gimp_object_get_name](), x, y, width, height);
  gint parent_off_x = 0;
  gint parent_off_y = 0;
  GimpViewable* parent = gimp_viewable_get_parent(GIMP_VIEWABLE(g_object));
  if (parent)
    gimp_item_get_offset(GIMP_ITEM(parent), &parent_off_x, &parent_off_y);
  GimpItem* self_item = GIMP_ITEM(g_object);
  GimpImage* image = gimp_item_get_image(self_item);
  gint item_width  = gimp_item_get_width (self_item);
  gint item_height = gimp_item_get_height(self_item);
  gint offset_x    = gimp_item_get_offset_x (self_item);
  gint offset_y    = gimp_item_get_offset_y (self_item);
  gint image_width = gimp_image_get_width(image);
  gint image_height= gimp_image_get_height(image);

  gint       x1           = x - parent_off_x,
             x2           = x1 + width,
             y1           = y - parent_off_y,
             y2           = y1 + height;
  if (x2 < offset_x - parent_off_x ||
      y2 < offset_y - parent_off_y ||
      x1 > offset_x - parent_off_x + item_width ||
      y1 > offset_y - parent_off_y + item_height )
    return;

  x1 = MAX((x1 / TILE_WIDTH ) * TILE_WIDTH,  offset_x - parent_off_x);
  y1 = MAX((y1 / TILE_HEIGHT) * TILE_HEIGHT, offset_y - parent_off_y);
  x2 = MIN(image_width  - parent_off_x,
           MIN(offset_x - parent_off_x + item_width,
               ((x2 + TILE_WIDTH  - 1) / TILE_WIDTH ) * TILE_WIDTH ));
  y2 = MIN(image_height - parent_off_y,
           MIN(offset_y - parent_off_y + item_height,
               ((y2 + TILE_HEIGHT - 1) / TILE_HEIGHT) * TILE_HEIGHT));

  for (int x3 = x1; x3 < x2; x3 += TILE_WIDTH - (x3 % TILE_WIDTH)) {
    for (int y3 = y1; y3 < y2; y3 += TILE_HEIGHT - (y3 % TILE_HEIGHT)) {
      gint w = MIN(TILE_WIDTH  - (x3 % TILE_WIDTH),  x2 - x3);
      gint h = MIN(TILE_HEIGHT - (y3 % TILE_HEIGHT), y2 - y3);

      bool found = false;
      int numlist=0;
      for (GList* list = updates; list; list = g_list_next(list)) {
        auto rect = (Rectangle*)list->data;
        if (rect->x == x3 && rect->y == y3 &&
            rect->width == w && rect->height == h) {
          found = true;
          break;
        } else if (MAX(x3, rect->x) < MIN(x3 + w, rect->x + rect->width) &&
                   MAX(y3, rect->y) < MIN(y3 + h, rect->y + rect->height)) {
//          g_print("Maybe below layer is moving:(%d,%d)+[%d,%d]<==>(%d,%d)+[%d,%d]\n",
//              x3, y3, w, h,
//              rect->x, rect->y, rect->width, rect->height);
        }
        numlist++;
      }
      if (!found) {
//        g_print("num of list=%d\n",numlist);
        {
          synchronized locker(&m_updates);
          updates = g_list_append(updates, new Rectangle(x3, y3, w, h));
        }
      }
    }
  }
}

void GLib::FilterLayer::invalidate_whole_area ()
{
  auto self         = ref(g_object);
  auto image        = ref( self [gimp_item_get_image] () );
  gint width        = self [gimp_item_get_width] ();
  gint height       = self [gimp_item_get_height] ();
  gint offset_x     = self [gimp_item_get_offset_x] ();
  gint offset_y     = self [gimp_item_get_offset_y] ();

  invalidate_area(offset_x, offset_y, width, height);
}


void GLib::FilterLayer::invalidate_layer ()
{
  auto self         = ref(g_object);
  auto image        = ref( self [gimp_item_get_image] () );
  gint width        = self [gimp_item_get_width] ();
  gint height       = self [gimp_item_get_height] ();
  gint offset_x     = self [gimp_item_get_offset_x] ();
  gint offset_y     = self [gimp_item_get_offset_y] ();

  invalidate_whole_area();


#if 1
  self [gimp_drawable_update] (0, 0, width, height);
  auto parent = ref( self [gimp_viewable_get_parent]() );
  auto projection = ref( parent ?
      parent [gimp_group_layer_get_projection] () :
      image [gimp_image_get_projection] () );
  if (projection) {
    projection [gimp_projection_flush_now] ();
    image [gimp_image_flush] ();
  }

#else
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
#endif
}


//////////////////////////////////////////////////////////////////////////
// Event handlers
void GLib::FilterLayer::on_parent_changed (GimpViewable  *viewable,
                                             GimpViewable  *parent)
{
//  g_print("on_parent_changed: %p ==> parent=%s\n", viewable, parent? gimp_object_get_name(parent): NULL);
  if (parent) {
    GimpContainer* children = gimp_viewable_get_children(parent);
    child_update_conn =
      g_signal_connect_delegator (G_OBJECT(children), "update",
                                  Delegators::delegator(this, &GLib::FilterLayer::on_stack_update));
    reorder_conn      =
      g_signal_connect_delegator (G_OBJECT(children), "reorder",
                                  Delegators::delegator(this, &GLib::FilterLayer::on_reorder));
  } else {
    GimpImage*   image   = gimp_item_get_image(GIMP_ITEM(g_object));
    GimpContainer* container = gimp_image_get_layers(image);
    child_update_conn =
      g_signal_connect_delegator (G_OBJECT(container), "update",
                                  Delegators::delegator(this, &GLib::FilterLayer::on_stack_update));
    reorder_conn      =
      g_signal_connect_delegator (G_OBJECT(container), "reorder",
                                  Delegators::delegator(this, &GLib::FilterLayer::on_reorder));
  }
  last_index = -1;
  invalidate_layer();
}

void GLib::FilterLayer::on_stack_update (GimpDrawableStack *_stack,
                                           gint               x,
                                           gint               y,
                                           gint               width,
                                           gint               height,
                                           GimpItem          *item)
{
//  g_print("%s: FilterLayer::on_stack_update(%s)\n", ref(g_object)[gimp_object_get_name](), ref(item)[gimp_object_get_name]());

  if (layer_projected_once && G_OBJECT(item) == g_object)
    return;

  auto stack = ref(_stack);
  gint item_index = stack [gimp_container_get_child_index] (GIMP_OBJECT(item));
  gint self_index = stack [gimp_container_get_child_index] (GIMP_OBJECT(g_object));
  gint stack_size = stack [gimp_container_get_n_children] ();
  auto self = ref(g_object);

  if (item_index < 0) {
    // item is removed from stack.
    gint n_children = stack [gimp_container_get_n_children] ();
    GLib::FilterLayer* bottom_layer = NULL;
    for (int i = 0; i < n_children; i ++) {
      GimpObject* layer = stack [gimp_container_get_child_by_index] (i);
      if (FilterLayerInterface::is_instance(layer)) {
        auto filter = dynamic_cast<FilterLayer*>(FilterLayerInterface::cast(layer));
        if (filter->last_index != i) {
          // filter is below item, and index is changed.
          break;
        } else {
          bottom_layer = filter;
        }
      }
    }
    if (bottom_layer) {
//      g_print("--->%s: %s is removed. invalidate %s\n", ref(g_object)[gimp_object_get_name](), ref(item)[gimp_object_get_name](), ref(bottom_layer->g_object)[gimp_object_get_name]());
      bottom_layer->invalidate_layer();
    }

  } else if (item_index < self_index) {
    // Item is above in the current stack. This layer is not affected by item.
//    g_print("--->%s: Updating above in stack(%d<%d). Ignore it.\n", ref(g_object)[gimp_object_get_name](), item_index, self_index);
    return;
  }

  bool prev_waiting_state = waiting_process_stack;

  waiting_process_stack = false;
  for (int i = self_index + 1; i < item_index; i ++) {
    GimpObject* layer = stack [gimp_container_get_child_by_index] (i);
    if (FilterLayerInterface::is_instance(layer) && ref(layer) [gimp_item_get_visible] ()) {
//      g_print("--->%s: Wait for other filter(%s).\n", ref(g_object)[gimp_object_get_name](), ref(layer) [gimp_object_get_name] ());
      waiting_process_stack = true;
      if (runner) {
        runner->stop();
        waiting_for_runner = false;
      }
      break;
    }
  }

  if (G_OBJECT(item) != g_object && FilterLayerInterface::is_instance(item) && ref(item) [gimp_item_get_visible] ()) {
    if (dynamic_cast<FilterLayer*>(FilterLayerInterface::cast(item))->is_waiting_to_be_processed())
      waiting_process_stack = true;
  }

  for (int i = self_index + 1; i < stack_size; i ++) {
    GimpObject* layer = stack [gimp_container_get_child_by_index] (i);

    if (FilterLayerInterface::is_instance(layer) && ref(layer) [gimp_item_get_visible] ()) {
      auto filter = dynamic_cast<FilterLayer*>( FilterLayerInterface::cast(layer) );
      if (filter->is_waiting_to_be_processed()) {
        waiting_process_stack = true;
        if (runner) {
          runner->stop();
          waiting_for_runner = false;
        }
        break;
      }
    }

  }

//  g_print("waiting: prev=%d --> cur=%d\n", prev_waiting_state, waiting_process_stack);
  if (prev_waiting_state && !waiting_process_stack) {
//    g_print("become !waiting_process_stack\n");
    // wating_process_stac: 5-1 (assuming below layer flush its contents if they finished the filter process.)
    filter_reset();
    invalidate_layer();

  } else if (!waiting_process_stack) {
    // waiting_process_stack: 5-1 we don't need to cache projected_tiles, nor record updates when this flag is set.
    invalidate_area(x, y, width, height);
  } else if ( !self [gimp_item_get_visible] () ) {
    invalidate_whole_area();
  }
}

void GLib::FilterLayer::on_reorder (GimpContainer* _container,
                                    GimpLayer* _layer,
                                    gint index)
{
//  g_print("%s,%d: FilterLayer::on_reorder(%s,%d)\n", ref(g_object)[gimp_object_get_name](), last_index, ref(_layer)[gimp_object_get_name](), index);
  auto layer = ref(_layer);
  auto stack = ref(_container);
  gint item_index = stack [gimp_container_get_child_index] (GIMP_OBJECT(_layer));
  gint self_index = stack [gimp_container_get_child_index] (GIMP_OBJECT(g_object));

  if (G_OBJECT(_layer) == g_object) {
    if (last_index < 0 || last_index < self_index) {
//      g_print("--->invalidate self(%s).\n", layer [gimp_object_get_name] ());
      invalidate_layer();
    }
    else {
      int n_children = stack [gimp_container_get_n_children] ();

      for (int i = last_index; i > self_index; i --) {
        auto it = ref( stack [gimp_container_get_child_by_index] (i) );
        if (FilterLayerInterface::is_instance(it) && it [gimp_item_get_visible] () ) {
//          g_print("--->invalidate bottom layer(%s).\n", it [gimp_object_get_name] ());
          reinterpret_cast<FilterLayer*>(FilterLayerInterface::cast(it))->invalidate_layer();
          break;
        }
      }

    }
  } else if (item_index > self_index) {
    bool is_at_bottom = true;
    for (int i = item_index; i > self_index; i --) {
      GimpObject* layer = stack [gimp_container_get_child_by_index] (i);
      if (FilterLayerInterface::is_instance(layer) && ref(layer) [gimp_item_get_visible] ()) {
//        g_print("--->%s: Wait for other filter(%s).\n", ref(g_object)[gimp_object_get_name](), ref(layer) [gimp_object_get_name] ());
        waiting_process_stack = true;
        if (runner) {
          runner->stop();
          waiting_for_runner = false;
        }
        is_at_bottom = false;
        break;
      }
    }
    if (is_at_bottom)
      invalidate_layer();
  } else if (item_index < self_index) {
//    invalidate_layer();
  }
  last_index = self_index;

}

void GLib::FilterLayer::on_visibility_changed (GimpFilterLayer* filter)
{
  g_print("FilterLayer::visibility-changed\n");
  auto i_filter = ref(filter);

  if (!(bool)i_filter ["visible"])
    return;

  filter_reset();
  invalidate_layer();
}

//////////////////////////////////////////////////////////////////////////
// Public functions
GimpLayer *
FilterLayerInterface::new_instance (GimpImage            *image,
                                    gint                  width,
                                    gint                  height,
                                    const gchar          *name,
                                    gdouble               opacity,
                                    GimpLayerModeEffects  mode)
{
  typedef GLib::Class F;
  GimpImageType   type;
  GimpFilterLayer *group;

  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);

  type = gimp_image_base_type_with_alpha (image);

  g_print("create instance type=%lx\n", GLib::Class::Traits::get_type());
  group = F::Traits::cast (gimp_drawable_new (GLib::Class::Traits::get_type(),
                                               image, name,
                                               0, 0, width, height,
                                               type));
  GimpLayer* layer = GIMP_LAYER(group);
  opacity = CLAMP (opacity, GIMP_OPACITY_TRANSPARENT, GIMP_OPACITY_OPAQUE);

  layer->opacity = opacity;
  layer->mode    = mode;

  g_print("new_instance::group=%p\n", group);

  return GIMP_LAYER (group);
}

FilterLayerInterface* FilterLayerInterface::cast(gpointer obj)
{
  if (!is_instance(obj))
    return NULL;
  return dynamic_cast<FilterLayerInterface*>(GLib::Class::get_private(obj));
}

bool FilterLayerInterface::is_instance(gpointer obj)
{
  return GLib::Class::Traits::is_instance(obj);
}

GType gimp_filter_layer_get_type() {
  return GLib::Class::Traits::get_type();
}

GimpLayer*
gimp_filter_layer_new            (GimpImage            *image,
                                  gint                  width,
                                  gint                  height,
                                  const gchar          *name,
                                  gdouble               opacity,
                                  GimpLayerModeEffects  mode)
{
  return FilterLayerInterface::new_instance(image, width, height, name, opacity, mode);
}

const gchar*
gimp_filter_layer_get_procedure  (GimpFilterLayer *layer)
{
  return FilterLayerInterface::cast(layer)->get_procedure ();
}

GArray*
gimp_filter_layer_get_procedure_args  (GimpFilterLayer *layer)
{
  return FilterLayerInterface::cast(layer)->get_procedure_args ();
}

void
gimp_filter_layer_set_procedure  (GimpFilterLayer* layer,
                                  const gchar*     proc_name,
                                  GArray*          args)
{
  FilterLayerInterface::cast(layer)->set_procedure (proc_name, args);
}

void
gimp_filter_layer_mark_as_loaded (GimpFilterLayer* layer)
{
  FilterLayerInterface::cast(layer)->mark_as_loaded ();
}
