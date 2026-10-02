/* SPDX-License-Identifier: GPL-3.0-or-later
 * Surface coverage and blend order derived from pinned gimp-painter afa43fae.
 */
#include "gegl-surface.hpp"
#include "legacy-pixel-modes.hpp"
#include "painter/object-ref.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace GimpPainter {
template<> struct TypeTraits<GeglBuffer> { static GType type () noexcept { return GEGL_TYPE_BUFFER; } };
namespace MyPaint {
namespace {
float clamp (float v) { return CLAMP (v,0.f,1.f); }
void finite (float v) { if (!std::isfinite (v)) throw std::invalid_argument ("Nonfinite Surface argument"); }
}
struct GeglSurface::Impl {
  ObjectRef<GeglBuffer> target, initial, floating, selection;
  ShapeProvider shape;
  Texture texture;
  TextureProvider texture_provider;
  std::function<void (const GeglRectangle&)> notify;
  bool non_incremental = false, active = false;
  float opacity = 1, background[3] = {1,1,1};
  int offset_x = 0, offset_y = 0;
  GeglRectangle dirty = {0,0,0,0};
  std::size_t read_bytes = 0, written_bytes = 0;
  const Babl *format,*floating_format;
  int channels;
  explicit Impl (GeglBuffer *buffer) : target (ObjectRef<GeglBuffer>::retain (buffer))
  {
    if (!target) throw std::invalid_argument ("Expected target GeglBuffer");
    const auto *native = gegl_buffer_get_format (buffer);
    floating_format = babl_format_with_space ("R'G'B'A u8", babl_format_get_space (native));
    const auto *rgb = babl_format_with_space ("R'G'B' u8", babl_format_get_space (native));
    const auto *gray = babl_format_with_space ("Y' u8", babl_format_get_space (native));
    if (native != floating_format && native != rgb && native != gray)
      throw std::invalid_argument ("Legacy Surface currently requires nonlinear Gray/RGB/RGBA u8; refusing precision conversion");
    format=native;channels=babl_format_get_bytes_per_pixel(native);
  }
  std::vector<guchar> read (GeglBuffer *buffer, const GeglRectangle& rect)
  {
    const auto *buffer_format=gegl_buffer_get_format(buffer);
    std::vector<guchar> pixels (std::size_t (rect.width)*rect.height*babl_format_get_bytes_per_pixel(buffer_format));
    gegl_buffer_get (buffer,&rect,1,buffer_format,pixels.data (),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    read_bytes += pixels.size (); return pixels;
  }
  void write (GeglBuffer *buffer, const GeglRectangle& rect, const std::vector<guchar>& pixels)
  {
    gegl_buffer_set (buffer,&rect,0,gegl_buffer_get_format(buffer),pixels.data (),GEGL_AUTO_ROWSTRIDE); written_bytes += pixels.size ();
  }
  struct Dab {
    GeglRectangle rect = {0,0,0,0};
    std::vector<float> mask;
    bool shaped = false;
    int paper_width = 0, paper_height = 0;
  };
  Dab coverage (float x,float y,float radius,float hardness,float aspect,float angle,float grain,float contrast,bool sampling)
  {
    for (auto v : {x,y,radius,hardness,aspect,angle,grain,contrast}) finite (v);
    Dab result;
    const auto texture_owner = texture_provider ? texture_provider () : nullptr;
    const Texture& paper = texture_owner ? *texture_owner : texture;
    if (!paper.values.empty ()) {
      if (paper.width <= 0 || paper.height <= 0 || paper.values.size () != std::size_t(paper.width)*paper.height)
        throw std::invalid_argument ("Texture provider returned invalid dimensions");
      result.paper_width = paper.width; result.paper_height = paper.height;
    }
    if (std::abs(x)>=1e8 || std::abs(y)>=1e8) throw std::invalid_argument("Surface coordinates exceed safe integer bounds");
    if (radius < .1f || radius > 800.f || hardness <= 0.f) return result;
    hardness = clamp (hardness); aspect = std::max (1.f,aspect);
    ShapeMask transformed; int bx=0,by=0,bw=0,bh=0;
    if (shape) {
      auto provider = shape;
      transformed = provider (radius,hardness,aspect,angle);
      if (transformed.width || transformed.height || !transformed.pixels.empty ()) {
        bw = transformed.width; bh = transformed.height;
        if (bw <= 0 || bh <= 0 || transformed.pixels.size () != std::size_t (bw)*bh)
          throw std::invalid_argument ("Invalid transformed brush mask");
        result.shaped = true;
      }
    }
    if (result.shaped) {
      bx = std::floor (x-(bw+1)/2); by = std::floor (y-(bh+1)/2);
    } else {
      const float fringe = radius+1;
      bx = std::floor (x-fringe); by = std::floor (y-fringe);
      bw = int (std::floor (x+fringe))-bx+1; bh = int (std::floor (y+fringe))-by+1;
    }
    GeglRectangle bounds = {bx,by,bw,bh};
    if (!gegl_rectangle_intersect (&result.rect,&bounds,gegl_buffer_get_extent (target.get ()))) return result;
    if (selection) {
      auto mask_rect = *gegl_buffer_get_extent (selection.get ());
      mask_rect.x -= offset_x; mask_rect.y -= offset_y;
      if (!gegl_rectangle_intersect (&result.rect,&result.rect,&mask_rect)) return result;
    }
    const int w = result.rect.width, h = result.rect.height;
    result.mask.resize (std::size_t (w)*h);
    std::vector<guchar> selected;
    if (selection) {
      selected.resize (std::size_t (w)*h); auto rect = result.rect; rect.x += offset_x; rect.y += offset_y;
      gegl_buffer_get (selection.get (),&rect,1,babl_format ("Y u8"),selected.data (),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      read_bytes += selected.size ();
    }
    float slope1 = -(1.0/hardness-1.0), offset2 = hardness != 1.f ? hardness/(1.0-hardness) : 0;
    float slope2 = hardness != 1.f ? -hardness/(1.0-hardness) : 0;
    const float rr = radius*radius, inverse = 1.0/rr;
    const float radians = angle/360*2*G_PI, cs = std::cos (radians), sn = std::sin (radians);
    const float cx = x-result.rect.x, cy = y-result.rect.y;
    for (int row = 0; row < h; ++row) for (int col = 0; col < w; ++col) {
      const int px = result.rect.x+col, py = result.rect.y+row; float alpha;
      if (result.shaped) alpha = float (transformed.pixels[(py-by)*bw+px-bx])/255.f;
      else {
        const float xx = col+.5-cx, yy = row+.5-cy;
        const float yr = (yy*cs-xx*sn)*aspect, xr = yy*sn+xx*cs;
        const float distance = (yr*yr+xr*xr)*inverse;
        if (distance > 1.f) alpha = 0;
        else { alpha = distance <= hardness ? 1.f : offset2; alpha += distance*(distance <= hardness ? slope1 : slope2); }
      }
      if (selection) alpha = clamp (alpha*(float(selected[row*w+col])/255.f)); else alpha = clamp (alpha);
      // Pinned ellipse sampling ignores paper; GIMP-mask sampling includes it.
      if (!paper.values.empty () && (!sampling || result.shaped)) {
        int tx = ((px%paper.width)+paper.width)%paper.width, ty = ((py%paper.height)+paper.height)%paper.height;
        alpha = clamp (alpha*(float(paper.values[ty*paper.width+tx])/255.f+grain)*contrast);
      }
      if (!result.shaped && alpha*65535.f < 1.f) alpha = 0.f;
      result.mask[row*w+col] = alpha;
    }
    return result;
  }
};
GeglSurface::GeglSurface (GeglBuffer *buffer) : impl_ (new Impl (buffer)) {}
GeglSurface::~GeglSurface () = default;
void GeglSurface::set_shape_provider (ShapeProvider provider) { impl_->shape = std::move (provider); }
void GeglSurface::set_texture (Texture texture)
{
  if (!texture.values.empty () && (texture.width <= 0 || texture.height <= 0 || texture.values.size () != std::size_t(texture.width)*texture.height))
    throw std::invalid_argument ("Invalid texture dimensions");
  impl_->texture = std::move (texture);
  impl_->texture_provider = {};
}
void GeglSurface::set_texture_provider (TextureProvider provider)
{ impl_->texture = {}; impl_->texture_provider = std::move (provider); }
void GeglSurface::set_selection (GeglBuffer *mask,int x,int y)
{ impl_->selection = ObjectRef<GeglBuffer>::retain (mask); impl_->offset_x=x; impl_->offset_y=y; }
void GeglSurface::set_non_incremental (bool value)
{ if (impl_->active) throw std::logic_error ("Cannot change accumulation mode mid-session"); if(value && impl_->channels==1) throw std::invalid_argument("Native Gray nonincremental extension is not yet enabled"); impl_->non_incremental=value; }
void GeglSurface::set_stroke_opacity (float value) { finite(value); impl_->opacity=clamp(value); }
void GeglSurface::set_background (float r,float g,float b)
{ for (auto v:{r,g,b}) finite(v); impl_->background[0]=r;impl_->background[1]=g;impl_->background[2]=b; }
void GeglSurface::set_dirty_callback (std::function<void(const GeglRectangle&)> callback) { impl_->notify=std::move(callback); }
const GeglRectangle& GeglSurface::dirty () const { return impl_->dirty; }
std::size_t GeglSurface::bytes_read () const { return impl_->read_bytes; }
std::size_t GeglSurface::bytes_written () const { return impl_->written_bytes; }
void GeglSurface::begin_session () { begin_session_from(nullptr); }
void GeglSurface::begin_session_from (GeglBuffer *before)
{
  if (impl_->active) throw std::logic_error ("Session already active");
  // GEGL snapshot is one copy-on-write snapshot per stroke, never per dab.
  if (before && (before==impl_->target.get() || gegl_buffer_get_format(before)!=impl_->format ||
                 !gegl_rectangle_equal(gegl_buffer_get_extent(before),gegl_buffer_get_extent(impl_->target.get()))))
    throw std::invalid_argument("Incompatible paint-core snapshot");
  auto initial=before?ObjectRef<GeglBuffer>::retain(before):ObjectRef<GeglBuffer>::adopt(gegl_buffer_dup(impl_->target.get()));
  auto floating=impl_->non_incremental?ObjectRef<GeglBuffer>::adopt(gegl_buffer_new(gegl_buffer_get_extent(impl_->target.get()),impl_->floating_format)):ObjectRef<GeglBuffer>();
  impl_->initial=std::move(initial);impl_->floating=std::move(floating);
  impl_->active=true;impl_->dirty={0,0,0,0};impl_->read_bytes=impl_->written_bytes=0;
}
void GeglSurface::end_session ()
{ impl_->active=false;impl_->initial.reset();impl_->floating.reset(); }
void GeglSurface::cancel_session ()
{
  const auto rect=impl_->dirty;
  auto notify=impl_->notify;
  const bool changed=impl_->active && rect.width && rect.height;
  if (changed) gegl_buffer_copy(impl_->initial.get(),&rect,GEGL_ABYSS_NONE,impl_->target.get(),&rect);
  end_session();
  if (changed && notify) notify(rect); // callback may release the Surface owner
}
bool GeglSurface::draw_dab (float x,float y,float radius,float r,float g,float b,float opaque,float hardness,float alpha,float aspect,float angle,float lock_alpha,float colorize,float grain,float contrast)
{
  if (!impl_->active) throw std::logic_error ("Draw requires active session");
  for (auto value:{r,g,b,opaque,alpha,lock_alpha,colorize}) finite(value);
  opaque=clamp(opaque);lock_alpha=clamp(lock_alpha);colorize=clamp(colorize);
  if (!opaque) return false;
  auto dab=impl_->coverage(x,y,radius,hardness,aspect,angle,grain,contrast,false);
  const auto rect=dab.rect; if (!rect.width || !rect.height) return false;
  auto *target=impl_->non_incremental ? impl_->floating.get() : impl_->target.get();
  auto pixels=impl_->read(target,rect); float color[4]={r,g,b,alpha};
  const int channels=babl_format_get_bytes_per_pixel(gegl_buffer_get_format(target));
  using namespace LegacyPixel;
  using Iter=BrushPixelIteratorForPlainData<ColoredBrushmarkIterator,float,float>;
  Iter iter(dab.mask.data(),color,pixels.data(),pixels.data(),rect.width,rect.height,rect.width,rect.width*channels,rect.width*channels,1,channels,channels);
  float normal=1.f;normal*=1.f-lock_alpha;normal*=1.f-colorize;
  if (normal) {
    if (alpha==1.f) draw_dab_pixels_BlendMode_Normal(iter,normal*opaque);
    else draw_dab_pixels_BlendMode_Normal_and_Eraser(iter,alpha,normal*opaque,impl_->background[0],impl_->background[1],impl_->background[2]);
  }
  if (lock_alpha) draw_dab_pixels_BlendMode_LockAlpha(iter,lock_alpha*opaque);
  impl_->write(target,rect,pixels);
  if (impl_->non_incremental) {
    auto original=impl_->read(impl_->initial.get(),rect);auto result=impl_->read(impl_->target.get(),rect);
    BrushPixelIteratorForPlainData<PixmapBrushmarkIterator,guchar,guchar> copy(pixels.data(),nullptr,original.data(),result.data(),rect.width,rect.height,rect.width*4,rect.width*impl_->channels,rect.width*impl_->channels,4,impl_->channels,impl_->channels);
    draw_dab_pixels_BlendMode_Normal_and_Eraser(copy,1.f,impl_->opacity,impl_->background[0],impl_->background[1],impl_->background[2]);
    impl_->write(impl_->target.get(),rect,result);
  }
  if (!impl_->dirty.width) impl_->dirty=rect;else gegl_rectangle_bounding_box(&impl_->dirty,&impl_->dirty,&rect);
  if (impl_->notify) impl_->notify(rect);
  return true;
}
void GeglSurface::get_color (float x,float y,float radius,float *r,float *g,float *b,float *a,float hardness,float aspect,float angle,float grain,float contrast)
{
  if (!r||!g||!b||!a) throw std::invalid_argument ("Expected sample outputs");
  *r=0;*g=1;*b=0;*a=0;
  auto dab=impl_->coverage(x,y,radius,hardness,aspect,angle,grain,contrast,true);const auto rect=dab.rect;
  if (!rect.width || !rect.height) return;
  auto pixels=impl_->read(impl_->target.get(),rect);float color[4]{};
  const int channels=impl_->channels;
  using namespace LegacyPixel;
  float weight=0,red=0,green=0,blue=0,alpha=0;
  // Legacy PixelRegion processing splits at closed-loop paper boundaries even
  // for ellipse sampling (whose coverage deliberately ignores paper). Preserve
  // those partial sums: changing the grouping changes float smudge samples.
  for (int row=0;row<rect.height;) {
    const int bh=!dab.paper_height?rect.height-row:
      std::min(rect.height-row,dab.paper_height-(((rect.y+row)%dab.paper_height+dab.paper_height)%dab.paper_height));
    for (int col=0;col<rect.width;) {
      const int bw=!dab.paper_width?rect.width-col:
        std::min(rect.width-col,dab.paper_width-(((rect.x+col)%dab.paper_width+dab.paper_width)%dab.paper_width));
      const int index=row*rect.width+col;
      BrushPixelIteratorForPlainData<ColoredBrushmarkIterator,float,float> iter(dab.mask.data()+index,color,pixels.data()+index*channels,pixels.data()+index*channels,bw,bh,rect.width,rect.width*channels,rect.width*channels,1,channels,channels);
      float sw=0,sr=0,sg=0,sb=0,sa=0;
      get_color_pixels_accumulate(iter,&sw,&sr,&sg,&sb,&sa);
      weight+=sw;red+=sr;green+=sg;blue+=sb;alpha+=sa;
      col+=bw;
    }
    row+=bh;
  }
  if (weight>0) {red/=weight;green/=weight;blue/=weight;alpha/=weight;}
  if (alpha>0) {red/=alpha;green/=alpha;blue/=alpha;}else {red=0;green=1;blue=0;}
  *r=clamp(red);*g=clamp(green);*b=clamp(blue);*a=clamp(alpha);
}
} }
