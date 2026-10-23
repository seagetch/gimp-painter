/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "search.hpp"
#include <vector>
#include <stdexcept>
using namespace GimpPainter::Fill;
namespace {
GeglBuffer* buffer(const GeglRectangle&r,const char*f,const std::vector<guchar>&p){auto*b=gegl_buffer_new(&r,babl_format(f));gegl_buffer_set(b,&r,0,babl_format(f),p.data(),GEGL_AUTO_ROWSTRIDE);return b;}
std::vector<guchar> read(GeglBuffer*b){auto r=*gegl_buffer_get_extent(b);std::vector<guchar>p(std::size_t(r.width)*r.height);gegl_buffer_get(b,&r,1,babl_format("Y u8"),p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return p;}
void finish(Search&job,std::size_t budget=1){while(job.state()!=Search::State::Complete){auto n=job.visited();job.step(budget);g_assert_cmpuint(job.visited()-n,<=,budget);}}
void barriers(){
  GeglRectangle r={0,0,7,5};std::vector<guchar>p(35,40),m(35,255);for(int y=0;y<5;++y)m[y*7+3]=0;
  auto*b=buffer(r,"Y' u8",p);auto*mask=buffer(r,"Y u8",m);auto s=std::make_shared<Snapshot>(b,1,false,1,2);Search::Options o;o.grow=false;
  Search bounded(s,mask,r,1,2,o);finish(bounded);auto out=read(bounded.result());for(int y=0;y<5;++y)for(int x=0;x<7;++x)g_assert_cmpint(out[y*7+x],==,x<3?255:0);
  // The old penalty at threshold255 intentionally allows zero-mask traversal.
  o.threshold=255;Search high(s,mask,r,1,2,o);finish(high);for(auto v:read(high.result()))g_assert_cmpint(v,==,255);
  // A zero-mask wall can be bypassed in the full image, but not bounded ROI.
  m[3]=255;gegl_buffer_set(mask,&r,0,babl_format("Y u8"),m.data(),GEGL_AUTO_ROWSTRIDE);o.threshold=30;GeglRectangle roi={0,1,7,4};Search limited(s,mask,roi,1,2,o);finish(limited);out=read(limited.result());g_assert_cmpint(out[2*7+5],==,0);
  Search global(s,mask,r,1,2,o);finish(global);out=read(global.result());g_assert_cmpint(out[2*7+5],==,255);g_object_unref(b);g_object_unref(mask);
}
void immutable(){
  GeglRectangle r={-4,-3,8,6};std::vector<guchar>p(48,73),m(48,255);auto*b=buffer(r,"Y' u8",p);auto*mask=buffer(r,"Y u8",m);auto s=std::make_shared<Snapshot>(b,1,false,-1,-1);Search::Options o;o.grow=false;Search first(s,mask,r,-1,-1,o);
  std::fill(p.begin(),p.end(),0);gegl_buffer_set(b,&r,0,babl_format("Y' u8"),p.data(),GEGL_AUTO_ROWSTRIDE);gegl_buffer_clear(mask,&r);g_object_unref(mask);g_object_unref(b);
  finish(first,7);for(auto v:read(first.result()))g_assert_cmpint(v,==,255);
  Search second(s,nullptr,r,0,0,o);finish(second,16);g_assert(read(first.result())==read(second.result()));
}
void connectivity_grow(){
  GeglRectangle r={0,0,5,5};std::vector<guchar>p(25,255);p[6]=p[12]=20;auto*b=buffer(r,"Y' u8",p);auto s=std::make_shared<Snapshot>(b,1,false,1,1);Search::Options o;o.grow=false;
  Search search(s,nullptr,r,1,1,o);finish(search);auto out=read(search.result());g_assert_cmpint(out[6],==,255);g_assert_cmpint(out[12],==,0);
  o.grow=true;GeglRectangle roi={1,1,1,1};Search grow(s,nullptr,roi,1,1,o);finish(grow);out=read(grow.result());for(int y=0;y<5;++y)for(int x=0;x<5;++x)g_assert_cmpint(out[y*5+x],==,(x<3&&y<3)?255:0);g_object_unref(b);
}
void cancellation(){
  GeglRectangle r={0,0,512,512};std::vector<guchar>p(512*512,10);auto*b=buffer(r,"Y' u8",p);auto s=std::make_shared<Snapshot>(b,1,false,250,250);Search::Options o;Search search(s,nullptr,r,250,250,o);g_assert_null(search.result());search.step(0);g_assert_cmpuint(search.visited(),==,0);search.step(13);g_assert_cmpuint(search.visited(),==,13);g_assert_null(search.result());search.cancel();g_assert(search.state()==Search::State::Cancelled);search.step(10000);g_assert_cmpuint(search.visited(),==,13);g_assert_null(search.result());g_object_unref(b);
}
void validation(){
  GeglRectangle r={0,0,2,2};std::vector<guchar>p(4,0);auto*b=buffer(r,"Y' u8",p);auto s=std::make_shared<Snapshot>(b,1,false,0,0);Search::Options o;o.threshold=-1;bool caught=false;try{Search bad(s,nullptr,r,0,0,o);}catch(const std::invalid_argument&){caught=true;}g_assert_true(caught);
  o.threshold=30;Search outside(s,nullptr,r,3,0,o);g_assert(outside.state()==Search::State::Complete);for(auto v:read(outside.result()))g_assert_cmpint(v,==,0);
  GeglRectangle overflow={G_MAXINT,0,2,2};caught=false;try{Search bad(s,nullptr,overflow,0,0,o);}catch(const std::invalid_argument&){caught=true;}g_assert_true(caught);
  auto*linear=buffer(r,"Y u8",p);caught=false;try{Snapshot bad(linear,1,false,0,0);}catch(const std::invalid_argument&){caught=true;}g_assert_true(caught);g_object_unref(linear);g_object_unref(b);
}
void independent_read_domains()
{
  // More than eight horizontal cache tiles, across negative tile boundaries.
  // A child mask must not expose its populated parent's halo.
  for (int channels : {1, 2, 3, 4})
    for (std::size_t budget : {std::size_t(1), std::size_t(37), std::size_t(4096)})
      {
        const char *formats[] = {"Y' u8", "Y'A u8", "R'G'B' u8", "R'G'B'A u8"};
        const bool alpha = channels == 2 || channels == 4;
        const GeglRectangle extent = {-131, -66, 580, 5};
        const GeglRectangle mask_extent = {-125, -65, 570, 3};
        const GeglRectangle roi = {-129, -66, 576, 5};
        std::vector<guchar> pixels(580 * 5 * channels, 73);
        if (alpha)
          for (std::size_t i = channels - 1; i < pixels.size(); i += channels)
            pixels[i] = 255;
        auto *source = buffer(extent, formats[channels-1], pixels);
        auto *mask_parent = buffer(extent, "Y u8", std::vector<guchar>(580 * 5, 255));
        auto *mask = gegl_buffer_create_sub_buffer(mask_parent, &mask_extent);
        // A source mismatch cuts the reachable domain even with full coverage.
        for (int y = 0; y < 5; ++y) pixels[(y * 580 + 561) * channels] = 190;
        gegl_buffer_set(source, &extent, 0, babl_format(formats[channels-1]), pixels.data(), GEGL_AUTO_ROWSTRIDE);
        auto snapshot = std::make_shared<Snapshot>(source, channels, alpha, -120, -64);
        Search::Options options; options.grow = false; options.antialias = false; options.threshold = 0;
        Search job(snapshot, mask, roi, -120, -64, options);
        // Production owns immutable copies, not these caller references.
        gegl_buffer_clear(source, &extent); gegl_buffer_clear(mask_parent, &extent);
        g_object_unref(source); g_object_unref(mask); g_object_unref(mask_parent);
        finish(job, budget);
        const auto actual = read(job.result());
        for (int y = 0; y < extent.height; ++y)
          for (int x = 0; x < extent.width; ++x)
            {
              const int xx = x + extent.x, yy = y + extent.y;
              const bool reachable = xx >= -125 && xx < 430 && yy >= -65 && yy < -62;
              g_assert_cmpuint(actual[y * extent.width + x], ==, reachable ? 255 : 0);
            }
      }
}
void read_domain_owner_orders()
{
  const GeglRectangle extent = {-70, -2, 140, 4};
  for (bool cancel : {false, true})
    {
      auto *source = buffer(extent, "Y' u8", std::vector<guchar>(560, 40));
      auto snapshot = std::make_shared<Snapshot>(source, 1, false, -1, 0);
      std::weak_ptr<const Snapshot> weak = snapshot;
      GeglBuffer *retained = nullptr;
      {
        Search::Options options; options.grow = false;
        Search job(snapshot, nullptr, extent, -1, 0, options);
        snapshot.reset(); g_object_unref(source);
        g_assert_false(weak.expired());
        job.step(17); g_assert_null(job.result());
        if (cancel) { job.cancel(); job.step(1000); g_assert_null(job.result()); }
        else { finish(job, 31); retained = GEGL_BUFFER(g_object_ref(job.result())); }
      }
      g_assert_true(weak.expired());
      if (retained)
        { for (auto value : read(retained)) g_assert_cmpuint(value, ==, 255); g_object_unref(retained); }
    }
}

}
int main(int argc,char**argv){gegl_init(&argc,&argv);g_test_init(&argc,&argv,nullptr);g_test_add_func("/fill/barriers",barriers);g_test_add_func("/fill/immutable",immutable);g_test_add_func("/fill/connectivity-grow",connectivity_grow);g_test_add_func("/fill/cancellation",cancellation);g_test_add_func("/fill/validation",validation);g_test_add_func("/fill/independent-read-domains",independent_read_domains);g_test_add_func("/fill/read-domain-owner-orders",read_domain_owner_orders);return g_test_run();}
