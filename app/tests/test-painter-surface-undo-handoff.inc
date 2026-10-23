/* SPDX-License-Identifier: GPL-3.0-or-later */
namespace UndoHandoff {
static void operation(PaintCore&core,GimpDrawable*d,int x,int y,bool undo=true)
{
 core.begin_batch(undo,"Buffer difference fixture");
 GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=x;c.y=y;c.pressure=0;core.stroke_to(d,.01,c);c.pressure=.85;core.stroke_to(d,.075,c);core.end_batch(true);
}
static void native_operation_history()
{
 for(auto precision:{GIMP_PRECISION_U8_NON_LINEAR,GIMP_PRECISION_FLOAT_LINEAR,GIMP_PRECISION_DOUBLE_NON_LINEAR})for(bool floating:{false,true}){
  const GeglRectangle extent={0,0,512,512};auto*image=gimp_image_new(gimp,512,512,GIMP_RGB,precision);
  auto*layer=gimp_layer_new(image,512,512,gimp_image_get_layer_format(image,TRUE),"Undo target",1,GIMP_LAYER_MODE_NORMAL);g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));
  auto*d=GIMP_DRAWABLE(layer);auto*target=gimp_drawable_get_buffer(d);snapshot_fill(target,extent,"blue");gimp_image_undo_free(image);const auto initial=snapshot_bytes(target,extent);auto*options=options_new();
  {
   PaintCore core(options,settings(floating));operation(core,d,40,40);const auto first=snapshot_bytes(target,extent);g_assert_true(first!=initial);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
   operation(core,d,300,300);const auto second=snapshot_bytes(target,extent);g_assert_true(second!=first);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,2);
   g_assert_true(gimp_image_undo(image));g_assert_true(snapshot_bytes(target,extent)==first);g_assert_true(gimp_image_undo(image));g_assert_true(snapshot_bytes(target,extent)==initial);
   g_assert_true(gimp_image_redo(image));g_assert_true(snapshot_bytes(target,extent)==first);g_assert_true(gimp_image_redo(image));g_assert_true(snapshot_bytes(target,extent)==second);
   // A new operation after Undo replaces only the redo branch. The earlier
   // operation remains separately restorable, rather than coalescing history.
   g_assert_true(gimp_image_undo(image));g_assert_true(snapshot_bytes(target,extent)==first);operation(core,d,420,100);const auto branch=snapshot_bytes(target,extent);g_assert_true(branch!=first);g_assert_true(branch!=second);
   g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,2);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_redo_stack(image)),==,0);
   g_assert_true(gimp_image_undo(image));g_assert_true(snapshot_bytes(target,extent)==first);g_assert_true(gimp_image_redo(image));g_assert_true(snapshot_bytes(target,extent)==branch);
   const int depth=gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image));operation(core,d,10000,10000);g_assert_true(snapshot_bytes(target,extent)==branch);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,depth);
   operation(core,d,420,420,false);g_assert_true(snapshot_bytes(target,extent)!=branch);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,depth);
   g_assert_false(core.active());g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
  }
  g_object_unref(options);g_object_unref(image);
 }
}
}
