/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Original 15.003: modern read-only properties and original C query names must
 * materialize one saved definition. No old Edge or old PDB runner is invoked. */
typedef struct {
  GimpFilterLayer *filter;
  gdouble radius;
  const guchar *raw,*opaque;
  gsize raw_size,opaque_size;
  guint counts[4];
} FilterPropertyContract;
static const gchar *filter_contract_properties[]={
  "filter-procedure","filter-arguments","filter-original-definition","filter-opaque-arguments"
};
static void
filter_property_bytes (GBytes *actual,const guchar *expected,gsize size)
{
  if(!expected){g_assert_null(actual);return;}
  g_assert_nonnull(actual);g_assert_cmpuint(g_bytes_get_size(actual),==,size);
  g_assert_cmpmem(g_bytes_get_data(actual,NULL),size,expected,size);
}
static void
filter_property_values (GimpValueArray *values,gdouble radius)
{
  g_assert_nonnull(values);g_assert_cmpuint(gimp_value_array_length(values),==,6);
  g_assert_cmpint(G_VALUE_TYPE(gimp_value_array_index(values,1)),==,G_TYPE_INT);
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(values,1)),==,123);
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(values,2)),==,456);
  g_assert_cmpfloat(g_value_get_double(gimp_value_array_index(values,3)),==,radius);
  g_assert_cmpfloat(g_value_get_double(gimp_value_array_index(values,4)),==,radius);
}
static void
filter_property_snapshot (FilterPropertyContract *state)
{
  gchar *name=NULL,*duplicate;
  GimpValueArray *values=NULL,*direct;
  GArray *original;
  GBytes *raw=NULL,*opaque=NULL,*direct_raw,*direct_opaque;
  guint64 generation=gimp_filter_layer_get_generation(state->filter);
  g_object_get(state->filter,"filter-procedure",&name,"filter-arguments",&values,
               "filter-original-definition",&raw,"filter-opaque-arguments",&opaque,NULL);
  g_assert_cmpstr(name,==,"plug-in-gauss");g_free(name);
  g_assert_cmpstr(gimp_filter_layer_get_procedure(state->filter),==,"plug-in-gauss");
  duplicate=gimp_filter_layer_dup_procedure(state->filter);g_assert_cmpstr(duplicate,==,"plug-in-gauss");g_free(duplicate);
  filter_property_bytes(raw,state->raw,state->raw_size);
  filter_property_bytes(opaque,state->opaque,state->opaque_size);
  direct_raw=gimp_filter_layer_ref_definition(state->filter);
  direct_opaque=gimp_filter_layer_ref_opaque_arguments(state->filter);
  filter_property_bytes(direct_raw,state->raw,state->raw_size);
  filter_property_bytes(direct_opaque,state->opaque,state->opaque_size);
  g_clear_pointer(&raw,g_bytes_unref);g_clear_pointer(&opaque,g_bytes_unref);
  g_clear_pointer(&direct_raw,g_bytes_unref);g_clear_pointer(&direct_opaque,g_bytes_unref);
  direct=gimp_filter_layer_dup_args(state->filter);
  original=gimp_filter_layer_get_procedure_args(state->filter);
  if(state->opaque)
    {g_assert_null(values);g_assert_null(direct);g_assert_null(original);}
  else
    {
      filter_property_values(values,state->radius);filter_property_values(direct,state->radius);
      g_assert_nonnull(original);g_assert_cmpuint(original->len,==,6);
      g_assert_cmpfloat(g_value_get_double(&g_array_index(original,GValue,3)),==,state->radius);
      /* Each public materialization owns independently releasable GValues. */
      g_value_set_double(gimp_value_array_index(values,3),71.0);
      g_value_set_double(gimp_value_array_index(direct,3),72.0);
      g_value_set_double(&g_array_index(original,GValue,3),73.0);
      gimp_value_array_unref(values);gimp_value_array_unref(direct);g_array_unref(original);
      direct=gimp_filter_layer_dup_args(state->filter);filter_property_values(direct,state->radius);
      gimp_value_array_unref(direct);
    }
  g_assert_cmpuint(gimp_filter_layer_get_generation(state->filter),==,generation);
}
static void
filter_property_notified (GObject *object,GParamSpec *spec,gpointer data)
{
  FilterPropertyContract *state=data;(void)object;
  for(guint i=0;i<4;i++)if(!strcmp(spec->name,filter_contract_properties[i]))state->counts[i]++;
  /* Read-only observation at notification proves the complete saved tuple is
   * installed before publication; this does not mutate/reenter its definition. */
  filter_property_snapshot(state);
}
static GimpValueArray *
filter_property_args (gdouble radius)
{
  return gimp_value_array_new_from_types(NULL,G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,
    G_TYPE_DOUBLE,radius,G_TYPE_DOUBLE,radius,G_TYPE_INT,1,G_TYPE_NONE);
}
static void
filter_property_counts (FilterPropertyContract *state,guint expected)
{for(guint i=0;i<4;i++)g_assert_cmpuint(state->counts[i],==,expected);}
static void
filter_gaussian_property_contract (void)
{
  static const guchar raw_a[]={1,0,0xa5},raw_b[]={2,0,0x5a},raw_c[]={3,0,9},opaque_c[]={8,0,7,6};
  GimpImage *image=image_new(16,16);GimpLayer *source=source_new(image,NULL,16,16);
  GimpFilterLayer *filter=contract_gaussian_filter(image);
  FilterPropertyContract state={filter,2.0,raw_a,NULL,sizeof(raw_a),0,{0,0,0,0}};
  GimpValueArray *args;GBytes *raw,*opaque;GError *error=NULL;
  guint64 opaque_runs;
  fill(source,255,0,0,255);settle(filter);
  for(guint i=0;i<4;i++)
    {
      GParamSpec *spec=g_object_class_find_property(G_OBJECT_GET_CLASS(filter),filter_contract_properties[i]);
      gchar *signal=g_strdup_printf("notify::%s",filter_contract_properties[i]);
      g_assert_nonnull(spec);g_assert_true((spec->flags&G_PARAM_READABLE)!=0);
      g_assert_false((spec->flags&G_PARAM_WRITABLE)!=0);
      g_signal_connect(filter,signal,G_CALLBACK(filter_property_notified),&state);g_free(signal);
    }
  args=filter_property_args(2.0);raw=g_bytes_new_static(raw_a,sizeof(raw_a));
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-gauss",raw,args,&error));g_assert_no_error(error);
  g_value_set_double(gimp_value_array_index(args,3),99.0);g_value_set_int(gimp_value_array_index(args,1),999);
  gimp_value_array_unref(args);g_bytes_unref(raw);filter_property_snapshot(&state);filter_property_counts(&state,1);settle(filter);
  gimp_image_undo_free(image);
  state.radius=4.0;state.raw=raw_b;state.raw_size=sizeof(raw_b);
  args=filter_property_args(4.0);raw=g_bytes_new_static(raw_b,sizeof(raw_b));
  g_assert_true(gimp_filter_layer_edit_definition(filter,"plug-in-gauss",raw,args,&error));g_assert_no_error(error);
  gimp_value_array_unref(args);g_bytes_unref(raw);filter_property_counts(&state,2);settle(filter);
  state.radius=2.0;state.raw=raw_a;state.raw_size=sizeof(raw_a);
  g_assert_true(gimp_image_undo(image));filter_property_counts(&state,3);filter_property_snapshot(&state);settle(filter);
  state.radius=4.0;state.raw=raw_b;state.raw_size=sizeof(raw_b);
  g_assert_true(gimp_image_redo(image));filter_property_counts(&state,4);filter_property_snapshot(&state);settle(filter);
  opaque_runs=gimp_filter_layer_get_run_count(filter);
  state.raw=raw_c;state.raw_size=sizeof(raw_c);state.opaque=opaque_c;state.opaque_size=sizeof(opaque_c);
  raw=g_bytes_new_static(raw_c,sizeof(raw_c));opaque=g_bytes_new_static(opaque_c,sizeof(opaque_c));
  g_assert_true(gimp_filter_layer_set_definition_with_opaque_arguments(filter,"plug-in-gauss",raw,opaque,&error));g_assert_no_error(error);
  g_bytes_unref(raw);g_bytes_unref(opaque);filter_property_counts(&state,5);filter_property_snapshot(&state);
  g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,opaque_runs);
  /* Preserve opaque metadata without executing an unavailable argument model. */
  state.radius=1.0;state.raw=NULL;state.raw_size=0;state.opaque=NULL;state.opaque_size=0;
  args=filter_property_args(1.0);
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-gauss",NULL,args,&error));g_assert_no_error(error);
  gimp_value_array_unref(args);filter_property_counts(&state,6);filter_property_snapshot(&state);settle(filter);
  pixel(GIMP_LAYER(filter),255,0,0,255);
  g_test_message("Gaussian definition:6 publications x4 coherent property notifications;original/modern getters,independent GValue copies,binary raw/opaque distinction,Undo/Redo;reads do not schedule");
  g_signal_handlers_disconnect_by_data(filter,&state);
  g_object_unref(image);
}
