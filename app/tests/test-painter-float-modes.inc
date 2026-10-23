/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "operations/layer-modes-legacy/gimpoperationpainterfloat.h"
static void float_close (const float *actual, const float *expected, guint count, const gchar *route, guint scene)
{
 for(guint i=0;i<count;++i)if(!isfinite(actual[i]) || fabs((double)actual[i]-expected[i])>3e-6*fmax(1,fabs(expected[i])))
  g_error("float %s scene%u component%u expected%.9g got%.9g",route,scene,i,expected[i],actual[i]);
}
static void float_native_routes(void)
{
 gchar *path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration/fixtures/painter-float/vectors.bin",NULL),*data=NULL;
 gsize length=0;GError*error=NULL;const Babl*format=babl_format("R'G'B'A float");GeglRectangle rect={0,0,8,8};
 g_assert_true(g_file_get_contents(path,&data,&length,&error));g_assert_no_error(error);g_assert_cmpuint(length,==,45*(8+832*4));
 for(guint scene=0;scene<45;++scene){
  guint32 words[834];float values[833],out[256];guint32 raw;GimpLayerMode mode;GeglBuffer *a,*b,*m;GeglNode*graph,*na,*nb,*nm,*blend;
  memcpy(words,data+scene*sizeof words,sizeof words);for(guint j=0;j<834;++j)words[j]=GUINT32_FROM_LE(words[j]);raw=words[0];memcpy(values,words+1,sizeof values);
  float opacity=values[0],*aa=values+1,*bb=values+257,*mm=values+513,*expected=values+577;
  mode=GIMP_LAYER_MODE_PAINTER_FLOAT_ERASE+raw;g_assert_true(gimp_painter_layer_mode_is_float(mode));
  guint32 old_id=999;g_assert_false(gimp_painter_layer_mode_to_legacy(mode,&old_id));g_assert_false(gimp_painter_layer_mode_is_compatibility(mode));
  a=gegl_buffer_new(&rect,format);b=gegl_buffer_new(&rect,format);m=gegl_buffer_new(&rect,babl_format("Y float"));
  gegl_buffer_set(a,&rect,0,format,aa,GEGL_AUTO_ROWSTRIDE);gegl_buffer_set(b,&rect,0,format,bb,GEGL_AUTO_ROWSTRIDE);gegl_buffer_set(m,&rect,0,babl_format("Y float"),mm,GEGL_AUTO_ROWSTRIDE);
  graph=gegl_node_new();na=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",a,NULL);nb=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",b,NULL);nm=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",m,NULL);
  blend=gegl_node_new_child(graph,"operation","gimp:painter-float-mode","layer-mode",mode,"opacity",(double)opacity,NULL);
  gegl_node_connect_to(na,"output",blend,"input");gegl_node_connect_to(nb,"output",blend,"aux");gegl_node_connect_to(nm,"output",blend,"aux2");
  gegl_node_blit(blend,1,&rect,format,out,GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_DEFAULT);float_close(out,expected,256,"GEGL",scene);
  GimpOperationLayerMode*direct=(gpointer)gimp_layer_mode_get_operation(mode);direct->opacity=opacity;
  g_assert_true(gimp_layer_mode_get_function(mode)(GEGL_OPERATION(direct),aa,bb,mm,out,64,&rect,0));float_close(out,expected,256,"direct",scene);
  for(guint layout=0;layout<4;++layout) {
  GimpImage*image=gimp_image_new(gimp,8,8,GIMP_RGB,GIMP_PRECISION_FLOAT_NON_LINEAR);
  GimpLayer*group=NULL;
  if(layout){group=gimp_group_layer_new(image);gimp_layer_set_mode(group,layout==1?GIMP_LAYER_MODE_PAINTER_FLOAT_NORMAL:GIMP_LAYER_MODE_PASS_THROUGH,FALSE);g_assert_true(gimp_image_add_layer(image,group,NULL,0,FALSE));}
  GimpLayer*base=gimp_layer_new(image,8,8,format,"float backdrop",1,GIMP_LAYER_MODE_PAINTER_FLOAT_NORMAL),*source=gimp_layer_new(image,8,8,format,"float source",opacity,mode);
  g_assert_true(gimp_image_add_layer(image,base,layout==3?NULL:group,layout==3?1:0,FALSE));g_assert_true(gimp_image_add_layer(image,source,group,0,FALSE));
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(base)),&rect,0,format,aa,GEGL_AUTO_ROWSTRIDE);gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),&rect,0,format,bb,GEGL_AUTO_ROWSTRIDE);
  GimpLayerMask*mask=gimp_layer_create_mask(source,GIMP_ADD_MASK_WHITE,NULL);gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(mask)),&rect,0,babl_format("Y float"),mm,GEGL_AUTO_ROWSTRIDE);g_assert_nonnull(gimp_layer_add_mask(source,mask,FALSE,&error));g_assert_no_error(error);
  gimp_drawable_update(GIMP_DRAWABLE(base),0,0,8,8);gimp_drawable_update(GIMP_DRAWABLE(source),0,0,8,8);gimp_pickable_flush(GIMP_PICKABLE(image));
  gegl_buffer_get(gimp_pickable_get_buffer(GIMP_PICKABLE(image)),&rect,1,format,out,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);float_close(out,expected,256,layout==0?"native flat":layout==1?"native isolated":layout==2?"native pass-through":"native single-child pass-through",scene);
  g_object_unref(image);}
  g_object_unref(graph);g_object_unref(a);g_object_unref(b);g_object_unref(m);
 }
 g_test_message("45 scenes x64 RGBA pixels match independent float oracle in GEGL, direct PaintCore dispatch and four native image/group layouts");g_free(data);g_free(path);
}
