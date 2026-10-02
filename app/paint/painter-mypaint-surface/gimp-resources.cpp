/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include "gimp-resources.hpp"
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpbrush.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimppattern.h"
#include "core/gimptempbuf.h"
}
#include "legacy-mask-transform.hpp"
#include "legacy-generated-mask.hpp"
#include "painter/connection.hpp"
#include <cmath>
#include <cstring>
namespace GimpPainter {
template<> struct TypeTraits<GimpBrush> { static GType type () noexcept { return GIMP_TYPE_BRUSH; } };
template<> struct TypeTraits<GimpPattern> { static GType type () noexcept { return GIMP_TYPE_PATTERN; } };
template<> struct TypeTraits<GimpContext> { static GType type () noexcept { return GIMP_TYPE_CONTEXT; } };
namespace MyPaint {
namespace {
struct BrushUse {
  ObjectRef<GimpBrush> owner;
  explicit BrushUse (GimpBrush *brush) : owner (ObjectRef<GimpBrush>::retain (brush))
  { if (owner) gimp_brush_begin_use (owner.get ()); }
  ~BrushUse () noexcept { if (owner) gimp_brush_end_use (owner.get ()); }
};
struct BitmapCache {
  ObjectRef<GimpBrush> brush;
  Connection changed;
  ShapeMask mask;
  bool valid = false;
  static void dirty (GimpData *, gpointer data) noexcept
  {
    const auto self = static_cast<std::weak_ptr<BitmapCache> *> (data)->lock ();
    if (self) self->valid = false;
  }
  static void destroy (gpointer data,GClosure *) noexcept { delete static_cast<std::weak_ptr<BitmapCache> *> (data); }
  explicit BitmapCache (GimpBrush *b) : brush (ObjectRef<GimpBrush>::retain (b)) {}
  void connect (const std::shared_ptr<BitmapCache>& self)
  {
    std::unique_ptr<std::weak_ptr<BitmapCache>> weak (new std::weak_ptr<BitmapCache> (self));
    changed = Connection::connect (ObjectRef<GObject>::retain(G_OBJECT(brush.get())),"dirty",G_CALLBACK(dirty),weak.get(),destroy);
    weak.release ();
  }
  const ShapeMask& get ()
  {
    if (!valid) {
      const auto *source = gimp_brush_get_mask (brush.get ());
      if (!source || gimp_temp_buf_get_format(source) != babl_format("Y u8"))
        throw std::invalid_argument("Bitmap painter mask must retain original byte precision");
      ShapeMask fresh;fresh.width=gimp_temp_buf_get_width(source);fresh.height=gimp_temp_buf_get_height(source);
      const auto *data=gimp_temp_buf_get_data(source);fresh.pixels.assign(data,data+std::size_t(fresh.width)*fresh.height);
      mask=std::move(fresh);valid=true;
    }
    return mask;
  }
};
struct PaperCache {
  ObjectRef<GimpPattern> pattern;
  Connection changed;
  std::shared_ptr<const Texture> cached;
  explicit PaperCache(GimpPattern *p):pattern(ObjectRef<GimpPattern>::retain(p)){}
  static void dirty(GimpData *,gpointer data) noexcept {auto self=static_cast<std::weak_ptr<PaperCache>*>(data)->lock();if(self)self->cached.reset();}
  static void destroy(gpointer data,GClosure*) noexcept {delete static_cast<std::weak_ptr<PaperCache>*>(data);}
  void connect(const std::shared_ptr<PaperCache>&self)
  {
    std::unique_ptr<std::weak_ptr<PaperCache>> weak(new std::weak_ptr<PaperCache>(self));
    changed=Connection::connect(ObjectRef<GObject>::retain(G_OBJECT(pattern.get())),"dirty",G_CALLBACK(dirty),weak.get(),destroy);weak.release();
  }
  std::shared_ptr<const Texture> get()
  {
    if(!cached) {
      auto *mask=gimp_pattern_get_mask(pattern.get());
      if(!mask)throw std::invalid_argument("Painter paper has no pixel data");
      const auto *format=gimp_temp_buf_get_format(mask);
      const int channels=babl_format_get_bytes_per_pixel(format);
      if(babl_format_get_type(format,0)!=babl_type("u8") || channels<1 || channels>4)
        throw std::invalid_argument("Painter paper requires original byte channels");
      auto fresh=std::make_shared<Texture>();fresh->width=gimp_temp_buf_get_width(mask);fresh->height=gimp_temp_buf_get_height(mask);
      fresh->values.resize(std::size_t(fresh->width)*fresh->height);const auto*bytes=gimp_temp_buf_get_data(mask);
      for(std::size_t i=0;i<fresh->values.size();++i)fresh->values[i]=bytes[i*channels];
      cached=std::move(fresh);
    }
    return cached;
  }
};
GeneratedShape shape_of(GimpBrushGenerated *b) {return static_cast<GeneratedShape>(gimp_brush_generated_get_shape(b));}
std::pair<int,int> original_size(GimpBrush *brush)
{
  if(GIMP_IS_BRUSH_GENERATED(brush)) {
    auto*b=GIMP_BRUSH_GENERATED(brush);
    return legacy_generated_dimensions(shape_of(b),b->radius,b->spikes,b->hardness,b->aspect_ratio,b->angle);
  }
  const auto*mask=gimp_brush_get_mask(brush);
  if(!mask)throw std::invalid_argument("Painter brush has no pixel data");
  return {gimp_temp_buf_get_width(mask),gimp_temp_buf_get_height(mask)};
}
}
struct GimpResources::Impl {
  ResourceResolution resolution;
  std::unique_ptr<BrushUse> brush;
  std::shared_ptr<PaperCache> paper;
  std::vector<std::shared_ptr<BitmapCache>> bitmaps;
  GimpCoords last=GIMP_COORDS_DEFAULT_VALUES,current=GIMP_COORDS_DEFAULT_VALUES;
  ShapeMask transform(float radius,float hardness,float aspect,float angle)
  {
    if(!brush || !brush->owner) throw std::logic_error("No painter brush resource");
    auto*root=brush->owner.get();const auto size=original_size(root);
    const int diameter=std::max(size.first,size.second);
    if(diameter<1)throw std::invalid_argument("Empty painter brush resource");
    auto selected=ObjectRef<GimpBrush>::retain(gimp_brush_select_brush(root,&last,&current));last=current;
    if(!selected)throw std::invalid_argument("GIMP brush selection returned no brush");
    const float scale=radius*2/diameter,gimp_aspect=20*(1.0-(1.0/aspect));
    if(GIMP_IS_BRUSH_GENERATED(selected.get())) {
      auto*b=GIMP_BRUSH_GENERATED(selected.get());double ratio=gimp_aspect==0?b->aspect_ratio:std::min(std::abs(double(gimp_aspect))+1,20.);
      double turns=-angle/360;if(gimp_aspect<0)turns+=.25;
      return generate_legacy_mask(shape_of(b),b->radius*scale,b->spikes,b->hardness*hardness,ratio,b->angle+360*turns);
    }
    std::shared_ptr<BitmapCache> cache;
    for(auto& candidate:bitmaps)if(candidate->brush.get()==selected.get()){cache=candidate;break;}
    if(!cache) {
      cache=std::make_shared<BitmapCache>(selected.get());cache->connect(cache);
      if(bitmaps.size()==16)bitmaps.erase(bitmaps.begin());
      bitmaps.push_back(cache);
    }
    return transform_bitmap_mask(cache->get(),scale,gimp_aspect,-angle/360,hardness);
  }
};
GimpResources::GimpResources(GimpContext *context,const Resource&resource,Purpose purpose):impl_(std::make_shared<Impl>())
{
  auto ctx=ObjectRef<GimpContext>::retain(context);if(!ctx)throw std::invalid_argument("Expected GimpContext");
  auto& r=impl_->resolution;
  r.requested_brush=resource.text_value(BRUSH_BRUSHMARK_NAME);r.requested_texture=resource.text_value(BRUSH_TEXTURE_NAME);
  if(resource.switch_value(BRUSH_USE_GIMP_BRUSHMARK)) {
    GimpBrush *brush=gimp_context_get_brush(context);
    const bool lookup=purpose==Purpose::Preview ? !resource.text_is_null(BRUSH_BRUSHMARK_NAME) : resource.switch_value(BRUSH_BRUSHMARK_SPECIFIED)&&!r.requested_brush.empty();
    if(lookup) {
      auto*matched=GIMP_BRUSH(gimp_container_get_child_by_name(gimp_data_factory_get_container(context->gimp->brush_factory),r.requested_brush.c_str()));
      r.brush_missing=!matched;
      if(matched||purpose==Purpose::Preview)brush=matched;
    }
    set_brush(brush);if(brush&&gimp_object_get_name(brush))r.effective_brush=gimp_object_get_name(brush);
  }
  if(resource.switch_value(BRUSH_USE_GIMP_TEXTURE)) {
    GimpPattern *pattern=gimp_context_get_pattern(context);
    const bool lookup=purpose==Purpose::Preview ? !resource.text_is_null(BRUSH_TEXTURE_NAME) : resource.switch_value(BRUSH_TEXTURE_SPECIFIED)&&!r.requested_texture.empty();
    if(lookup) {
      auto*matched=GIMP_PATTERN(gimp_container_get_child_by_name(gimp_data_factory_get_container(context->gimp->pattern_factory),r.requested_texture.c_str()));
      r.texture_missing=!matched;
      if(matched||purpose==Purpose::Preview)pattern=matched;
    }
    set_pattern(pattern);if(pattern&&gimp_object_get_name(pattern))r.effective_texture=gimp_object_get_name(pattern);
  }
}
GimpResources::~GimpResources()=default;
ResourceResolution GimpResources::resolution() const{return impl_->resolution;}
void GimpResources::set_brush(GimpBrush *brush)
{
  if(impl_->brush&&impl_->brush->owner.get()==brush)return;
  auto replacement=brush?std::unique_ptr<BrushUse>(new BrushUse(brush)):nullptr;
  auto old=std::move(impl_->brush);auto caches=std::move(impl_->bitmaps);impl_->brush=std::move(replacement);
}
void GimpResources::set_pattern(GimpPattern *pattern)
{
  if(impl_->paper&&impl_->paper->pattern.get()==pattern)return;
  auto replacement=pattern?std::make_shared<PaperCache>(pattern):nullptr;if(replacement)replacement->connect(replacement);
  impl_->paper=std::move(replacement);
}
void GimpResources::set_coords(const GimpCoords&coords){impl_->current=coords;}
void GimpResources::attach(GeglSurface&surface)
{
  auto state=impl_;
  surface.set_shape_provider([state](float r,float h,float a,float angle){return state->brush?state->transform(r,h,a,angle):ShapeMask{};});
  surface.set_texture_provider([state](){return state->paper?state->paper->get():std::shared_ptr<const Texture>();});
}
} }
