/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include "paint-core.hpp"
#include "paint/painter-mypaint/engine.hpp"
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimplayer.h"
#include "core/gimpimage.h"
#include "paint/gimppaintcore.h"
#include "paint/gimppaintoptions.h"
}
#include "painter/object-ref.hpp"
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
namespace GimpPainter {
template<> struct TypeTraits<GimpImage> { static GType type () noexcept { return GIMP_TYPE_IMAGE; } };
template<> struct TypeTraits<GimpPaintCore> { static GType type () noexcept { return GIMP_TYPE_PAINT_CORE; } };
template<> struct TypeTraits<GimpPaintOptions> { static GType type () noexcept { return GIMP_TYPE_PAINT_OPTIONS; } };
template<> struct TypeTraits<GimpDrawable> { static GType type () noexcept { return GIMP_TYPE_DRAWABLE; } };
namespace MyPaint {
namespace {
class HoverSurface final : public Surface {
  GeglSurface& source_;
  Engine* tail_;
public:
  HoverSurface(GeglSurface&source,Engine*tail):source_(source),tail_(tail){}
  bool draw_dab(float x,float y,float radius,float r,float g,float b,float opaque,
                float hardness,float alpha,float aspect,float angle,float lock_alpha,
                float colorize,float grain,float contrast) override
  {
    // A real preceding press can leave interpolated positive-pressure dabs
    // before this zero-pressure input is reached. Preserve that release tail,
    // including native pipe selection and the evaluator's split timing. Once
    // pressure reaches zero, even constant-opacity brushes remain sample-only.
    // No transient hover adapter is ever allowed to start a paint transaction.
    return tail_ && tail_->state(STATE_PRESSURE)>0 &&
      source_.draw_dab(x,y,radius,r,g,b,opaque,hardness,alpha,aspect,angle,
                       lock_alpha,colorize,grain,contrast);
  }
  void get_color(float x,float y,float radius,float*r,float*g,float*b,float*a,
                 float hardness,float aspect,float angle,float grain,float contrast) override
  { source_.get_color(x,y,radius,r,g,b,a,hardness,aspect,angle,grain,contrast); }
  void begin_session() override {}
  void end_session() override {}
};
struct Processing {
  bool&active;
  explicit Processing(bool&value):active(value){if(active)throw std::logic_error("Paint sample is already in progress");active=true;}
  ~Processing(){active=false;}
};
}
struct PaintCore::Impl : std::enable_shared_from_this<Impl> {
  ObjectRef<GimpPaintCore> native;
  ObjectRef<GimpPaintOptions> options;
  ObjectRef<GimpDrawable> drawable;
  ObjectRef<GimpImage> image;
  Resource resource;
  Engine engine;
  std::shared_ptr<GeglSurface> surface;
  std::shared_ptr<GimpResources> resources;
  bool started=false,starting=false,ending=false,cancel_requested=false,finish_requested=false,processing=false,logical=false;
  bool batch=false,batch_aborted=false,push_undo=true;
  std::unique_ptr<gchar,decltype(&g_free)> saved_undo_desc{nullptr,g_free};
  bool custom_undo_desc=false;
  void validate_input(double dt,const GimpCoords&c)
  {
    bool valid=std::isfinite(dt);
    for(auto value:{c.x,c.y,c.pressure,c.xtilt,c.ytilt})
      valid=valid&&std::isfinite(value)&&std::abs(value)<=std::numeric_limits<float>::max();
    if(valid)valid=std::abs(float(c.x))<1e8&&std::abs(float(c.y))<1e8;
    if(!valid){if(batch)end(false);throw std::invalid_argument("Invalid brush input sample");}
  }
  WeakRef<GimpDrawable> logical_target;
  std::uint64_t lifecycle=0;
  std::size_t read=0,written=0;
  Impl(GimpPaintOptions *o,const Resource&r)
    :native(ObjectRef<GimpPaintCore>::adopt(GIMP_PAINT_CORE(g_object_new(GIMP_TYPE_PAINT_CORE,"undo-desc","Painter MyPaint",nullptr)))),
     options(ObjectRef<GimpPaintOptions>::retain(o)),resource(r),engine(r)
  {if(!options)throw std::invalid_argument("Expected paint options");}
  ~Impl() noexcept {try{end(false);}catch(...){}restore_undo_desc();}
  void restore_undo_desc() noexcept
  {if(custom_undo_desc){auto*retired=native.get()->undo_desc;native.get()->undo_desc=saved_undo_desc.release();custom_undo_desc=false;g_free(retired);}}
  void release_native_scratch()
  {
    auto*c=native.get();
    g_clear_object(&c->mask_buffer);
    if(c->applicators){g_hash_table_unref(c->applicators);c->applicators=nullptr;}
    if(c->stroke_buffer){g_array_free(c->stroke_buffer,TRUE);c->stroke_buffer=nullptr;}
    c->image_pickable=nullptr;
    gimp_paint_core_cleanup(c);
  }
  void end(bool commit)
  {
    // Only end_batch may commit an atomic operation. External finish/configure,
    // close, target changes or cancellation must not publish a partial batch.
    if(batch){batch_aborted=true;commit=false;}
    if(starting){cancel_requested=true;return;}
    if(processing){finish_requested=true;if(!commit)cancel_requested=true;return;}
    if(ending)return;
    ++lifecycle;logical=false;logical_target=WeakRef<GimpDrawable>();
    cancel_requested=finish_requested=false;
    if(!started)return;
    struct Transition { bool& state; explicit Transition(bool&s):state(s){state=true;} ~Transition(){state=false;} } transition(ending);
    started=false; // callbacks may reenter the controller while updates thaw
    auto target=drawable;auto image_owner=image;
    auto raster=std::move(surface);auto owners=std::move(resources);
    auto*c=native.get();GList drawables={target.get(),nullptr,nullptr};
    if(raster){read+=raster->bytes_read();written+=raster->bytes_written();raster->end_session();}
    if(commit&&gimp_item_is_attached(GIMP_ITEM(target.get())))gimp_paint_core_finish(c,&drawables,push_undo);
    else if(c->x1==c->x2||c->y1==c->y2)gimp_paint_core_finish(c,&drawables,FALSE);
    else gimp_paint_core_cancel(c,&drawables);
    release_native_scratch();drawable.reset();image.reset();
  }
  void retire_segment(bool reset)
  {
    if(starting||ending||processing)throw std::logic_error("Paint transaction is transitioning");
    if(!batch||batch_aborted)throw std::logic_error("No usable atomic paint operation");
    {
      Processing transition(processing);
      auto raster=std::move(surface);auto owners=std::move(resources);
      ++lifecycle;logical=false;logical_target=WeakRef<GimpDrawable>();
      if(raster){read+=raster->bytes_read();written+=raster->bytes_written();raster->end_session();}
      if(reset)engine.reset();
    }
    if(cancel_requested||finish_requested)end(false);
  }
  void split()
  {if(batch)retire_segment(false);else end(true);}
  void begin_batch(bool undo,const char*description)
  {
    if(batch||started||starting||ending||processing)throw std::logic_error("Paint operation is already active");
    if(description){auto*replacement=g_strdup(description);saved_undo_desc.reset(native.get()->undo_desc);native.get()->undo_desc=replacement;custom_undo_desc=true;}
    batch=true;batch_aborted=false;push_undo=undo;logical=false;logical_target=WeakRef<GimpDrawable>();++lifecycle;engine.reset();
  }
  void end_batch(bool commit)
  {
    if(starting||ending||processing)throw std::logic_error("Paint transaction is transitioning");
    if(!batch)throw std::logic_error("No atomic paint operation");
    const bool failed=commit&&batch_aborted;
    batch=false;
    try{end(commit&&!failed);}catch(...){push_undo=true;batch_aborted=false;restore_undo_desc();throw;}
    push_undo=true;batch_aborted=false;restore_undo_desc();
    if(failed)throw std::runtime_error("Atomic paint operation was canceled");
  }
  void logical_begin(GimpDrawable*d)
  {
    auto previous=logical_target.lock();
    if(logical&&previous.get()==d)return;
    engine.new_stroke();double rgba[4];
    gegl_color_get_pixel(gimp_context_get_foreground(GIMP_CONTEXT(options.get())),GeglSurface::evaluation_format(gimp_drawable_get_buffer(d)),rgba);
    engine.set_foreground(rgba[0],rgba[1],rgba[2]);
    if(GIMP_IS_LAYER(d))engine.set_base_value(BRUSH_LOCK_ALPHA,gimp_layer_get_lock_alpha(GIMP_LAYER(d))?1.f:0.f);
    logical=true;logical_target=WeakRef<GimpDrawable>(ObjectRef<GimpDrawable>::retain(d));
  }
  bool begin(GimpDrawable *d,const GimpCoords&coords)
  {
    if(starting||ending)throw std::logic_error("Paint transaction is transitioning");
    const bool fresh=!started;
    starting=true;cancel_requested=finish_requested=false;
    try {
      // Own both endpoints before resource/native callbacks. GimpItem's image
      // link is weak, so a retained drawable alone cannot protect native start.
      if(fresh)drawable=ObjectRef<GimpDrawable>::retain(d);
      else if(drawable.get()!=d)throw std::invalid_argument("Atomic paint target changed");
      if(!gimp_item_is_attached(GIMP_ITEM(d)))throw std::invalid_argument("Drawable is not attached");
      if(fresh)image=ObjectRef<GimpImage>::retain(gimp_item_get_image(GIMP_ITEM(d)));
      else if(gimp_item_get_image(GIMP_ITEM(d))!=image.get())throw std::invalid_argument("Atomic paint image changed");
      if(gimp_item_is_content_locked(GIMP_ITEM(d),nullptr))throw std::invalid_argument("Drawable content is locked");
      auto raster=std::make_shared<GeglSurface>(gimp_drawable_get_buffer(d));
      auto owners=std::make_shared<GimpResources>(GIMP_CONTEXT(options.get()),resource);
      raster->set_non_incremental(engine.non_incremental());raster->set_stroke_opacity(engine.stroke_opacity());owners->attach(*raster);
      double rgba[4];
      gegl_color_get_pixel(gimp_context_get_background(GIMP_CONTEXT(options.get())),GeglSurface::evaluation_format(gimp_drawable_get_buffer(d)),rgba);
      raster->set_background(rgba[0],rgba[1],rgba[2]);
      if(!gimp_item_is_attached(GIMP_ITEM(d))||gimp_item_get_image(GIMP_ITEM(d))!=image.get())
        throw std::invalid_argument("Drawable changed during resource setup");
      GList drawables={d,nullptr,nullptr};GError*error=nullptr;
      if(fresh&&!gimp_paint_core_start(native.get(),&drawables,options.get(),&coords,&error)) {
        const std::string message=error?error->message:"Unable to start paint transaction";g_clear_error(&error);throw std::runtime_error(message);
      }
      started=true;surface=raster;resources=owners;
      // notify::frozen may request cancellation or remove the drawable. Do not
      // expose another begin, configure, or paint while native start unwinds.
      if(!gimp_item_is_attached(GIMP_ITEM(d))||gimp_item_get_image(GIMP_ITEM(d))!=image.get())cancel_requested=true;
      if(!cancel_requested) {
        int x,y;gimp_item_get_offset(GIMP_ITEM(d),&x,&y);raster->set_selection(native.get()->mask_buffer,x,y);
        // A later logical segment samples/composites against its own current
        // pixels, while the one native Undo snapshot still precedes the batch.
        raster->begin_session_from(fresh?GEGL_BUFFER(g_hash_table_lookup(native.get()->undo_buffers,d)):nullptr);
        std::weak_ptr<Impl> weak=shared_from_this();
        raster->set_dirty_callback([weak](const GeglRectangle&rect){
          auto self=weak.lock();if(!self||!self->started)return;
          auto target=self->drawable;auto core=self->native;auto image_owner=self->image;
          if(!target||!gimp_item_is_attached(GIMP_ITEM(target.get())))return;
          auto*c=core.get();
          if(c->x1==c->x2||c->y1==c->y2){c->x1=rect.x;c->y1=rect.y;c->x2=rect.x+rect.width;c->y2=rect.y+rect.height;}
          else {c->x1=std::min(c->x1,rect.x);c->y1=std::min(c->y1,rect.y);c->x2=std::max(c->x2,rect.x+rect.width);c->y2=std::max(c->y2,rect.y+rect.height);}
          gimp_drawable_update(target.get(),rect.x,rect.y,rect.width,rect.height);
        });
        logical_begin(d);
      }
    }catch(...){
      if(started){starting=false;end(false);}
      else {release_native_scratch();resources.reset();surface.reset();drawable.reset();image.reset();starting=false;}
      throw;
    }
    starting=false;
    if(cancel_requested){end(false);return false;}
    return true;
  }
  bool motion(GimpDrawable *d,double dt,const GimpCoords&coords)
  {
    if(starting||ending||processing)throw std::logic_error("Paint transaction is transitioning");
    validate_input(dt,coords); // native start converts x/y to integer extents
    if(!GIMP_IS_DRAWABLE(d))throw std::invalid_argument("Expected drawable");
    if(batch_aborted)throw std::logic_error("Atomic paint operation was canceled");
    if(batch&&started&&(drawable.get()!=d||gimp_item_get_image(GIMP_ITEM(d))!=image.get()||gimp_item_is_content_locked(GIMP_ITEM(d),nullptr))) {
      end(false);throw std::invalid_argument("Atomic paint target changed or became locked");
    }
    if(started&&drawable.get()!=d)end(true);
    if((!started||!surface)&&!begin(d,coords))return false;
    if(!gimp_item_is_attached(GIMP_ITEM(d))){end(false);throw std::invalid_argument("Drawable was detached");}
    auto raster=surface;auto owners=resources;
    owners->set_coords(coords);gimp_paint_core_set_current_coords(native.get(),&coords);
    try {
      bool split;
      {Processing sample(processing);split=engine.stroke_to(*raster,coords.x,coords.y,coords.pressure,coords.xtilt,coords.ytilt,dt);}
      gimp_paint_core_set_last_coords(native.get(),&coords);
      if(cancel_requested)end(false);
      else if(finish_requested)end(true);
      else if(split&&started)this->split();
      if(batch&&(!gimp_item_is_attached(GIMP_ITEM(d))||gimp_item_get_image(GIMP_ITEM(d))!=image.get())) {
        end(false);throw std::invalid_argument("Atomic paint target was detached");
      }
      return split;
    }catch(...){end(false);throw;}
  }
  bool hover(GimpDrawable*d,double dt,const GimpCoords&coords)
  {
    if(starting||ending||processing)throw std::logic_error("Paint transaction is transitioning");
    validate_input(dt,coords);
    if(batch_aborted)throw std::logic_error("Atomic paint operation was canceled");
    auto target=ObjectRef<GimpDrawable>::retain(d);
    if(!target||!gimp_item_is_attached(GIMP_ITEM(d)))throw std::invalid_argument("Expected attached hover drawable");
    auto image_owner=ObjectRef<GimpImage>::retain(gimp_item_get_image(GIMP_ITEM(d)));
    bool split=false;
    try {
      if(batch&&started&&drawable.get()!=d){end(false);throw std::invalid_argument("Atomic paint target changed");}
      if(started&&drawable.get()!=d)end(true);
      {
        Processing sample(processing);
        const auto epoch=lifecycle;
        auto raster=surface;auto owners=resources;const bool transient=!surface;
        if(transient) {
          raster=std::make_shared<GeglSurface>(gimp_drawable_get_buffer(d));
          owners=std::make_shared<GimpResources>(GIMP_CONTEXT(options.get()),resource);
          owners->attach(*raster);
        }
        if(lifecycle==epoch&&!cancel_requested&&!finish_requested) {
          logical_begin(d);GimpCoords hover_coords=coords;hover_coords.pressure=0;
          owners->set_coords(hover_coords);HoverSurface sample_surface(*raster,transient?nullptr:&engine);
          split=engine.stroke_to(sample_surface,coords.x,coords.y,0,coords.xtilt,coords.ytilt,dt);
          if(transient)read+=raster->bytes_read();
        }
      }
      if(cancel_requested)end(false);
      else if(finish_requested)end(true);
      else if(split)this->split();
      return split;
    }catch(...){end(false);throw;}
    // Transient image/drawable/sampling resources are never retained by hover.
  }


};
PaintCore::PaintCore(GimpPaintOptions*options,const Resource&resource):impl_(std::make_shared<Impl>(options,resource)){}
PaintCore::~PaintCore()=default;
void PaintCore::configure(const Resource&r){auto self=impl_;if(self->starting||self->ending||self->processing)throw std::logic_error("Paint transaction is transitioning");if(self->batch){self->end(false);throw std::logic_error("Brush settings changed during an atomic paint operation");}Engine validate(r);self->end(true);self->resource=r;self->engine.configure(r);}
bool PaintCore::stroke_to(GimpDrawable*d,double dt,const GimpCoords&coords){auto self=impl_;return self->motion(d,dt,coords);}
bool PaintCore::hover_to(GimpDrawable*d,double dt,const GimpCoords&coords){auto self=impl_;return self->hover(d,dt,coords);}
void PaintCore::finish(){auto self=impl_;self->end(true);}
void PaintCore::cancel(){auto self=impl_;self->end(false);}
void PaintCore::begin_batch(bool push_undo,const char*description){auto self=impl_;self->begin_batch(push_undo,description);}
void PaintCore::next_segment(){auto self=impl_;self->retire_segment(true);}
void PaintCore::end_batch(bool commit){auto self=impl_;self->end_batch(commit);}
bool PaintCore::active()const{return impl_->started;}
ResourceResolution PaintCore::resolution()const{return impl_->resources?impl_->resources->resolution():ResourceResolution{};}
std::size_t PaintCore::bytes_read()const{return impl_->read+(impl_->surface?impl_->surface->bytes_read():0);}
std::size_t PaintCore::bytes_written()const{return impl_->written+(impl_->surface?impl_->surface->bytes_written():0);}
} }
