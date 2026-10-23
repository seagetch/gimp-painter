/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native known-property ordering. Only complete records move, retaining all
 * offsets and values. Legacy byte precedes its modern float counterpart. */
static guint32 order_u32 (const guint8 *p)
{ guint32 v; memcpy (&v,p,4); return GUINT32_FROM_BE(v); }
static guint64 order_offset (const guint8 *p,guint bytes)
{ if(bytes==4)return order_u32(p);guint64 v;memcpy(&v,p,8);return GUINT64_FROM_BE(v); }
static gsize order_after_props (const guint8 *p,gsize length,gsize start)
{
  while(TRUE){g_assert_cmpuint(start+8,<=,length);guint32 type=order_u32(p+start),n=order_u32(p+start+4);g_assert_cmpuint(n,<=,length-start-8);start+=8+n;if(!type)return start;}
}
static void fields_channel_property_order (void)
{
  /* PROP_OPACITY=6, FLOAT_OPACITY=33, COLOR=16, FLOAT_COLOR=38.
   * Six linear extensions preserve precedence within each representation pair. */
  static const guint32 ids[]={6,33,16,38};
  static const guint permutations[][4]={{0,1,2,3},{0,2,1,3},{0,2,3,1},{2,3,0,1},{2,0,3,1},{2,0,1,3}};
  GimpImage *seed=fields_image();GFile *file=temporary_file();
  GimpLayer *base=gimp_layer_new(seed,96,80,gimp_image_get_layer_format(seed,TRUE),"order base",1,GIMP_LAYER_MODE_NORMAL);
  GeglColor *color=gegl_color_new(NULL);const gdouble wanted[]={.13,.27,.71,.37};
  gegl_color_set_pixel(color,babl_format("R'G'B'A double"),wanted);
  GimpChannel *channel=gimp_channel_new(seed,96,80,"order channel",color);g_object_unref(color);
  gimp_image_add_layer(seed,base,NULL,0,FALSE);gimp_image_add_channel(seed,channel,NULL,0,FALSE);
  save(seed,file);g_object_unref(seed);
  gchar *contents=NULL;gsize length=0;GError *error=NULL;
  g_assert_true(g_file_load_contents(file,NULL,&contents,&length,NULL,&error));g_assert_no_error(error);
  const guint8 *raw=(const guint8*)contents;g_assert_cmpuint(length,>,30);
  g_assert_cmpmem(raw,9,"gimp xcf ",9);g_assert_cmpint(raw[9],==,'v');
  guint version=(raw[10]-'0')*100+(raw[11]-'0')*10+raw[12]-'0';guint offset_bytes=version>=11?8:4;
  gsize pos=order_after_props(raw,length,version>=4?30:26);
  while(order_offset(raw+pos,offset_bytes)){pos+=offset_bytes;g_assert_cmpuint(pos+offset_bytes,<=,length);}pos+=offset_bytes;
  gsize channel_pos=order_offset(raw+pos,offset_bytes);g_assert_cmpuint(channel_pos+12,<=,length);
  g_assert_cmpuint(order_offset(raw+pos+offset_bytes,offset_bytes),==,0);
  gsize begin=channel_pos+12+order_u32(raw+channel_pos+8),end=order_after_props(raw,length,begin);
  GBytes *records[4]={NULL,NULL,NULL,NULL};GByteArray *other=g_byte_array_new();
  for(pos=begin;pos<end;){guint32 type=order_u32(raw+pos),n=order_u32(raw+pos+4);guint k;for(k=0;k<4;k++)if(type==ids[k])break;
    if(k<4){g_assert_null(records[k]);records[k]=g_bytes_new(raw+pos,n+8);}else g_byte_array_append(other,raw+pos,n+8);pos+=n+8;}
  for(guint k=0;k<4;k++)g_assert_nonnull(records[k]);
  for(guint permutation=0;permutation<G_N_ELEMENTS(permutations);permutation++){
    GByteArray *wire=g_byte_array_sized_new(length);g_byte_array_append(wire,raw,begin);
    for(guint k=0;k<4;k++){gsize n;const guint8 *p=g_bytes_get_data(records[permutations[permutation][k]],&n);g_byte_array_append(wire,p,n);}
    g_byte_array_append(wire,other->data,other->len);g_assert_cmpuint(wire->len,==,end);g_byte_array_append(wire,raw+end,length-end);
    g_assert_true(g_file_replace_contents(file,(const gchar*)wire->data,wire->len,NULL,FALSE,G_FILE_CREATE_NONE,NULL,NULL,&error));g_assert_no_error(error);g_byte_array_unref(wire);
    GimpImage *image=load(file);
    for(guint repeat=0;repeat<2;repeat++){
      GList *channels=gimp_image_get_channel_list(image);g_assert_cmpuint(g_list_length(channels),==,1);GimpChannel *actual=channels->data;gdouble rgba[4];
      g_assert_cmpstr(gimp_object_get_name(actual),==,"order channel");gegl_color_get_pixel(actual->color,babl_format("R'G'B'A double"),rgba);
      for(guint k=0;k<3;k++)g_assert_cmpfloat_with_epsilon(rgba[k],(gdouble)(gfloat)wanted[k],2e-7);
      g_test_message("property order%u round%u alpha %.17g",permutation,repeat,rgba[3]);g_assert_cmpfloat_with_epsilon(rgba[3],(gdouble)(gfloat)wanted[3],1e-12);
      g_list_free(channels);if(!repeat){save(image,file);GimpImage *copy=load(file);g_object_unref(image);image=copy;}
    }g_object_unref(image);
  }
  for(guint k=0;k<4;k++) { g_bytes_unref(records[k]); }
  g_byte_array_unref(other);g_free(contents);g_file_delete(file,NULL,NULL);g_object_unref(file);
}
