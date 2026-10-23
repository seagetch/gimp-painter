/* Prior native port reader diagnostic; never registered in float-enabled builds. */
static guint prior_mode_diagnostics;
static gboolean prior_mode_fatal (const gchar *domain, GLogLevelFlags flags,
                                  const gchar *message, gpointer data)
{
  if (!g_strcmp0 (domain, "Gimp-Layer-Modes") &&
      (flags & G_LOG_LEVEL_CRITICAL) && strstr (message, "gimp_layer_mode_info"))
    { prior_mode_diagnostics++; return FALSE; }
  return TRUE;
}
static void prior_native_mode_reader (void)
{
  GimpImage *seed=fields_image();GFile *file=temporary_file();
  GimpLayer *base=gimp_layer_new(seed,96,80,gimp_image_get_layer_format(seed,TRUE),
                               "prior mode",1,GIMP_LAYER_MODE_NORMAL);
  gimp_image_add_layer(seed,base,NULL,0,FALSE);save(seed,file);g_object_unref(seed);
  gchar *contents=NULL;gsize length;GError *error=NULL;
  g_assert_true(g_file_load_contents(file,NULL,&contents,&length,NULL,&error));g_assert_no_error(error);
  const guint8 *raw=(guint8*)contents;guint version=(raw[10]-'0')*100+(raw[11]-'0')*10+raw[12]-'0';
  guint n=version>=11?8:4;gsize table=order_after_props(raw,length,version>=4?30:26);
  gsize pos=order_offset(raw+table,n);gsize begin=pos+16+order_u32(raw+pos+12),end=order_after_props(raw,length,begin),mode_pos=0;
  for(pos=begin;pos<end;pos+=8+order_u32(raw+pos+4))if(order_u32(raw+pos)==7)mode_pos=pos+8;
  g_assert_cmpuint(mode_pos,>,0);
  for(guint mode=73;mode<=81;mode++) {
    guint32 wire=GUINT32_TO_BE(mode);memcpy(contents+mode_pos,&wire,4);
    g_assert_true(g_file_replace_contents(file,contents,length,NULL,FALSE,G_FILE_CREATE_NONE,NULL,NULL,&error));g_assert_no_error(error);
    prior_mode_diagnostics=0;g_test_log_set_fatal_handler(prior_mode_fatal,NULL);
    GimpImage *image=load(file);GList *layers=gimp_image_get_layer_list(image);
    g_assert_cmpuint(g_list_length(layers),==,1);
    guint actual=gimp_layer_get_mode(layers->data);
    g_test_message("prior native reader requested%u retained%u diagnostics%u",mode,actual,prior_mode_diagnostics);
    g_assert_cmpuint(prior_mode_diagnostics,>,0);
    g_assert_cmpuint(actual,==,mode);
    g_list_free(layers);g_object_unref(image);g_test_log_set_fatal_handler(NULL,NULL);
  }
  g_free(contents);g_file_delete(file,NULL,NULL);g_object_unref(file);
}
