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
#include "core/gimpbrushclipboard.h"
#include "core/gimpbrushpipe.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimppattern.h"
#include "core/gimppatternclipboard.h"
#include "core/gimptempbuf.h"
#include "paint/gimppainterpaper.h"
}
#include "legacy-mask-transform.hpp"
#include "legacy-generated-mask.hpp"
#include "painter/connection.hpp"
#include <algorithm>
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
      GimpPainterPaperView paper {}; GError *error = nullptr;
      if (!gimp_painter_paper_get_view (pattern.get (), &paper, &error)) {
        std::string message (error ? error->message : "Invalid painter paper");
        g_clear_error (&error); throw std::invalid_argument (message);
      }
      auto fresh=std::make_shared<Texture>();fresh->width=paper.width;fresh->height=paper.height;
      fresh->values.resize(std::size_t(fresh->width)*fresh->height);
      for(std::size_t i=0;i<fresh->values.size();++i)fresh->values[i]=paper.data[i*paper.channels];
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
void validate_preview_brush(GimpBrush *brush,unsigned depth=0)
{
  // Native duplicate deeply copies these built-in brush types. Unknown virtual
  // selectors need an explicit isolation contract before running in previews.
  const auto type=G_OBJECT_TYPE(brush);
  if(type!=GIMP_TYPE_BRUSH&&type!=GIMP_TYPE_BRUSH_GENERATED&&
     type!=GIMP_TYPE_BRUSH_PIPE&&type!=GIMP_TYPE_BRUSH_CLIPBOARD)
    throw std::invalid_argument("Painter preview cannot isolate this brush subclass");
  if(!GIMP_IS_BRUSH_PIPE(brush))return;
  auto*pipe=GIMP_BRUSH_PIPE(brush);
  if(depth>=64||pipe->n_brushes<1||!pipe->brushes||pipe->dimension<1||
     !pipe->rank||!pipe->stride||!pipe->select||!pipe->index)
    throw std::invalid_argument("Painter preview requires a valid brush pipe");
  gint64 maximum_index=0;
  for(int i=0;i<pipe->dimension;++i) {
    if(pipe->rank[i]<1||pipe->stride[i]<0)
      throw std::invalid_argument("Painter preview requires valid brush pipe dimensions");
    maximum_index+=gint64(pipe->stride[i])*(pipe->rank[i]-1);
    if(maximum_index>G_MAXINT)
      throw std::invalid_argument("Painter preview brush pipe indices exceed native selector bounds");
  }
  for(int i=0;i<pipe->n_brushes;++i) {
    if(!pipe->brushes[i])throw std::invalid_argument("Painter preview brush pipe has an empty cell");
    validate_preview_brush(pipe->brushes[i],depth+1);
  }
}
void reset_preview_pipe(GimpBrush *brush)
{
  if(!GIMP_IS_BRUSH_PIPE(brush))return;
  auto*pipe=GIMP_BRUSH_PIPE(brush);
  std::fill(pipe->index,pipe->index+pipe->dimension,0);
  // Native duplicate already sets current/mask to its deep-copied first child.
  for(int i=0;i<pipe->n_brushes;++i)reset_preview_pipe(pipe->brushes[i]);
}
}
struct GimpResources::Impl {
  ResourceResolution resolution;
  Purpose purpose=Purpose::Stroke;
  std::unique_ptr<GRand,decltype(&g_rand_free)> random{nullptr,g_rand_free};
  std::unique_ptr<BrushUse> brush;
  std::shared_ptr<PaperCache> paper;
  std::vector<std::shared_ptr<BitmapCache>> bitmaps;
  GimpCoords last=GIMP_COORDS_DEFAULT_VALUES,current=GIMP_COORDS_DEFAULT_VALUES;
  GimpBrush* select(GimpBrush*root)
  {
    if(purpose!=Purpose::Preview||!GIMP_IS_BRUSH_PIPE(root))
      return gimp_brush_select_brush(root,&last,&current);
    auto*pipe=GIMP_BRUSH_PIPE(root);
    // Reuse native selection for every mode. Only random dimensions need a
    // private stream: the native implementation otherwise consumes global RNG.
    struct Modes {
      GimpBrushPipe*pipe;std::vector<PipeSelectModes> values;
      explicit Modes(GimpBrushPipe*p):pipe(p),values(p->select,p->select+p->dimension){}
      ~Modes(){std::copy(values.begin(),values.end(),pipe->select);}
    } modes(pipe);
    if(pipe->n_brushes>1)for(int i=0;i<pipe->dimension;++i)
      if(pipe->select[i]==PIPE_SELECT_RANDOM) {
        pipe->index[i]=g_rand_int_range(random.get(),0,pipe->rank[i]);
        pipe->select[i]=PIPE_SELECT_CONSTANT;
      }
    return gimp_brush_select_brush(root,&last,&current);
  }
  ShapeMask transform(float radius,float hardness,float aspect,float angle)
  {
    if(!brush || !brush->owner) throw std::logic_error("No painter brush resource");
    auto*root=brush->owner.get();const auto size=original_size(root);
    const int diameter=std::max(size.first,size.second);
    if(diameter<1)throw std::invalid_argument("Empty painter brush resource");
    auto selected=ObjectRef<GimpBrush>::retain(select(root));last=current;
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
  impl_->purpose=purpose;
  if(purpose==Purpose::Preview)impl_->random.reset(g_rand_new_with_seed(12345));
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
  ObjectRef<GimpBrush> snapshot;
  if(brush&&impl_->purpose==Purpose::Preview) {
    validate_preview_brush(brush);
    snapshot=ObjectRef<GimpBrush>::adopt(GIMP_BRUSH(gimp_data_duplicate(GIMP_DATA(brush))));
    if(!snapshot)throw std::invalid_argument("Painter preview brush cannot be duplicated");
    reset_preview_pipe(snapshot.get());brush=snapshot.get();
    g_rand_set_seed(impl_->random.get(),12345);
    impl_->last=impl_->current=GimpCoords GIMP_COORDS_DEFAULT_VALUES;
  }
  auto replacement=brush?std::unique_ptr<BrushUse>(new BrushUse(brush)):nullptr;
  auto old=std::move(impl_->brush);auto caches=std::move(impl_->bitmaps);impl_->brush=std::move(replacement);
}
void GimpResources::set_pattern(GimpPattern *pattern)
{
  if(impl_->paper&&impl_->paper->pattern.get()==pattern)return;
  ObjectRef<GimpPattern> snapshot;
  if(pattern&&impl_->purpose==Purpose::Preview) {
    if(G_OBJECT_TYPE(pattern)!=GIMP_TYPE_PATTERN &&
       G_OBJECT_TYPE(pattern)!=GIMP_TYPE_PATTERN_CLIPBOARD)
      throw std::invalid_argument("Painter preview cannot isolate this paper subclass");
    snapshot=ObjectRef<GimpPattern>::adopt(GIMP_PATTERN(gimp_data_duplicate(GIMP_DATA(pattern))));
    if(!snapshot)throw std::invalid_argument("Painter preview paper cannot be duplicated");
    pattern=snapshot.get();
  }
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
