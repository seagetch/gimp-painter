/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "paint-core-pixels.hpp"
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "paint/gimppaintcore.h"
#include "gegl/gimp-gegl-utils.h"
#include "gegl/gimp-gegl-loops.h"
}
#include "painter/object-ref.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace GimpPainter { namespace MyPaint {
namespace {
using Ref = ObjectRef<GObject>;
Ref retain (gpointer p) { return Ref::retain (G_OBJECT (p)); }
struct WeakWitness {
 GWeakRef ref;
 explicit WeakWitness(gpointer object) noexcept {g_weak_ref_init(&ref,object);}
 ~WeakWitness(){g_weak_ref_clear(&ref);}
 WeakWitness(const WeakWitness&)=delete;
 Ref acquire(){return Ref::adopt(static_cast<GObject*>(g_weak_ref_get(&ref)));}
};
bool same_rect (const GeglRectangle& a, const GeglRectangle& b) noexcept
{ return a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height; }
GeglRectangle bounds (GimpDrawable *d) noexcept
{
  GeglRectangle r; gimp_item_get_offset (GIMP_ITEM(d), &r.x, &r.y);
  r.width=gimp_item_get_width(GIMP_ITEM(d)); r.height=gimp_item_get_height(GIMP_ITEM(d)); return r;
}
bool same_coords (const GimpCoords&a,const GimpCoords&b) noexcept
{
  return a.x==b.x && a.y==b.y && a.pressure==b.pressure && a.xtilt==b.xtilt &&
    a.ytilt==b.ytilt && a.wheel==b.wheel && a.distance==b.distance &&
    a.rotation==b.rotation && a.slider==b.slider && a.velocity==b.velocity &&
    a.direction==b.direction && a.xscale==b.xscale && a.yscale==b.yscale &&
    a.angle==b.angle && a.reflect==b.reflect;
}
}
struct PreparedPixels::Impl
{
  Ref core, drawable, image, target, baseline, pixels;
  GeglRectangle original{}, rectangle{}, dirty{}, baseline_extent{}, target_extent{};
  const Babl *format=nullptr;
  GimpCoords start{};
  bool matches (GimpPaintCore*c,GimpDrawable*d) const noexcept
  {
    if (G_OBJECT(c)!=core.get() || G_OBJECT(d)!=drawable.get() ||
        !c->undo_buffers || !c->original_bounds ||
        !gimp_item_is_attached(GIMP_ITEM(d)) || G_OBJECT(gimp_item_get_image(GIMP_ITEM(d)))!=image.get() ||
        G_OBJECT(gimp_drawable_get_buffer(d))!=target.get() || gimp_drawable_get_format(d)!=format ||
        g_hash_table_lookup(c->undo_buffers,d)!=baseline.get() ||
        !same_rect(bounds(d),original) ||
        !same_rect(*gegl_buffer_get_extent(GEGL_BUFFER(target.get())),target_extent) ||
        !same_rect(*gegl_buffer_get_extent(GEGL_BUFFER(baseline.get())),baseline_extent) || !same_coords(c->start_coords,start) ||
        c->x1!=dirty.x || c->y1!=dirty.y ||
        std::int64_t(c->x2)-c->x1!=dirty.width || std::int64_t(c->y2)-c->y1!=dirty.height)
      return false;
    const auto *old=static_cast<const GeglRectangle*>(g_hash_table_lookup(c->original_bounds,d));
    return old && same_rect(*old,original) && gegl_buffer_get_format(GEGL_BUFFER(baseline.get()))==format;
  }
};
PreparedPixels::PreparedPixels(std::unique_ptr<Impl> p):impl_(std::move(p)){}
PreparedPixels::~PreparedPixels()=default;
GeglBuffer* PreparedPixels::buffer()const noexcept{return GEGL_BUFFER(impl_->pixels.get());}
GimpDrawable* PreparedPixels::drawable()const noexcept{return GIMP_DRAWABLE(impl_->drawable.get());}
GimpImage* PreparedPixels::image()const noexcept{return GIMP_IMAGE(impl_->image.get());}
const GeglRectangle& PreparedPixels::rectangle()const noexcept{return impl_->rectangle;}
const GimpCoords& PreparedPixels::start_coords()const noexcept{return impl_->start;}
bool PreparedPixels::matches(GimpPaintCore*c,GimpDrawable*d)const noexcept
{return c && d && impl_->matches(c,d);}
std::unique_ptr<PreparedPixels> prepare_pixels(GimpPaintCore*c,GimpDrawable*d)
{
  if (!GIMP_IS_PAINT_CORE(c) || !GIMP_IS_DRAWABLE(d) || !gimp_item_is_attached(GIMP_ITEM(d)))
    throw std::invalid_argument("Pixel preparation requires an attached native paint transaction");
  // Install all weak witnesses before any allocation or ref can invoke arbitrary
  // toggle callbacks. Acquiring one endpoint may destroy another endpoint.
  auto *initial_image=gimp_item_get_image(GIMP_ITEM(d));
  WeakWitness core_witness(c), drawable_witness(d), image_witness(initial_image);
  auto core_lease=core_witness.acquire();
  auto drawable_lease=drawable_witness.acquire();
  auto image_lease=image_witness.acquire();
  if(!core_lease || !drawable_lease || !image_lease)
    throw std::invalid_argument("Native paint endpoint expired during acquisition");
  c=GIMP_PAINT_CORE(core_lease.get()); d=GIMP_DRAWABLE(drawable_lease.get());
  if(!gimp_item_is_attached(GIMP_ITEM(d)) ||
     gimp_item_get_image(GIMP_ITEM(d))!=initial_image)
    throw std::invalid_argument("Native paint endpoint changed during acquisition");
  auto p=std::unique_ptr<PreparedPixels::Impl>(new PreparedPixels::Impl);
  p->core=std::move(core_lease); p->drawable=std::move(drawable_lease); p->image=std::move(image_lease);
  if(!c->undo_buffers || !c->original_bounds)throw std::invalid_argument("Native paint transaction tables were cleared");
  auto *baseline=static_cast<GeglBuffer*>(g_hash_table_lookup(c->undo_buffers,d));
  const auto *old=static_cast<const GeglRectangle*>(g_hash_table_lookup(c->original_bounds,d));
  if (!baseline || !old) throw std::invalid_argument("Pixel preparation has no native rollback baseline");
  p->original=*old; p->baseline=retain(baseline); p->target=retain(gimp_drawable_get_buffer(d));
  p->format=gimp_drawable_get_format(d); p->start=c->start_coords;
  p->baseline_extent=*gegl_buffer_get_extent(baseline);
  p->target_extent=*gegl_buffer_get_extent(GEGL_BUFFER(p->target.get()));
  const GeglRectangle expected_extent={0,0,p->original.width,p->original.height};
  if(!same_rect(p->baseline_extent,expected_extent) || !same_rect(p->target_extent,expected_extent))
    throw std::invalid_argument("Pixel preparation rejects expanded native buffer extents");
  const auto width=std::int64_t(c->x2)-c->x1, height=std::int64_t(c->y2)-c->y1;
  if(width<0 || height<0 || width>std::numeric_limits<int>::max() || height>std::numeric_limits<int>::max())
    throw std::invalid_argument("Invalid native paint dirty bounds");
  p->dirty={c->x1,c->y1,int(width),int(height)};
  if(!p->matches(c,d))throw std::invalid_argument("Pixel preparation rejects changed native geometry or transaction");
  if(!width || !height)return nullptr;
  GeglRectangle extent={0,0,p->original.width,p->original.height};
  if(!gegl_rectangle_intersect(&p->rectangle,&p->dirty,&extent))return nullptr;
  gegl_rectangle_align_to_buffer(&p->rectangle,&p->rectangle,baseline,GEGL_RECTANGLE_ALIGNMENT_SUPERSET);
  if(p->rectangle.width<=0 || p->rectangle.height<=0)throw std::invalid_argument("Invalid aligned native pixel rectangle");
  const GeglRectangle output={0,0,p->rectangle.width,p->rectangle.height};
  auto *copy=gegl_buffer_new(&output,p->format);
  if(!copy)throw std::bad_alloc();
  p->pixels=Ref::adopt(G_OBJECT(copy));
  gimp_gegl_buffer_copy(baseline,&p->rectangle,GEGL_ABYSS_NONE,copy,GEGL_RECTANGLE(0,0,0,0));
  auto result=std::unique_ptr<PreparedPixels>(new PreparedPixels(std::move(p)));
  if(!result->matches(c,d))throw std::runtime_error("Native transaction changed while preparing pixels");
  return result;
}
} }
