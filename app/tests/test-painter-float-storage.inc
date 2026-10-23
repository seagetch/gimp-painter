/* SPDX-License-Identifier: GPL-3.0-or-later */
static void float_storage_routes(void)
{
 const gchar *formats[]={"R'G'B'A u8","R'G'B'A u16","R'G'B'A u32","R'G'B'A half","R'G'B'A float","R'G'B'A double"};
 gchar*path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration/fixtures/painter-float/storage-vectors.bin",NULL),*data=NULL;
 gsize length=0;GError*error=NULL;const Babl*ff=babl_format("R'G'B'A float");GeglRectangle inner={-3,-2,4,4},outer={-4,-3,6,6};
 g_assert_true(g_file_get_contents(path,&data,&length,&error));g_assert_no_error(error);g_assert_cmpuint(length,==,162*211*4);
 for(guint scene=0;scene<162;++scene){
  guint32 words[211];float v[209],actual[144],backdrop[144];
  memcpy(words,data+scene*sizeof words,sizeof words);for(guint i=0;i<211;++i)words[i]=GUINT32_FROM_LE(words[i]);memcpy(v,words+2,sizeof v);
  guint fi=words[0],mode=words[1];g_assert_cmpuint(fi,<,6);g_assert_cmpuint(mode,<,9);
  const Babl*storage=babl_format(formats[fi]);float *a=v+1,*b=v+65,*mask=v+129,*expected=v+145;
  GeglBuffer*ab=gegl_buffer_new(&outer,storage),*bb=gegl_buffer_new(&inner,storage),*mb=gegl_buffer_new(&inner,babl_format("Y float"));
  for(guint i=0;i<36;++i){backdrop[4*i]=.1234567f;backdrop[4*i+1]=-2;backdrop[4*i+2]=3;backdrop[4*i+3]=.6789123f;}
  gegl_buffer_set(ab,&outer,0,ff,backdrop,GEGL_AUTO_ROWSTRIDE);gegl_buffer_set(ab,&inner,0,ff,a,GEGL_AUTO_ROWSTRIDE);
  gegl_buffer_get(ab,&outer,1,ff,backdrop,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  gegl_buffer_set(bb,&inner,0,ff,b,GEGL_AUTO_ROWSTRIDE);gegl_buffer_set(mb,&inner,0,babl_format("Y float"),mask,GEGL_AUTO_ROWSTRIDE);
  GeglNode*graph=gegl_node_new(),*na=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",ab,NULL),*nb=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",bb,NULL),*nm=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",mb,NULL);
  GeglNode*blend=gegl_node_new_child(graph,"operation","gimp:painter-float-mode","layer-mode",GIMP_LAYER_MODE_PAINTER_FLOAT_ERASE+mode,"opacity",(double)v[0],NULL);
  gegl_node_connect(blend,"input",na,"output");gegl_node_connect(blend,"aux",nb,"output");gegl_node_connect(blend,"aux2",nm,"output");
  gegl_node_blit(blend,1,&outer,ff,actual,GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_DEFAULT);
  for(guint y=0;y<6;++y)for(guint x=0;x<6;++x){
   guint pos=4*(6*y+x);
   if(x>=1&&x<=4&&y>=1&&y<=4)float_close(actual+pos,expected+4*(4*(y-1)+x-1),4,formats[fi],scene);
   else g_assert_cmpmem(actual+pos,4*sizeof(float),backdrop+pos,4*sizeof(float));
  }
  g_object_unref(graph);g_object_unref(ab);g_object_unref(bb);g_object_unref(mb);
 }
 g_test_message("162 six-storage float32 graphs:16 oracle pixels and20 bit-identical outside-source pixels each at negative origin");g_free(data);g_free(path);
}
