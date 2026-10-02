/* SPDX-License-Identifier: GPL-3.0-or-later
 * Coverage arithmetic and scanline connectivity derived from pinned legacy
 * gimpimage-contiguous-region.c; storage and cooperative execution are new.
 */
#include "search.hpp"
#include "painter/object-ref.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <deque>
#include <limits>
#include <stdexcept>
namespace GimpPainter {
template<> struct TypeTraits<GeglBuffer> { static GType type () noexcept { return GEGL_TYPE_BUFFER; } };
namespace Fill {
namespace {
constexpr int tile_side = 64;
bool contains (const GeglRectangle& r, int x, int y)
{ return x>=r.x && y>=r.y && std::int64_t(x)<std::int64_t(r.x)+r.width && std::int64_t(y)<std::int64_t(r.y)+r.height; }
void validate (const GeglRectangle& r)
{
  if (r.width<0 || r.height<0 || std::int64_t(r.x)+r.width>std::numeric_limits<int>::max () || std::int64_t(r.y)+r.height>std::numeric_limits<int>::max ())
    throw std::invalid_argument ("Fill rectangle exceeds coordinate bounds");
}
/* Fixed 8-tile cache bounds resident memory regardless of canvas dimensions.
 * GEGL owns the sparse/backed raster, not an unbounded whole-image C++ array. */
class Tiles {
  struct Tile { bool valid=false,dirty=false; GeglRectangle rect={0,0,0,0}; std::uint64_t age=0; std::array<guchar,tile_side*tile_side*4> bytes{}; };
  ObjectRef<GeglBuffer> buffer_; const Babl *format_; int channels_; std::array<Tile,8> tiles_; std::uint64_t age_=0;
  void flush (Tile& t) { if(t.valid&&t.dirty) { gegl_buffer_set(buffer_.get(),&t.rect,0,format_,t.bytes.data(),tile_side*channels_);t.dirty=false; } }
  Tile& tile (int x,int y) {
    const int tx=int(std::floor(double(x)/tile_side))*tile_side,ty=int(std::floor(double(y)/tile_side))*tile_side;
    for(auto& t:tiles_)if(t.valid&&t.rect.x==tx&&t.rect.y==ty){t.age=++age_;return t;}
    auto* target=&tiles_[0];for(auto& t:tiles_)if(!t.valid||t.age<target->age)target=&t;
    flush(*target);target->valid=true;target->rect={tx,ty,int(std::min<std::int64_t>(tile_side,std::int64_t(std::numeric_limits<int>::max())-tx)),int(std::min<std::int64_t>(tile_side,std::int64_t(std::numeric_limits<int>::max())-ty))};target->age=++age_;
    gegl_buffer_get(buffer_.get(),&target->rect,1,format_,target->bytes.data(),tile_side*channels_,GEGL_ABYSS_NONE);return *target;
  }
public:
  Tiles (GeglBuffer* b,const Babl* f,int c):buffer_(ObjectRef<GeglBuffer>::retain(b)),format_(f),channels_(c){}
  void get(int x,int y,guchar* value){auto& t=tile(x,y);std::memcpy(value,&t.bytes[(std::size_t(y-t.rect.y)*tile_side+x-t.rect.x)*channels_],channels_);}
  guchar get(int x,int y){guchar v[4];get(x,y,v);return v[0];}
  void put(int x,int y,guchar v){auto&t=tile(x,y);t.bytes[std::size_t(y-t.rect.y)*tile_side+x-t.rect.x]=v;t.dirty=true;}
  void flush(){for(auto& t:tiles_)flush(t);}
};
guchar difference(const guchar* base,const guchar* pixel,int channels,bool alpha,bool transparent,bool aa,int threshold,int mask)
{
  if(!transparent&&alpha&&pixel[channels-1]==0)return 0;
  float maximum=0;
  if(transparent&&alpha)maximum=std::abs(int(base[channels-1])-int(pixel[channels-1]));
  else for(int c=0;c<channels-int(alpha);++c)maximum=std::max(maximum,float(std::abs(int(base[c])-int(pixel[c]))));
  if(mask>=0)maximum=(mask*maximum+(255-mask)*255)/255;
  if(aa&&threshold>0){float coverage=1.5-(maximum/threshold);if(coverage<=0)return 0;if(coverage<.5)return guchar(coverage*512);return 255;}
  return maximum>threshold?0:255;
}
}
struct Snapshot::Impl {
  ObjectRef<GeglBuffer> source; const Babl* format; int channels; bool alpha;
  GeglRectangle extent; std::array<guchar,4> color{};
  Impl(GeglBuffer* src,int c,bool a,int x,int y):channels(c),alpha(a) {
    if(!src||c<1||c>4||a!=(c==2||c==4))throw std::invalid_argument("Invalid fill source format");
    const char* names[]={"Y' u8","Y'A u8","R'G'B' u8","R'G'B'A u8"};
    const Babl* native=gegl_buffer_get_format(src);format=babl_format_with_space(names[c-1],babl_format_get_space(native));
    if(native!=format)throw std::invalid_argument("Fill requires native nonlinear byte source; no silent conversion");
    extent=*gegl_buffer_get_extent(src);validate(extent);
    source=ObjectRef<GeglBuffer>::adopt(gegl_buffer_dup(src));
    if(contains(extent,x,y)){GeglRectangle p={x,y,1,1};gegl_buffer_get(source.get(),&p,1,format,color.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);}
  }
};
Snapshot::Snapshot(GeglBuffer*s,int c,bool a,int x,int y):impl_(new Impl(s,c,a,x,y)){}
Snapshot::Snapshot(GeglBuffer*s,int c,bool a,const std::array<guchar,4>& color):impl_(new Impl(s,c,a,0,0)){impl_->color=color;}
Snapshot::~Snapshot()=default;
struct Search::Impl {
  struct Span { int y; std::int64_t left,right; };
  std::shared_ptr<const Snapshot> snapshot;
  ObjectRef<GeglBuffer> mask,output,grown;
  std::unique_ptr<Tiles> source_tiles,mask_tiles,output_tiles,grown_tiles;
  GeglRectangle bounds,extent,grow_bounds; Options options; State state=State::Searching;
  std::deque<Span> pending; Span span{}; bool active=false; std::int64_t x=0,left=0,right=0; int phase=0,grow_x=0,grow_y=0;
  std::size_t visits=0; bool transparent=false;
  Impl(std::shared_ptr<const Snapshot> snap,GeglBuffer*m,const GeglRectangle& b,int sx,int sy,Options o):snapshot(std::move(snap)),options(o){
    if(!snapshot||o.threshold<0||o.threshold>255)throw std::invalid_argument("Invalid fill parameters");
    validate(b);auto& src=*snapshot->impl_;extent=src.extent;
    gegl_rectangle_intersect(&bounds,&extent,&b);
    output=ObjectRef<GeglBuffer>::adopt(gegl_buffer_new(&extent,babl_format("Y u8")));
    output_tiles.reset(new Tiles(output.get(),babl_format("Y u8"),1));
    source_tiles.reset(new Tiles(src.source.get(),src.format,src.channels));
    if(m){if(gegl_buffer_get_format(m)!=babl_format("Y u8"))throw std::invalid_argument("Fill search mask requires Y u8 coverage");
      mask=ObjectRef<GeglBuffer>::adopt(gegl_buffer_dup(m));mask_tiles.reset(new Tiles(mask.get(),babl_format("Y u8"),1));}
    if(!contains(bounds,sx,sy)){state=State::Complete;return;}
    guchar seed[4];source_tiles->get(sx,sy,seed);transparent=o.select_transparent&&src.alpha&&seed[src.channels-1]==0;
    pending.push_back({sy,std::int64_t(sx)-1,std::int64_t(sx)+1});
  }
  guchar evaluate(int px,int py){guchar p[4];source_tiles->get(px,py,p);auto& s=*snapshot->impl_;return difference(s.color.data(),p,s.channels,s.alpha,transparent,options.antialias,options.threshold,mask_tiles?mask_tiles->get(px,py):-1);}
  void begin_grow(){
    output_tiles->flush();if(!options.grow){state=State::Complete;return;}
    grow_bounds=bounds;
    const std::int64_t x1=std::max(std::int64_t(extent.x),std::int64_t(bounds.x)-1),y1=std::max(std::int64_t(extent.y),std::int64_t(bounds.y)-1);
    const std::int64_t x2=std::min(std::int64_t(extent.x)+extent.width,std::int64_t(bounds.x)+bounds.width+1),y2=std::min(std::int64_t(extent.y)+extent.height,std::int64_t(bounds.y)+bounds.height+1);
    grow_bounds={int(x1),int(y1),int(x2-x1),int(y2-y1)};grow_x=grow_bounds.x;grow_y=grow_bounds.y;
    grown=ObjectRef<GeglBuffer>::adopt(gegl_buffer_new(&extent,babl_format("Y u8")));grown_tiles.reset(new Tiles(grown.get(),babl_format("Y u8"),1));state=State::Growing;
  }
  State step(std::size_t budget){
    while(budget && (state==State::Searching||state==State::Growing)){
      if(state==State::Growing){
        guchar v=0;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){auto xx=std::int64_t(grow_x)+dx,yy=std::int64_t(grow_y)+dy;if(xx>=extent.x&&xx<std::int64_t(extent.x)+extent.width&&yy>=extent.y&&yy<std::int64_t(extent.y)+extent.height)v=std::max(v,output_tiles->get(int(xx),int(yy)));}
        grown_tiles->put(grow_x,grow_y,v);--budget;++visits;
        if(++grow_x==grow_bounds.x+grow_bounds.width){grow_x=grow_bounds.x;if(++grow_y==grow_bounds.y+grow_bounds.height){grown_tiles->flush();state=State::Complete;}}continue;
      }
      if(!active){if(pending.empty()){begin_grow();continue;}span=pending.front();pending.pop_front();x=span.left+1;active=true;phase=0;}
      if(phase==0){
        if(x>=span.right){active=false;continue;}
        --budget;++visits;
        if(output_tiles->get(x,span.y)){++x;continue;}
        const guchar v=evaluate(x,span.y);if(!v){++x;continue;}
        output_tiles->put(x,span.y,v);left=x-1;right=x+1;phase=1;
      }else if(phase==1){
        if(left<bounds.x){phase=2;continue;}--budget;++visits;auto v=evaluate(left,span.y);output_tiles->put(left,span.y,v);if(v)--left;else phase=2;
      }else{
        if(right<bounds.x+bounds.width){--budget;++visits;auto v=evaluate(right,span.y);output_tiles->put(right,span.y,v);if(v){++right;continue;}}
        if(span.y>bounds.y)pending.push_back({span.y-1,left,right});
        if(span.y+1<bounds.y+bounds.height)pending.push_back({span.y+1,left,right});
        x=right+1;phase=0;
      }
    }return state;
  }
};
Search::Search(std::shared_ptr<const Snapshot>s,GeglBuffer*m,const GeglRectangle&b,int x,int y,Options o):impl_(new Impl(std::move(s),m,b,x,y,o)){}
Search::~Search()=default;
Search::State Search::step(std::size_t b){return impl_->step(b);}
Search::State Search::state()const noexcept{return impl_->state;}
void Search::cancel()noexcept{impl_->state=State::Cancelled;}
GeglBuffer* Search::result()const noexcept{return state()==State::Complete?(impl_->grown?impl_->grown.get():impl_->output.get()):nullptr;}
std::size_t Search::visited()const noexcept{return impl_->visits;}
} }
