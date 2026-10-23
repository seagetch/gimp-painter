/* SPDX-License-Identifier: GPL-3.0-or-later */
namespace CommitCancel {
static void segment(PaintCore&core,GimpDrawable*d,int x,int y)
{
 GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=x;c.y=y;c.pressure=0;
 core.stroke_to(d,.01,c);c.pressure=.85;core.stroke_to(d,.075,c);
}
static void native_pixels_and_history()
{
 for(auto precision:{GIMP_PRECISION_U8_NON_LINEAR,GIMP_PRECISION_FLOAT_LINEAR,GIMP_PRECISION_DOUBLE_NON_LINEAR})
 for(bool floating:{false,true})for(int exit=0;exit<8;++exit){
  g_test_message("precision=%d floating=%d exit=%d",int(precision),floating,exit);
  const GeglRectangle extent={0,0,512,512};auto*image=gimp_image_new(gimp,512,512,GIMP_RGB,precision);
  auto*layer=gimp_layer_new(image,512,512,gimp_image_get_layer_format(image,TRUE),"Commit cancel target",1,GIMP_LAYER_MODE_NORMAL);
  g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));auto*d=GIMP_DRAWABLE(layer);auto*target=gimp_drawable_get_buffer(d);
  snapshot_fill(target,extent,"blue");gimp_image_undo_free(image);auto*options=options_new();
  // Establish both a retained Undo item and a redo branch. Cancellation must
  // neither clear the redo branch nor accidentally publish a partial operation.
  {PaintCore seed(options,settings(floating));UndoHandoff::operation(seed,d,40,40);UndoHandoff::operation(seed,d,400,400);}
  const auto redo_pixels=snapshot_bytes(target,extent);g_assert_true(gimp_image_undo(image));const auto before=snapshot_bytes(target,extent);
  g_assert_true(before!=redo_pixels);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
  g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_redo_stack(image)),==,1);
  auto core=std::unique_ptr<PaintCore>(new PaintCore(options,settings(floating)));core->begin_batch(true,"Commit cancel fixture");
  if(exit!=7){segment(*core,d,220,80);core->next_segment();segment(*core,d,80,300);g_assert_true(core->active());g_assert_true(snapshot_bytes(target,extent)!=before);}
  const auto painted=snapshot_bytes(target,extent);
  switch(exit){
   case 0:core->end_batch(true);break;
   case 1:core->end_batch(false);break;
   case 2:core->cancel();core->cancel();core->end_batch(false);break;
   case 3:core->finish();{bool rejected=false;try{core->end_batch(true);}catch(const std::runtime_error&){rejected=true;}g_assert_true(rejected);}break;
   case 4:{bool rejected=false;try{core->configure(settings(floating));}catch(const std::logic_error&){rejected=true;}g_assert_true(rejected);core->end_batch(false);}break;
   case 5:{GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=std::numeric_limits<double>::quiet_NaN();bool rejected=false;try{core->stroke_to(d,.01,c);}catch(const std::invalid_argument&){rejected=true;}g_assert_true(rejected);core->end_batch(false);}break;
   case 6:core.reset();break;
   case 7:core->end_batch(true);break;
  }
  g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
  if(core){g_assert_false(core->active());core->finish();core->cancel();}
  if(exit==0){
   g_assert_true(snapshot_bytes(target,extent)==painted);
   g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,2);
   g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_redo_stack(image)),==,0);
   g_assert_true(gimp_image_undo(image));g_assert_true(snapshot_bytes(target,extent)==before);
   g_assert_true(gimp_image_redo(image));g_assert_true(snapshot_bytes(target,extent)==painted);
  }else{
   g_assert_true(snapshot_bytes(target,extent)==before);
   g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
   g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_redo_stack(image)),==,1);
   g_assert_true(gimp_image_redo(image));g_assert_true(snapshot_bytes(target,extent)==redo_pixels);
   g_assert_true(gimp_image_undo(image));g_assert_true(snapshot_bytes(target,extent)==before);
   if(!core)core.reset(new PaintCore(options,settings(floating)));
   UndoHandoff::operation(*core,d,200,200);const auto retry=snapshot_bytes(target,extent);g_assert_true(retry!=before);
   g_assert_true(gimp_image_undo(image));g_assert_true(snapshot_bytes(target,extent)==before);
   g_assert_true(gimp_image_redo(image));g_assert_true(snapshot_bytes(target,extent)==retry);
  }
  core.reset();g_object_unref(options);g_object_unref(image);
 }
}
}
