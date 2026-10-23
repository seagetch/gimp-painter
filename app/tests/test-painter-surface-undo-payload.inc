/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <cstdint>
#include <cmath>
namespace UndoPayload {
static void check_payload(GObject *undo,const Babl *format,const std::vector<guchar>&expected,int width,int height)
{
 GeglBuffer *buffer=nullptr;int x,y;g_object_get(undo,"buffer",&buffer,"x",&x,"y",&y,nullptr);g_assert_nonnull(buffer);g_assert_true(gegl_buffer_get_format(buffer)==format);
 const auto *r=gegl_buffer_get_extent(buffer);const int bpp=babl_format_get_bytes_per_pixel(format);auto bytes=snapshot_bytes(buffer,*r);unsigned checked=0;
 for(int row=0;row<r->height;++row)for(int col=0;col<r->width;++col){int dx=x+col,dy=y+row;if(dx>=0&&dy>=0&&dx<width&&dy<height){g_assert_cmpmem(bytes.data()+(row*r->width+col)*bpp,bpp,expected.data()+(dy*width+dx)*bpp,bpp);++checked;}}
 g_assert_cmpuint(checked,>,0);g_object_unref(buffer);
}
static void native_snapshot_payload()
{
 unsigned cases=0;
 for(auto precision:{GIMP_PRECISION_U16_LINEAR,GIMP_PRECISION_U32_LINEAR,GIMP_PRECISION_HALF_LINEAR,GIMP_PRECISION_DOUBLE_LINEAR})
 for(auto base:{GIMP_RGB,GIMP_GRAY})for(bool alpha:{false,true})for(bool floating:{false,true}){
  const int width=257,height=129;const GeglRectangle extent={0,0,width,height};auto *image=gimp_image_new(gimp,width,height,base,precision);auto *format=gimp_image_get_layer_format(image,alpha);auto *layer=gimp_layer_new(image,width,height,format,"Undo payload precision",1,GIMP_LAYER_MODE_NORMAL);g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));auto *d=GIMP_DRAWABLE(layer);auto *target=gimp_drawable_get_buffer(d);
  const int n=babl_format_get_n_components(format),bpp=babl_format_get_bytes_per_pixel(format),size=bpp/n;std::vector<guchar> before(width*height*bpp);
  for(int pixel=0;pixel<width*height;++pixel)for(int c=0;c<n;++c){auto *out=before.data()+pixel*bpp+c*size;bool opacity=alpha&&c==n-1;unsigned increment=(pixel*7+c)%251;
   if(precision==GIMP_PRECISION_U16_LINEAR){std::uint16_t v=opacity?65535:0x8000+increment;std::memcpy(out,&v,2);}
   else if(precision==GIMP_PRECISION_U32_LINEAR){std::uint32_t v=opacity?UINT32_MAX:0x80000000u+increment;std::memcpy(out,&v,4);}
   else if(precision==GIMP_PRECISION_HALF_LINEAR){std::uint16_t v=opacity?0x3c00:0x3800+increment;std::memcpy(out,&v,2);}
   else{double v=opacity?1.:.5+increment*std::ldexp(1.,-53);std::memcpy(out,&v,8);}
  }
  gegl_buffer_set(target,&extent,0,format,before.data(),GEGL_AUTO_ROWSTRIDE);g_assert_true(snapshot_bytes(target,extent)==before);gimp_image_undo_free(image);auto *options=options_new();PaintCore core(options,settings(floating));core.begin_batch(true,"Native Undo payload");
  for(auto p:{std::pair<int,int>{1,1},{127,65},{255,127}}){GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=p.first;c.y=p.second;c.pressure=0;core.stroke_to(d,.01,c);c.pressure=.85;core.stroke_to(d,.1,c);core.next_segment();}
  core.end_batch(true);auto after=snapshot_bytes(target,extent);g_assert_true(after!=before);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
  std::vector<GObject*> entries;DirtyHandoff::collect_undo(gimp_undo_stack_peek(gimp_image_get_undo_stack(image)),&entries);g_assert_cmpuint(entries.size(),==,1);auto *entry=G_OBJECT(g_object_ref(entries[0]));
  check_payload(entry,format,before,width,height);
  for(int cycle=0;cycle<3;++cycle){g_assert_true(gimp_image_undo(image));g_assert_true(snapshot_bytes(target,extent)==before);check_payload(entry,format,after,width,height);g_assert_true(gimp_image_redo(image));g_assert_true(snapshot_bytes(target,extent)==after);check_payload(entry,format,before,width,height);}
  g_assert_true(gegl_buffer_get_format(target)==format);g_assert_true(gegl_rectangle_equal(gegl_buffer_get_extent(target),&extent));g_assert_false(core.active());g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));g_test_message("native Undo payload precision=%d base=%d alpha=%d floating=%d cycles=3",precision,base,alpha,floating);
  g_object_unref(entry);g_object_unref(options);g_object_unref(image);++cases;
 }
 g_assert_cmpuint(cases,==,32);
}
}
