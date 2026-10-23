/* SPDX-License-Identifier: GPL-3.0-or-later */
extern "C" {
#include "paint/gimppaintcore.h"
}
#include <array>
#include <cmath>

// Image-space masks are generated from an independent local-coordinate oracle.
// All painted samples are interior even for asymmetric signed item offsets.
static float selection_coverage(int x,int y)
{
  if(x<10||x>=43||y<9||y>=35)return 0;
  static const float levels[]={0,.25f,.5f,.75f,1};
  return levels[((x/3)+(y/4))%5];
}
// Selection coverage is scalar storage, not a color-space conversion fixture.
template<class T> static void selection_store_native(GeglBuffer*buffer,const std::vector<T>&values)
{
  const auto*format=gegl_buffer_get_format(buffer);
  gegl_buffer_set(buffer,GEGL_RECTANGLE(0,0,64,48),0,format,values.data(),GEGL_AUTO_ROWSTRIDE);
  std::vector<T> stored(values.size());
  gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,64,48),1,format,stored.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_true(stored==values); // Verify the independent fixture before Surface reads it.
}
static void selection_install(GimpImage*image,int ox,int oy,int kind)
{
  std::vector<float> mask(64*48);
  for(int y=0;y<48;++y)for(int x=0;x<64;++x)
    mask[y*64+x]=kind==0?0:kind==1?1:selection_coverage(x-ox,y-oy);
  auto*buffer=gimp_drawable_get_buffer(GIMP_DRAWABLE(gimp_image_get_mask(image)));
  const auto*format=gegl_buffer_get_format(buffer);
  g_assert_cmpint(babl_format_get_n_components(format),==,1);
  g_assert_cmpstr(babl_get_name(babl_format_get_model(format)),==,"Y");
  const auto*type=babl_get_name(babl_format_get_type(format,0));
  if(!std::strcmp(type,"u8")) {
    std::vector<guchar> native(mask.size());
    for(std::size_t i=0;i<mask.size();++i)native[i]=std::lround(mask[i]*255.);
    selection_store_native(buffer,native);
  } else if(!std::strcmp(type,"float")) selection_store_native(buffer,mask);
  else if(!std::strcmp(type,"double")) {
    std::vector<double> native(mask.begin(),mask.end());selection_store_native(buffer,native);
  } else g_assert_not_reached();
  g_assert_cmpint(gimp_channel_is_empty(gimp_image_get_mask(image)),==,kind==0);
}
static void selection_core_signed_offsets()
{
  for(bool floating:{false,true}) {
    std::vector<guchar> reference;
    for(auto offset:{std::array<int,2>{0,0},{-7,5},{9,-6},{-5,-4}}) {
      GimpImage*image;auto*layer=layer_new(&image);auto*d=GIMP_DRAWABLE(layer);auto*options=options_new();
      gimp_item_set_offset(GIMP_ITEM(layer),offset[0],offset[1]);selection_install(image,offset[0],offset[1],2);
      const auto before=pixels(d);
      {PaintCore core(options,settings(floating));stroke(core,d);core.finish();}
      const auto after=pixels(d);g_assert_true(after!=before);
      if(reference.empty())reference=after;else g_assert_true(reference==after);
      unsigned partial_changes=0;
      for(int y=0;y<48;++y)for(int x=0;x<64;++x) {
        const auto i=(y*64+x)*4;const float coverage=selection_coverage(x,y);
        if(coverage==0)g_assert_cmpmem(before.data()+i,4,after.data()+i,4);
        if(coverage>0&&coverage<1&&std::memcmp(before.data()+i,after.data()+i,4))++partial_changes;
      }
      g_assert_cmpuint(partial_changes,>,0);g_object_unref(options);g_object_unref(image);
    }
  }
}
static void selection_empty_is_unrestricted()
{
  for(bool floating:{false,true}) {
    std::vector<guchar> reference;
    for(int kind:{0,1}) {
      GimpImage*image;auto*layer=layer_new(&image);auto*d=GIMP_DRAWABLE(layer);auto*options=options_new();
      selection_install(image,0,0,kind);const auto before=pixels(d);
      {PaintCore core(options,settings(floating));stroke(core,d);core.finish();}
      const auto after=pixels(d);g_assert_true(after!=before);
      if(reference.empty())reference=after;else g_assert_true(reference==after);
      g_object_unref(options);g_object_unref(image);
    }
  }
}
static void selection_native_handoff_coverage()
{
  for(auto precision:{GIMP_PRECISION_U8_NON_LINEAR,GIMP_PRECISION_FLOAT_LINEAR,GIMP_PRECISION_DOUBLE_NON_LINEAR})
  for(auto offset:{std::array<int,2>{0,0},{-7,5},{9,-6}}) {
    auto*image=gimp_image_new(gimp,64,48,GIMP_RGB,precision);
    auto*layer=gimp_layer_new(image,64,48,gimp_image_get_layer_format(image,TRUE),"Selection target",1,GIMP_LAYER_MODE_NORMAL);
    g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));
    auto*d=GIMP_DRAWABLE(layer);auto*options=options_new();
    gimp_item_set_offset(GIMP_ITEM(layer),offset[0],offset[1]);selection_install(image,offset[0],offset[1],2);
    auto*core=GIMP_PAINT_CORE(g_object_new(GIMP_TYPE_PAINT_CORE,nullptr));GList drawables={d,nullptr,nullptr};
    GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=25;coords.y=22;GError*error=nullptr;
    g_assert_true(gimp_paint_core_start(core,&drawables,options,&coords,&error));g_assert_no_error(error);
    g_assert_true(core->mask_buffer==gimp_drawable_get_buffer(GIMP_DRAWABLE(gimp_image_get_mask(image))));
    auto*target=gegl_buffer_new(GEGL_RECTANGLE(0,0,64,48),babl_format("R'G'B'A double"));
    {
      GeglSurface surface(target);surface.set_selection(core->mask_buffer,offset[0],offset[1]);
      surface.set_shape_provider([](float,float,float,float){return ShapeMask{41,35,std::vector<guchar>(41*35,255)};});
      gimp_paint_core_finish(core,&drawables,FALSE);gimp_paint_core_cleanup(core);g_object_unref(core);
      g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
      g_object_add_weak_pointer(G_OBJECT(image),reinterpret_cast<gpointer*>(&image));
      g_object_unref(image);g_assert_null(image);
      // Surface retains the mask after native transaction and image owners are gone.
      surface.begin_session();g_assert_true(surface.draw_dab(26,23,20,1,0,0,1,1));surface.end_session();
      std::vector<double> rgba(64*48*4);gegl_buffer_get(target,GEGL_RECTANGLE(0,0,64,48),1,babl_format("R'G'B'A double"),rgba.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      for(int y=0;y<48;++y)for(int x=0;x<64;++x) {
        const auto i=(y*64+x)*4;const double raw=selection_coverage(x,y);
        // U8 image selection stores quantized coverage; floating masks retain fractions.
        const double expected=precision==GIMP_PRECISION_U8_NON_LINEAR?std::round(raw*255)/255:raw;
        g_assert_cmpfloat_with_epsilon(rgba[i+3],expected,1e-12);
        if(expected>0){g_assert_cmpfloat_with_epsilon(rgba[i],1,1e-12);g_assert_cmpfloat(rgba[i+1],==,0);g_assert_cmpfloat(rgba[i+2],==,0);}
      }
    }
    g_object_unref(target);g_object_unref(options);g_assert_null(image);
  }
}
