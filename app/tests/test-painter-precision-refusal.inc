/* SPDX-License-Identifier: GPL-3.0-or-later */
static GimpColorProfile *refusal_icc(bool cmyk,GError **error)
{
 auto profile=cmyk?cmsCreateInkLimitingDeviceLink(cmsSigCmykData,300):cmsCreateLab4Profile(nullptr);g_assert_nonnull(profile);
 cmsUInt32Number length=0;g_assert_true(cmsSaveProfileToMem(profile,nullptr,&length));std::vector<guint8> data(length);g_assert_true(cmsSaveProfileToMem(profile,data.data(),&length));cmsCloseProfile(profile);
 return gimp_color_profile_new_from_icc_profile(data.data(),length,error);
}
static void native_profile_refusal()
{
 unsigned cases=0;
 for(auto base:{GIMP_RGB,GIMP_GRAY}){
  auto *image=gimp_image_new(gimp,32,24,base,GIMP_PRECISION_DOUBLE_LINEAR);
  auto *accepted=base==GIMP_RGB?gimp_color_profile_new_rgb_adobe():gimp_color_profile_new_d50_gray_lab_trc();GError *error=nullptr;
  g_assert_true(gimp_image_set_color_profile(image,accepted,&error));g_assert_no_error(error);
  auto *layer=gimp_layer_new(image,32,24,gimp_image_get_layer_format(image,TRUE),"Explicit ICC refusal",1,GIMP_LAYER_MODE_NORMAL);g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));auto *d=GIMP_DRAWABLE(layer);auto *buffer=gimp_drawable_get_buffer(d);auto *format=gegl_buffer_get_format(buffer);
  auto *fill=gegl_color_new("rgba(0.25,0.5,0.75,0.6)");gegl_buffer_set_color(buffer,nullptr,fill);g_object_unref(fill);auto before=pixels(d);
  gimp_image_undo_free(image);gimp_channel_select_rectangle(gimp_image_get_mask(image),2,3,8,7,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,TRUE);auto *undo=gimp_image_get_undo_stack(image);auto *top=gimp_undo_stack_peek(undo);int depth=gimp_undo_stack_get_depth(undo);g_assert_cmpint(depth,>,0);
  for(int kind=0;kind<3;++kind)for(int retry=0;retry<2;++retry){
   auto *rejected=kind==0?(base==GIMP_RGB?gimp_color_profile_new_d50_gray_lab_trc():gimp_color_profile_new_rgb_adobe()):refusal_icc(kind==2,&error);
   const char *stage="profile construction";
   if(rejected){g_assert_no_error(error);stage="native profile assignment";g_assert_false(gimp_image_assign_color_profile(image,rejected,nullptr,&error));g_object_unref(rejected);}
   g_assert_nonnull(error);g_assert_nonnull(error->message);g_assert_cmpuint(std::strlen(error->message),>,0);
   g_test_message("base=%d unsupported-kind=%d retry=%d rejected at %s: %s",base,kind,retry,stage,error->message);g_clear_error(&error);
   g_assert_true(pixels(d)==before);g_assert_true(gimp_drawable_get_buffer(d)==buffer);g_assert_true(gegl_buffer_get_format(buffer)==format);g_assert_true(gimp_color_profile_is_equal(gimp_image_get_color_profile(image),accepted));g_assert_cmpint(gimp_image_get_precision(image),==,GIMP_PRECISION_DOUBLE_LINEAR);
   g_assert_cmpint(gimp_undo_stack_get_depth(undo),==,depth);g_assert_true(gimp_undo_stack_peek(undo)==top);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_redo_stack(image)),==,0);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));++cases;
  }
  // Native default brush may end/commit non-batch segments when it exits the
  // selected area. An explicit atomic batch is the whole-operation rollback API.
  auto *options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));Resource resource;PaintCore core(options,resource);
  core.begin_batch(true,"ICC refusal recovery");stroke(core,d);g_assert_true(pixels(d)!=before);g_assert_cmpint(gimp_undo_stack_get_depth(undo),==,depth);core.end_batch(false);
  g_assert_true(pixels(d)==before);g_assert_false(core.active());g_assert_cmpint(gimp_undo_stack_get_depth(undo),==,depth);g_assert_true(gimp_undo_stack_peek(undo)==top);
  g_object_unref(options);g_object_unref(accepted);g_object_unref(image);
 }
 g_assert_cmpuint(cases,==,12);
}
