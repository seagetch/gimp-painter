/* SPDX-License-Identifier: GPL-3.0-or-later */
namespace DirtyHandoff {
struct Updates {std::vector<GeglRectangle> rects;};
static void updated(GimpDrawable*,int x,int y,int width,int height,gpointer data)
{static_cast<Updates*>(data)->rects.push_back({x,y,width,height});}
static void collect_undo(gpointer object,gpointer data)
{
 auto*rows=static_cast<std::vector<GObject*>*>(data);
 if(GIMP_IS_UNDO_STACK(object))gimp_container_foreach(GIMP_UNDO_STACK(object)->undos,collect_undo,data);
 else if(g_type_is_a(G_OBJECT_TYPE(object),g_type_from_name("GimpDrawableUndo")))rows->push_back(G_OBJECT(object));
}
static void native_dirty_undo_extent()
{
 for(auto precision:{GIMP_PRECISION_U8_NON_LINEAR,GIMP_PRECISION_FLOAT_LINEAR,GIMP_PRECISION_DOUBLE_NON_LINEAR})
 for(bool floating:{false,true})for(auto offset:{std::array<int,2>{0,0},{-7,5},{9,-6}}){
  const GeglRectangle extent={0,0,512,256};
  auto*image=gimp_image_new(gimp,512,256,GIMP_RGB,precision);
  auto*layer=gimp_layer_new(image,512,256,gimp_image_get_layer_format(image,TRUE),"Dirty target",1,GIMP_LAYER_MODE_NORMAL);
  g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));gimp_item_set_offset(GIMP_ITEM(layer),offset[0],offset[1]);
  auto*d=GIMP_DRAWABLE(layer);auto*target=gimp_drawable_get_buffer(d);snapshot_fill(target,extent,"blue");
  gimp_channel_select_rectangle(gimp_image_get_mask(image),32+offset[0],16+offset[1],440,160,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  gimp_image_undo_free(image);const auto before=snapshot_bytes(target,extent);auto*options=options_new();Updates updates;
  const auto handler=g_signal_connect(d,"update",G_CALLBACK(updated),&updates);
  {
   PaintCore core(options,settings(floating));core.begin_batch(true,"Dirty aggregation fixture");
   const std::array<int,2> centers[]={{34,40},{36,41},{400,100}};
   for(unsigned n=0;n<3;++n){
    if(n)core.next_segment();
    GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=centers[n][0];c.y=centers[n][1];c.pressure=0;core.stroke_to(d,.01,c);c.pressure=.85;core.stroke_to(d,.075,c);
   }
   g_assert_false(updates.rects.empty());const auto count=updates.rects.size();const auto painted=snapshot_bytes(target,extent);g_assert_true(painted!=before);
   // An entirely outside segment must neither extend damage nor erase the
   // accumulated native core box from the three preceding logical segments.
   core.next_segment();GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=10000;c.y=10000;c.pressure=0;core.stroke_to(d,.01,c);c.pressure=.85;core.stroke_to(d,.075,c);
   g_assert_cmpuint(updates.rects.size(),==,count);g_assert_true(snapshot_bytes(target,extent)==painted);
   // Conservative damage clips to mask storage extent, not its nonzero
   // content box. Only changed pixels must remain within selected coverage.
   int left=512,top=256,right=0,bottom=0;
   for(const auto&r:updates.rects){g_assert_cmpint(r.width,>,0);g_assert_cmpint(r.height,>,0);g_assert_cmpint(r.x,>=,std::max(0,-offset[0]));g_assert_cmpint(r.y,>=,std::max(0,-offset[1]));g_assert_cmpint(r.x+r.width,<=,std::min(512,512-offset[0]));g_assert_cmpint(r.y+r.height,<=,std::min(256,256-offset[1]));left=std::min(left,r.x);top=std::min(top,r.y);right=std::max(right,r.x+r.width);bottom=std::max(bottom,r.y+r.height);}
   g_assert_cmpint(left,<,40);g_assert_cmpint(right,>,395);
   const int bpp=babl_format_get_bytes_per_pixel(gegl_buffer_get_format(target));
   for(int y=0;y<256;++y)for(int x=0;x<512;++x){const auto i=std::size_t(y*512+x)*bpp;if(std::memcmp(before.data()+i,painted.data()+i,bpp)){g_assert_cmpint(x,>=,32);g_assert_cmpint(x,<,472);g_assert_cmpint(y,>=,16);g_assert_cmpint(y,<,176);bool covered=false;for(const auto&r:updates.rects)covered=covered||(x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height);g_assert_true(covered);}}
   GeglRectangle expected={left,top,right-left,bottom-top};gegl_rectangle_align_to_buffer(&expected,&expected,target,GEGL_RECTANGLE_ALIGNMENT_SUPERSET);
   core.end_batch(true);g_assert_false(core.active());g_assert_true(snapshot_bytes(target,extent)==painted);
   std::vector<GObject*>undos;collect_undo(gimp_undo_stack_peek(gimp_image_get_undo_stack(image)),&undos);g_assert_cmpuint(undos.size(),==,1);
   GeglBuffer*saved=nullptr;int x,y;g_object_get(undos[0],"buffer",&saved,"x",&x,"y",&y,nullptr);g_assert_nonnull(saved);
   const auto*saved_extent=gegl_buffer_get_extent(saved);g_assert_cmpint(x,==,expected.x);g_assert_cmpint(y,==,expected.y);g_assert_cmpint(saved_extent->width,==,expected.width);g_assert_cmpint(saved_extent->height,==,expected.height);g_object_unref(saved);
   g_assert_true(gimp_image_undo(image));g_assert_true(snapshot_bytes(target,extent)==before);g_assert_true(gimp_image_redo(image));g_assert_true(snapshot_bytes(target,extent)==painted);
  }
  g_signal_handler_disconnect(d,handler);g_object_unref(options);g_object_unref(image);
 }
}
}
