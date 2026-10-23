/* SPDX-License-Identifier: GPL-3.0-or-later */
static void float_zero_opacity_source_extent(void)
{
 const Babl*format=babl_format("R'G'B'A float");GeglRectangle ar={0,0,1,1},br={-2,0,5,1},sample={-2,0,1,1};
 const float opacities[]={0,.37f,1};float a[4]={.1,.2,.3,.4},b[20],actual[4];
 guint scenes=0;
 for(guint p=0;p<5;++p){b[4*p]=.234567f;b[4*p+1]=-.5;b[4*p+2]=2;b[4*p+3]=.812345f;}
 for(guint bottom=0;bottom<2;++bottom)for(guint mode=0;mode<9;++mode)
 for(guint oi=0;oi<3;++oi)for(guint mi=0;mi<3;++mi){
  GeglRectangle mr=mi==1?sample:(GeglRectangle){20,0,1,1};
  GeglBuffer*ab=gegl_buffer_new(&ar,format),*bb=gegl_buffer_new(&br,format),*mb=NULL;
  float expected[4]={0},mask=.25f;double weight=opacities[oi]*(mi==0?1:mi==1?.25:0);
  gegl_buffer_set(ab,&ar,0,format,a,GEGL_AUTO_ROWSTRIDE);gegl_buffer_set(bb,&br,0,format,b,GEGL_AUTO_ROWSTRIDE);
  GeglNode*graph=gegl_node_new(),*na=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",ab,NULL),*nb=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",bb,NULL);
  GeglNode*blend=gegl_node_new_child(graph,"operation","gimp:painter-float-mode","layer-mode",GIMP_LAYER_MODE_PAINTER_FLOAT_ERASE+mode,"opacity",(double)opacities[oi],NULL);
  if(!bottom)gegl_node_connect(blend,"input",na,"output");gegl_node_connect(blend,"aux",nb,"output");
  if(mi){mb=gegl_buffer_new(&mr,babl_format("Y float"));gegl_buffer_set(mb,&mr,0,babl_format("Y float"),&mask,GEGL_AUTO_ROWSTRIDE);GeglNode*nm=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",mb,NULL);gegl_node_connect(blend,"aux2",nm,"output");}
  /* Independent endpoint algebra with absent backdrop A=(0,0,0,0).
   * Bottom initialization is a separate documented path, not a blend. */
  if(bottom){memcpy(expected,b,3*sizeof(float));expected[3]=b[3]*weight;}
  else if(mode==4||mode==6){memcpy(expected,b,3*sizeof(float));expected[3]=b[3]*(1-weight);}
  else if(mode==2)expected[3]=b[3]*weight;
  else if(mode==1||mode==5||mode==7){expected[3]=b[3]*weight;if(expected[3]>0)memcpy(expected,b,3*sizeof(float));}
  gegl_node_blit(blend,1,&sample,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_DEFAULT);
  float_close(actual,expected,4,bottom?"bottom source-only endpoint":"nonbottom source-only endpoint",scenes++);
  g_object_unref(graph);g_object_unref(ab);g_object_unref(bb);if(mb)g_object_unref(mb);
 }
 g_test_message("162 endpoint graphs cover all float modes, bottom/nonbottom source-only ROI, zero/fractional/full opacity and absent/in-ROI/out-of-ROI masks");
}
