/* SPDX-License-Identifier: GPL-3.0-or-later */
static std::vector<guchar> snapshot_bytes(GeglBuffer *buffer, const GeglRectangle& rect)
{ return GimpPainter::MyPaint::BufferRead::packed(buffer,rect); }
static void snapshot_fill(GeglBuffer *buffer,const GeglRectangle&rect,const char*name)
{
  auto *color=gegl_color_new(name);
  gegl_buffer_set_color(buffer,&rect,color);g_object_unref(color);
}
static void snapshot_native_start_handoff()
{
  for(auto precision:{GIMP_PRECISION_U8_NON_LINEAR,GIMP_PRECISION_FLOAT_LINEAR,GIMP_PRECISION_DOUBLE_NON_LINEAR})
  for(auto offset:{std::array<int,2>{0,0},{-7,5},{9,-6}}) {
    const GeglRectangle extent={0,0,512,256};
    auto*image=gimp_image_new(gimp,extent.width,extent.height,GIMP_RGB,precision);
    auto*layer=gimp_layer_new(image,extent.width,extent.height,gimp_image_get_layer_format(image,TRUE),"Snapshot target",1,GIMP_LAYER_MODE_NORMAL);
    g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));
    gimp_item_set_offset(GIMP_ITEM(layer),offset[0],offset[1]);
    auto*d=GIMP_DRAWABLE(layer);auto*target=gimp_drawable_get_buffer(d);
    snapshot_fill(target,extent,"blue");const auto initial=snapshot_bytes(target,extent);
    auto*options=options_new();auto*core=GIMP_PAINT_CORE(g_object_new(GIMP_TYPE_PAINT_CORE,nullptr));
    GList drawables={d,nullptr,nullptr};GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=40;coords.y=40;GError*error=nullptr;
    g_assert_true(gimp_paint_core_start(core,&drawables,options,&coords,&error));g_assert_no_error(error);
    auto*snapshot=GEGL_BUFFER(g_hash_table_lookup(core->undo_buffers,d));
    g_assert_nonnull(snapshot);g_assert_true(snapshot!=target);
    g_assert_true(gegl_rectangle_equal(gegl_buffer_get_extent(snapshot),&extent));
    g_assert_true(gegl_buffer_get_format(snapshot)==gegl_buffer_get_format(target));
    g_assert_true(snapshot_bytes(snapshot,extent)==initial);
    auto*control=gegl_buffer_dup(target);
    // Mutation occurs after native start, before even Surface's first touch.
    snapshot_fill(target,extent,"lime");
    g_assert_true(snapshot_bytes(snapshot,extent)==initial);
    {
      GeglSurface actual(target),reference(control);
      for(auto*surface:{&actual,&reference}) {
        surface->set_non_incremental(true);
        surface->set_shape_provider([](float,float,float,float){return ShapeMask{3,3,std::vector<guchar>(9,255)};});
      }
      actual.begin_session_from(snapshot);reference.begin_session();
      for(int x:{40,400}) {
        if(x==400) {
          const GeglRectangle later={384,0,128,128};
          snapshot_fill(target,later,"yellow");
          g_assert_true(snapshot_bytes(snapshot,extent)==initial);
        }
        for(int dab=0;dab<2;++dab) {
          g_assert_true(actual.draw_dab(x,40,2,1,0,0,.5f,1));
          g_assert_true(reference.draw_dab(x,40,2,1,0,0,.5f,1));
          const GeglRectangle probe={x-2,38,3,3};
          g_assert_true(snapshot_bytes(target,probe)==snapshot_bytes(control,probe));
          g_assert_true(snapshot_bytes(snapshot,extent)==initial);
        }
      }
      const auto dirty=actual.dirty();auto*expected=gegl_buffer_dup(target);
      gegl_buffer_copy(snapshot,&dirty,GEGL_ABYSS_NONE,expected,&dirty);
      actual.cancel_session();reference.end_session();
      g_assert_true(snapshot_bytes(target,extent)==snapshot_bytes(expected,extent));
      g_assert_true(snapshot_bytes(target,extent)!=initial); // outside edits survive
      g_assert_true(snapshot_bytes(snapshot,extent)==initial);g_object_unref(expected);
    }
    gimp_paint_core_finish(core,&drawables,FALSE);gimp_paint_core_cleanup(core);g_object_unref(core);
    g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
    g_object_unref(control);g_object_unref(options);g_object_unref(image);
  }
}
