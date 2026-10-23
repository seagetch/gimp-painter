/* SPDX-License-Identifier: GPL-3.0-or-later */
static void float_zero_opacity_source_extent(void)
{
 const Babl*format=babl_format("R'G'B'A float");GeglRectangle ar={0,0,1,1},br={-2,0,5,1},sample={-2,0,1,1};
 float a[4]={.1,.2,.3,.4},b[20],actual[4];
 for(guint p=0;p<5;++p){b[4*p]=.234567f;b[4*p+1]=-.5;b[4*p+2]=2;b[4*p+3]=.812345f;}
 for(guint mode=0;mode<2;++mode){
  GeglBuffer*ab=gegl_buffer_new(&ar,format),*bb=gegl_buffer_new(&br,format);
  gegl_buffer_set(ab,&ar,0,format,a,GEGL_AUTO_ROWSTRIDE);gegl_buffer_set(bb,&br,0,format,b,GEGL_AUTO_ROWSTRIDE);
  GeglNode*graph=gegl_node_new(),*na=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",ab,NULL),*nb=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",bb,NULL);
  GeglNode*blend=gegl_node_new_child(graph,"operation","gimp:painter-float-mode","layer-mode",mode?GIMP_LAYER_MODE_PAINTER_FLOAT_DST_OUT:GIMP_LAYER_MODE_PAINTER_FLOAT_DST_IN,"opacity",0.0,NULL);
  gegl_node_connect(blend,"input",na,"output");gegl_node_connect(blend,"aux",nb,"output");
  /* Independent endpoint equation: swapping A/B precedes Replace, and w=0
   * returns swapped-left B even in source-only extent. Not bottom-layer init. */
  gegl_node_blit(blend,1,&sample,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_DEFAULT);
  float_close(actual,b,4,mode?"DST OUT source-only opacity0":"DST IN source-only opacity0",mode);
  g_object_unref(graph);g_object_unref(ab);g_object_unref(bb);
 }
}
