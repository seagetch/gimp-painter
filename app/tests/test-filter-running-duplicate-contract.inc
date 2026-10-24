/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual RUNNING scheduler state, never loader-supplied diagnostic metadata. */
static void
filter_gaussian_running_duplicate_contract (void)
{
  GimpImage *image=image_new(512,512);
  GimpLayer *groups[2]={gimp_group_layer_new(image),gimp_group_layer_new(image)};
  GimpLayer *sources[2];
  GimpFilterLayer *original,*copy;
  GimpValueArray *args,*observed;
  GBytes *raw=g_bytes_new_static("Gaussian original definition",28),*copied_raw;
  gint64 deadline;
  guint64 copy_generation,copy_runs;
  for(guint i=0;i<2;i++)
    {
      g_assert_true(gimp_image_add_layer(image,groups[i],NULL,0,FALSE));
      sources[i]=source_new(image,groups[i],512,512);
      fill(sources[i],i?0:255,0,i?255:0,255);
    }
  original=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,512,512,"active Gaussian",1.0,GIMP_LAYER_MODE_NORMAL_LEGACY));
  args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,
           G_TYPE_DOUBLE,20.0,G_TYPE_DOUBLE,20.0,G_TYPE_INT,1,G_TYPE_NONE);
  g_assert_true(gimp_filter_layer_set_definition(original,"plug-in-gauss",raw,args,NULL));
  g_assert_true(gimp_image_add_layer(image,GIMP_LAYER(original),groups[0],0,FALSE));
  deadline=g_get_monotonic_time()+20*G_TIME_SPAN_SECOND;
  while(gimp_filter_layer_get_state(original)!=GIMP_FILTER_LAYER_RUNNING && g_get_monotonic_time()<deadline)
    {g_assert_cmpint(gimp_filter_layer_get_state(original),!=,GIMP_FILTER_LAYER_FAILED);g_main_context_iteration(NULL,FALSE);g_usleep(100);}
  g_assert_cmpint(gimp_filter_layer_get_state(original),==,GIMP_FILTER_LAYER_RUNNING);
  g_assert_cmpuint(gimp_filter_layer_get_run_count(original),==,1);
  copy=GIMP_FILTER_LAYER(gimp_item_duplicate(GIMP_ITEM(original),GIMP_TYPE_FILTER_LAYER));
  g_assert_nonnull(copy);
  g_assert_cmpuint(gimp_filter_layer_get_run_count(copy),==,0);
  g_assert_cmpint(gimp_filter_layer_get_state(copy),!=,GIMP_FILTER_LAYER_RUNNING);
  g_assert_cmpint(gimp_filter_layer_get_state(original),==,GIMP_FILTER_LAYER_RUNNING);
  g_assert_true(gimp_filter_layer_cancel_generation(original,gimp_filter_layer_get_generation(original)));
  g_assert_true(gimp_image_add_layer(image,GIMP_LAYER(copy),groups[1],0,FALSE));
  settle(copy);pixel(GIMP_LAYER(copy),0,0,255,255);
  g_assert_cmpuint(gimp_filter_layer_get_run_count(copy),==,1);
  copied_raw=gimp_filter_layer_ref_definition(copy);g_assert_true(g_bytes_equal(raw,copied_raw));g_bytes_unref(copied_raw);
  observed=gimp_filter_layer_dup_args(copy);g_assert_cmpfloat(g_value_get_double(gimp_value_array_index(observed,3)),==,20.0);gimp_value_array_unref(observed);
  copy_generation=gimp_filter_layer_get_generation(copy);copy_runs=gimp_filter_layer_get_run_count(copy);
  /* Installing a new immutable model on the original cannot rewrite the
   * duplicate's definition or attach it to the original group's red source. */
  g_value_set_double(gimp_value_array_index(args,3),5.0);
  g_value_set_double(gimp_value_array_index(args,4),5.0);
  g_assert_true(gimp_filter_layer_set_definition(original,"plug-in-gauss",NULL,args,NULL));
  settle(original);pixel(GIMP_LAYER(original),255,0,0,255);
  g_assert_cmpuint(gimp_filter_layer_get_generation(copy),==,copy_generation);
  g_assert_cmpuint(gimp_filter_layer_get_run_count(copy),==,copy_runs);
  observed=gimp_filter_layer_dup_args(copy);g_assert_cmpfloat(g_value_get_double(gimp_value_array_index(observed,3)),==,20.0);gimp_value_array_unref(observed);
  copied_raw=gimp_filter_layer_ref_definition(copy);g_assert_true(g_bytes_equal(raw,copied_raw));g_bytes_unref(copied_raw);
  fill(sources[1],0,255,0,255);settle(copy);pixel(GIMP_LAYER(copy),0,255,0,255);
  pixel(GIMP_LAYER(original),255,0,0,255);
  g_assert_cmpuint(gimp_filter_layer_get_run_count(copy),==,copy_runs+1);
  gimp_value_array_unref(args);g_bytes_unref(raw);g_object_unref(image);
  g_test_message("real RUNNING Gaussian duplicated with fresh zero-run scheduler;original cancellation isolated;independent red/blue-green groups,immutable definition/raw bytes and4 exact pixels");
}
