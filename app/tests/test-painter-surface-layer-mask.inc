/* SPDX-License-Identifier: GPL-3.0-or-later */
extern "C" {
#include "core/gimplayermask.h"
#include "core/gimppickable.h"
#include "core/gimpprojection.h"
}
#include <limits>
#include <algorithm>
static constexpr double layer_mask_sampler_error=2*std::numeric_limits<float>::epsilon()+1e-14;
static void layer_mask_native_sampler_precision()
{
  auto*a=gegl_buffer_new(GEGL_RECTANGLE(0,0,256,1),babl_format("R'G'B'A u8"));
  auto*y=gegl_buffer_new(GEGL_RECTANGLE(0,0,256,1),babl_format("Y u8"));
  std::vector<guchar> rgba(1024),gray(256);
  for(int i=0;i<256;++i){rgba[i*4]=20;rgba[i*4+1]=90;rgba[i*4+2]=170;rgba[i*4+3]=gray[i]=i;}
  gegl_buffer_set(a,GEGL_RECTANGLE(0,0,256,1),0,babl_format("R'G'B'A u8"),rgba.data(),GEGL_AUTO_ROWSTRIDE);
  gegl_buffer_set(y,GEGL_RECTANGLE(0,0,256,1),0,babl_format("Y u8"),gray.data(),GEGL_AUTO_ROWSTRIDE);
  double max_error=0;
  for(int i=0;i<256;++i){double av=-1,yv=-1;
    gegl_buffer_sample(a,i,0,nullptr,&av,babl_format("A double"),GEGL_SAMPLER_NEAREST,GEGL_ABYSS_NONE);
    gegl_buffer_sample(y,i,0,nullptr,&yv,babl_format("Y double"),GEGL_SAMPLER_NEAREST,GEGL_ABYSS_NONE);
    const double expected=i/255.;
    g_assert_cmpfloat_with_epsilon(av,expected,1e-15);
    g_assert_cmpfloat_with_epsilon(yv,expected,2*std::numeric_limits<float>::epsilon());
    max_error=std::max(max_error,std::abs(yv-expected));
  }
  g_test_message("512 actual nearest sampler reads; scalar max absolute error %.17g",max_error);
  g_object_unref(a);g_object_unref(y);
}
static unsigned char layer_mask_value(int x,int y)
{
  static const unsigned char levels[]={0,64,128,255};
  return levels[(x/5+y/7)%4];
}
static GimpLayerMask* layer_mask_install(GimpLayer*layer)
{
  auto*mask=gimp_layer_create_mask(layer,GIMP_ADD_MASK_WHITE,nullptr);GError*error=nullptr;
  g_assert_true(gimp_layer_add_mask(layer,mask,FALSE,&error)==mask);g_assert_no_error(error);
  std::vector<guchar> bytes(64*48);
  for(int y=0;y<48;++y)for(int x=0;x<64;++x)bytes[y*64+x]=layer_mask_value(x,y);
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(mask)),GEGL_RECTANGLE(0,0,64,48),0,babl_format("Y u8"),bytes.data(),GEGL_AUTO_ROWSTRIDE);
  // Publish the fixture's direct buffer writes through the normal drawable API.
  gimp_drawable_update(GIMP_DRAWABLE(mask),0,0,64,48);
  gimp_drawable_update(GIMP_DRAWABLE(layer),0,0,64,48);
  return mask;
}
static void layer_mask_apply_read_and_raw_paint()
{
  for(bool floating:{false,true})for(auto offset:{std::array<int,2>{0,0},{-7,5},{9,-6}}) {
    std::vector<guchar> reference;
    for(bool initial_apply:{false,true}) {
      GimpImage*image;auto*layer=layer_new(&image);auto*d=GIMP_DRAWABLE(layer);auto*options=options_new();
      gimp_item_set_offset(GIMP_ITEM(layer),offset[0],offset[1]);auto*mask=layer_mask_install(layer);
      auto*mask_buffer=gimp_drawable_get_buffer(GIMP_DRAWABLE(mask));
      const auto mask_before=GimpPainter::MyPaint::BufferRead::packed(mask_buffer,*gegl_buffer_get_extent(mask_buffer));
      gimp_layer_set_apply_mask(layer,initial_apply,FALSE);const auto before=pixels(d);
      {PaintCore core(options,settings(floating));stroke(core,d);core.finish();}
      const auto after=pixels(d);g_assert_true(after!=before);
      if(reference.empty())reference=after;else g_assert_true(reference==after);
      unsigned hidden_changed=0;
      for(int y=0;y<48;++y)for(int x=0;x<64;++x)
        if(layer_mask_value(x,y)==0&&std::memcmp(before.data()+(y*64+x)*4,after.data()+(y*64+x)*4,4))++hidden_changed;
      g_assert_cmpuint(hidden_changed,>,0); // Layer mask is not an image selection.
      auto*projection=gimp_image_get_projection(image);
      for(bool apply:{true,false,true}) {
        gimp_layer_set_apply_mask(layer,apply,FALSE);g_assert_cmpint(gimp_layer_get_apply_mask(layer),==,apply);
        gimp_image_flush(image);
        gimp_pickable_flush(GIMP_PICKABLE(projection));
        for(int y=10;y<36;y+=3)for(int x=12;x<43;x+=3) {
          const double raw=after[(y*64+x)*4+3]/255.;
          const double expected=raw*(apply?layer_mask_value(x,y)/255.:1.);
          g_assert_cmpfloat_with_epsilon(gimp_pickable_get_opacity_at(GIMP_PICKABLE(layer),x,y),expected,layer_mask_sampler_error);
          double projected=-1;
          g_assert_true(gimp_pickable_get_pixel_at(GIMP_PICKABLE(projection),x+offset[0],y+offset[1],babl_format("A double"),&projected));
          // The image projection is byte precision; allow its final alpha rounding.
          g_assert_cmpfloat_with_epsilon(projected,expected,1./255.);
        }
        g_assert_true(pixels(d)==after);
        g_assert_true(GimpPainter::MyPaint::BufferRead::packed(mask_buffer,*gegl_buffer_get_extent(mask_buffer))==mask_before);
      }
      g_object_unref(options);g_object_unref(image);
    }
  }
}
static void layer_mask_is_independent_paint_target()
{
  for(bool apply:{false,true}) {
    GimpImage*image;auto*layer=layer_new(&image);auto*mask=layer_mask_install(layer);auto*d=GIMP_DRAWABLE(mask);auto*options=options_new();
    gimp_layer_set_apply_mask(layer,apply,FALSE);const auto layer_before=pixels(GIMP_DRAWABLE(layer));
    auto*buffer=gimp_drawable_get_buffer(d);const auto before=GimpPainter::MyPaint::BufferRead::packed(buffer,*gegl_buffer_get_extent(buffer));
    {PaintCore core(options,settings(false));stroke(core,d);core.finish();}
    const auto after=GimpPainter::MyPaint::BufferRead::packed(buffer,*gegl_buffer_get_extent(buffer));
    g_assert_true(before!=after);g_assert_true(pixels(GIMP_DRAWABLE(layer))==layer_before);
    g_assert_cmpint(gimp_layer_get_apply_mask(layer),==,apply);
    g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(mask)));
    g_object_unref(options);g_object_unref(image);
  }
}
