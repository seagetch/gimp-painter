/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <math.h>
/* Explicit new typed tint; not recovery of lost old boxed GimpRGB data. */
static void
xcf_colorify_define (GimpFilterLayer *filter,GBytes *raw,GVariant *tint)
{
  GimpValueArray *args=gimp_value_array_new_from_types(NULL,
    G_TYPE_INT,-123,G_TYPE_INT,0,G_TYPE_INT,0,G_TYPE_VARIANT,tint,G_TYPE_NONE);
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-colorify",raw,args,NULL));
  gimp_value_array_unref(args);
}
static void
xcf_colorify_model (GimpFilterLayer *filter,GBytes *raw,GVariant *tint)
{
  gchar *name=gimp_filter_layer_dup_procedure(filter);
  GimpValueArray *args=gimp_filter_layer_dup_args(filter);
  g_assert_cmpstr(name,==,"plug-in-colorify");g_free(name);
  g_assert_nonnull(args);g_assert_cmpuint(gimp_value_array_length(args),==,4);
  for(guint i=0;i<3;i++)g_assert_true(G_VALUE_HOLDS_INT(gimp_value_array_index(args,i)));
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,0)),==,-123);
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,1)),==,0);
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,2)),==,0);
  g_assert_true(G_VALUE_HOLDS_VARIANT(gimp_value_array_index(args,3)));
  GVariant *actual=g_value_get_variant(gimp_value_array_index(args,3));
  if(tint){
    g_assert_nonnull(actual);g_assert_cmpstr(g_variant_get_type_string(actual),==,g_variant_get_type_string(tint));
    g_assert_cmpmem(g_variant_get_data(actual),g_variant_get_size(actual),g_variant_get_data(tint),g_variant_get_size(tint));
  }else g_assert_null(actual);
  gimp_value_array_unref(args);xcf_max_rgb_raw_equal(filter,raw);
}
static GBytes *
xcf_colorify_reference (GBytes *source,const guint halves[3])
{
  gsize size;const guint8 *in=g_bytes_get_data(source,&size);guint8 *out=g_memdup2(in,size);
  for(gsize p=0;p<size;p+=4){guint l=in[p]*2126/10000+in[p+1]*7152/10000+in[p+2]*722/10000;
    for(guint c=0;c<3;c++)out[p+c]=l*halves[c]/2;}
  return g_bytes_new_take(out,size);
}
static void
colorify_typed_xcf_roundtrips (void)
{
  const guint halves[][3]={{2,2,2},{0,1,2},{2,0,1},{1,2,0}};
  const gdouble alpha[]={1,-0.0,0.5,0};const guint8 sentinel[]={13,29,47,255};
  GBytes *raw=xcf_max_rgb_capsule(1,1),*cache=xcf_max_rgb_constant_pixels(17,13,4,sentinel);
  GBytes *source=xcf_max_rgb_source(0),*changed=xcf_max_rgb_source(1);
  for(guint scene=0;scene<G_N_ELEMENTS(halves);scene++){
    GVariant *tint=g_variant_ref_sink(g_variant_new("(sudddd)","gimp-painter-colorify-rgba",1u,
      scene==1?-0.0:halves[scene][0]/2.0,halves[scene][1]/2.0,halves[scene][2]/2.0,alpha[scene]));
    GBytes *first=xcf_colorify_reference(source,halves[scene]),*second=xcf_colorify_reference(changed,halves[scene]);
    GimpImage *image=xcf_max_rgb_open_synthetic(raw,cache,source);GimpFilterLayer *filter=xcf_max_rgb_filter(image);
    xcf_colorify_define(filter,raw,tint);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_CLEAN);
    xcf_colorify_model(filter,raw,tint);xcf_max_rgb_pixels(GIMP_LAYER(filter),first,4);
    image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
    xcf_max_rgb_loaded(filter);xcf_colorify_model(filter,raw,tint);xcf_max_rgb_pixels(GIMP_LAYER(filter),first,4);
    xcf_max_rgb_lower_input(image,4,changed);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_CLEAN);
    xcf_max_rgb_pixels(GIMP_LAYER(filter),second,4);
    image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
    xcf_max_rgb_loaded(filter);xcf_colorify_model(filter,raw,tint);xcf_max_rgb_pixels(GIMP_LAYER(filter),second,4);
    g_test_message("Colorify typed scene%u: two Save/Open rounds, exact tuple bytes including signed zero, independent integer-luma pixels",scene);
    g_object_unref(image);g_variant_unref(tint);g_bytes_unref(first);g_bytes_unref(second);
  }
  g_bytes_unref(raw);g_bytes_unref(cache);g_bytes_unref(source);g_bytes_unref(changed);
}
static void
colorify_unsupported_typed_xcf_roundtrips (void)
{
  GVariant *variants[]={
    g_variant_new("(sudddd)","unknown-colorify-tint",1u,1.,1.,1.,1.),
    g_variant_new("(sudddd)","gimp-painter-colorify-rgba",2u,1.,1.,1.,1.),
    g_variant_new("(suddd)","gimp-painter-colorify-rgba",1u,1.,1.,1.),
    g_variant_new("(sudddd)","gimp-painter-colorify-rgba",1u,-0.5,1.,1.,1.),
    g_variant_new("(sudddd)","gimp-painter-colorify-rgba",1u,1.,1.,1.,1.5),
    g_variant_new("(sudddd)","gimp-painter-colorify-rgba",1u,NAN,1.,1.,1.),
    g_variant_new("(sudddd)","gimp-painter-colorify-rgba",1u,1.,INFINITY,1.,1.),NULL};
  const guint8 sentinel[]={13,29,47,255};GBytes *raw=xcf_max_rgb_capsule(1,1);
  GBytes *cache=xcf_max_rgb_constant_pixels(17,13,4,sentinel),*source=xcf_max_rgb_source(0),*changed=xcf_max_rgb_source(1);
  for(guint scene=0;scene<G_N_ELEMENTS(variants);scene++){
    GVariant *tint=variants[scene]?g_variant_ref_sink(variants[scene]):NULL;
    GimpImage *image=xcf_max_rgb_open_synthetic(raw,cache,source);GimpFilterLayer *filter=xcf_max_rgb_filter(image);
    xcf_colorify_define(filter,raw,tint);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);
    for(guint round=0;round<2;round++){
      xcf_colorify_model(filter,raw,tint);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
      image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
      xcf_colorify_model(filter,raw,tint);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
      if(!round)xcf_max_rgb_lower_input(image,4,changed);else gimp_filter_layer_invalidate(filter);
      xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
      xcf_colorify_model(filter,raw,tint);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    }
    g_test_message("Colorify unsupported tuple%u: two Save/Open rounds, exact nullable tuple/raw/cache retained, zero executions",scene);
    g_object_unref(image);if(tint)g_variant_unref(tint);
  }
  g_bytes_unref(raw);g_bytes_unref(cache);g_bytes_unref(source);g_bytes_unref(changed);
}
