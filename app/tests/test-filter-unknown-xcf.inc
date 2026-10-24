/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Original15.013: file Open/resave of an unavailable procedure, no executor. */
static void
filter_unknown_xcf_preservation (void)
{
  static const guint8 raw_data[]={0,0xff,0x80,'R','A','W',0,7};
  static const guint8 color[]={31,47,93,255};
  guint rounds=0,failures=0;
  for(guint opaque_case=0;opaque_case<2;opaque_case++)
    {
      GimpImage *image=gimp_image_new(gimp,4,4,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      GimpFilterLayer *filter=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,4,4,"unavailable preserved",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
      GFile *file=temporary_file();GBytes *raw=g_bytes_new_static(raw_data,sizeof(raw_data)),*opaque=NULL;
      GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,-17,G_TYPE_DOUBLE,3.25,G_TYPE_STRING,"saved unknown",G_TYPE_NONE);
      g_assert_true(gimp_image_add_layer(image,GIMP_LAYER(filter),NULL,0,FALSE));
      if(opaque_case)
        {
          GVariantBuilder b;GVariant *wrapper;
          g_variant_builder_init(&b,G_VARIANT_TYPE("a(sbv)"));
          g_variant_builder_add(&b,"(sbv)","future-unrecognized-value",FALSE,g_variant_new_int32(71));
          wrapper=g_variant_ref_sink(g_variant_new_maybe(G_VARIANT_TYPE_VARIANT,g_variant_new_variant(g_variant_builder_end(&b))));
#if G_BYTE_ORDER == G_BIG_ENDIAN
          {GVariant *little=g_variant_byteswap(wrapper);g_variant_unref(wrapper);wrapper=little;}
#endif
          opaque=g_variant_get_data_as_bytes(wrapper);g_variant_unref(wrapper);
          g_assert_true(gimp_filter_layer_set_definition_with_opaque_arguments(filter,"future-unavailable-preserved-filter",raw,opaque,NULL));
        }
      else g_assert_true(gimp_filter_layer_set_definition(filter,"future-unavailable-preserved-filter",raw,args,NULL));
      {guint8 pixels[4*4*4];for(guint i=0;i<16;i++)memcpy(pixels+i*4,color,4);
       gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),GEGL_RECTANGLE(0,0,4,4),0,babl_format("R'G'B'A u8"),pixels,GEGL_AUTO_ROWSTRIDE);}
      gimp_filter_layer_mark_as_loaded(filter);
      for(guint pass=0;pass<3;pass++)
        {
          GimpImage *copy;GBytes *saved;gchar *name;
          save(image,file);copy=load(file);g_object_unref(image);image=copy;
          filter=GIMP_FILTER_LAYER(layer(image,"unavailable preserved"));g_assert_true(GIMP_IS_FILTER_LAYER(filter));
          name=gimp_filter_layer_dup_procedure(filter);g_assert_cmpstr(name,==,"future-unavailable-preserved-filter");g_free(name);
          saved=gimp_filter_layer_ref_definition(filter);g_assert_true(g_bytes_equal(saved,raw));g_bytes_unref(saved);
          if(opaque_case)
            {saved=gimp_filter_layer_ref_opaque_arguments(filter);g_assert_nonnull(saved);g_assert_true(g_bytes_equal(saved,opaque));g_bytes_unref(saved);g_assert_null(gimp_filter_layer_dup_args(filter));}
          else
            {GimpValueArray *values=gimp_filter_layer_dup_args(filter);g_assert_nonnull(values);g_assert_cmpuint(gimp_value_array_length(values),==,3);
             g_assert_cmpint(g_value_get_int(gimp_value_array_index(values,0)),==,-17);g_assert_cmpfloat(g_value_get_double(gimp_value_array_index(values,1)),==,3.25);
             g_assert_cmpstr(g_value_get_string(gimp_value_array_index(values,2)),==,"saved unknown");gimp_value_array_unref(values);}
          pixel(GIMP_LAYER(filter),31,47,93,255);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
          gimp_filter_layer_invalidate(filter);
          {gint64 end=g_get_monotonic_time()+3000000;while(gimp_filter_layer_get_state(filter)!=GIMP_FILTER_LAYER_FAILED&&g_get_monotonic_time()<end)
            {while(g_main_context_iteration(NULL,FALSE));g_usleep(1000);}}
          g_assert_cmpint(gimp_filter_layer_get_state(filter),==,GIMP_FILTER_LAYER_FAILED);
          name=gimp_filter_layer_dup_error(filter);g_assert_nonnull(name);g_assert_true(*name!='\0');g_free(name);
          g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);pixel(GIMP_LAYER(filter),31,47,93,255);
          name=gimp_filter_layer_dup_procedure(filter);g_assert_cmpstr(name,==,"future-unavailable-preserved-filter");g_free(name);
          saved=gimp_filter_layer_ref_definition(filter);g_assert_true(g_bytes_equal(saved,raw));g_bytes_unref(saved);
          if(opaque_case){saved=gimp_filter_layer_ref_opaque_arguments(filter);g_assert_true(g_bytes_equal(saved,opaque));g_bytes_unref(saved);}
          rounds++;failures++;
          g_test_message("unknown %s pass%u:Open/resave exact name/raw/arguments/cache;explicit FAILED without executor",opaque_case?"opaque":"typed",pass+1);
        }
      g_object_unref(image);g_bytes_unref(raw);g_clear_pointer(&opaque,g_bytes_unref);gimp_value_array_unref(args);g_file_delete(file,NULL,NULL);g_object_unref(file);
    }
  g_assert_cmpuint(rounds,==,6);g_assert_cmpuint(failures,==,6);
  g_test_message("6 unknown-only public XCF Save/Open rounds;6 explicit failures;zero executor runs;typed and opaque bytes retained");
}
