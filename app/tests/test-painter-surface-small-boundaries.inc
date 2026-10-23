/* SPDX-License-Identifier: GPL-3.0-or-later */
namespace SmallBoundaries {
struct Bounds {int width,height;unsigned updates=0;};
static void updated(GimpDrawable*,gint x,gint y,gint width,gint height,gpointer data)
{
 auto *b=static_cast<Bounds*>(data);if(width<=0||height<=0)return;
 g_assert_cmpint(x,>=,0);g_assert_cmpint(y,>=,0);g_assert_cmpint(x+width,<=,b->width);g_assert_cmpint(y+height,<=,b->height);++b->updates;
}
static void native_small_edges()
{
 unsigned cases=0;
 for(auto size:{std::pair<int,int>{1,1},{2,3},{17,9}})
 for(auto precision:{GIMP_PRECISION_U8_NON_LINEAR,GIMP_PRECISION_DOUBLE_LINEAR})
 for(auto base:{GIMP_RGB,GIMP_GRAY})for(bool alpha:{false,true})for(int offset:{-5,0,4}){
  int width=size.first,height=size.second;auto *image=gimp_image_new(gimp,width,height,base,precision);auto *layer=gimp_layer_new(image,width,height,gimp_image_get_layer_format(image,alpha),"Small buffer boundary",1,GIMP_LAYER_MODE_NORMAL);g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));gimp_item_set_offset(GIMP_ITEM(layer),offset,-offset);
  auto *d=GIMP_DRAWABLE(layer);auto *buffer=gimp_drawable_get_buffer(d);const GeglRectangle extent={0,0,width,height};snapshot_fill(buffer,extent,"rgba(0.2,0.3,0.4,0.6)");const auto before=snapshot_bytes(buffer,extent);auto *format=gegl_buffer_get_format(buffer);
  gimp_image_undo_free(image);auto *options=options_new();auto *foreground=gegl_color_new("rgb(0.9,0.1,0.3)");gimp_context_set_foreground(GIMP_CONTEXT(options),foreground);g_object_unref(foreground);
  Bounds bounds{width,height};auto handler=g_signal_connect(d,"update",G_CALLBACK(updated),&bounds);
  Resource resource=settings(false);resource.set_base_value(BRUSH_RADIUS_LOGARITHMIC,.5);resource.set_base_value(BRUSH_STROKE_OPACITY,1);PaintCore core(options,resource);
  // PaintCore consumes drawable-local input (the native tool translates).
  // Include negative centers whose brush footprint still intersects the edge.
  core.begin_batch(true,"Small boundary fixture");
  for(auto point:{std::pair<double,double>{-.5,-.5},{.5,.5},{width-.5,.5},{.5,height-.5},{width-.5,height-.5}}){GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=point.first;c.y=point.second;c.pressure=0;core.stroke_to(d,.01,c);c.pressure=1;core.stroke_to(d,.1,c);}
  const auto painted=snapshot_bytes(buffer,extent);g_assert_true(painted!=before);g_assert_cmpuint(bounds.updates,>,0);core.end_batch(true);g_signal_handler_disconnect(d,handler);
  g_assert_true(gegl_rectangle_equal(gegl_buffer_get_extent(buffer),&extent));g_assert_true(gegl_buffer_get_format(buffer)==format);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
  g_assert_true(gimp_image_undo(image));g_assert_true(snapshot_bytes(buffer,extent)==before);g_assert_true(gimp_image_redo(image));g_assert_true(snapshot_bytes(buffer,extent)==painted);g_assert_false(core.active());g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
  g_test_message("tiny=%dx%d offset=(%d,%d) precision=%d base=%d alpha=%d",width,height,offset,-offset,precision,base,alpha);g_object_unref(options);g_object_unref(image);++cases;
 }
 g_assert_cmpuint(cases,==,72);
}
}
