/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/gimpcontext.h"
#include "pdb/gimppdb.h"
#include "plug-in/gimppluginprocedure.h"
#include "file/file-open.h"
#include "file/file-save.h"
static GimpLayer *mode_named_layer(GimpImage *image,const gchar *name)
{
  GList *layers=gimp_image_get_layer_list(image);GimpLayer *found=NULL;
  for(GList *p=layers;p;p=p->next)
    if(!g_strcmp0(gimp_object_get_name(p->data),name)) found=p->data;
  g_list_free(layers);g_assert_nonnull(found);return found;
}
static void mode_capsule_id(GimpLayer *layer,guint raw)
{
  const GimpParasite *parasite=gimp_item_parasite_find(GIMP_ITEM(layer),"gimp-painter-item");
  const guint8 *data;guint32 size,id;GBytes *bytes;GVariant *dict;
  g_assert_nonnull(parasite);data=gimp_parasite_get_data(parasite,&size);
  g_assert_cmpuint(size,>,12);g_assert_cmpmem(data,5,"GPXCF",5);
  bytes=g_bytes_new(data+12,size-12);
  dict=g_variant_ref_sink(g_variant_new_from_bytes(G_VARIANT_TYPE_VARDICT,bytes,FALSE));
  g_assert_true(g_variant_is_normal_form(dict));
  g_assert_true(g_variant_lookup(dict,"legacy-mode","u",&id));g_assert_cmpuint(id,==,raw);
  g_variant_unref(dict);g_bytes_unref(bytes);
}
static void modes_xcf_roundtrip(void)
{
  const guint raw_modes[]={0,3,23,24,25,26,27,28,29};
  const Babl *format=babl_format("R'G'B'A u8");
  const GeglRectangle rect={0,0,8,8};
  guchar *a=read_fixture("backdrop.rgba"),*b=read_fixture("source.rgba"),mask[64];
  GimpPlugInProcedure *save_proc=GIMP_PLUG_IN_PROCEDURE(gimp_pdb_lookup_procedure(gimp->pdb,"gimp-xcf-save"));
  GimpPlugInProcedure *load_proc=GIMP_PLUG_IN_PROCEDURE(gimp_pdb_lookup_procedure(gimp->pdb,"gimp-xcf-load"));
  g_assert_nonnull(save_proc);g_assert_nonnull(load_proc);memset(mask,128,sizeof mask);
  for(guint layout=0;layout<3;++layout) for(guint k=0;k<G_N_ELEMENTS(raw_modes);++k) {
    guint raw=raw_modes[k];GimpLayerMode mode;GimpLayer *group=NULL,*base,*source;
    GimpLayerMask *lm;GError *error=NULL;GFileIOStream *io;GFile *file;
    gchar *name=g_strdup_printf("mode-%u-o50-m128-projection.rgba",raw);
    guchar *expected=read_fixture(name),actual[256],actual_mask[64];
    GimpImage *image=gimp_image_new(gimp,8,8,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
    gimp_painter_layer_mode_from_legacy(raw,&mode);
    if(layout) {
      group=gimp_group_layer_new(image);gimp_object_set_name(GIMP_OBJECT(group),"mode group");
      gimp_layer_set_mode(group,layout==1?GIMP_LAYER_MODE_PAINTER_NORMAL:GIMP_LAYER_MODE_PASS_THROUGH,FALSE);
      g_assert_true(gimp_image_add_layer(image,group,NULL,0,FALSE));
    }
    base=gimp_layer_new(image,8,8,format,"mode backdrop",1,GIMP_LAYER_MODE_PAINTER_NORMAL);
    source=gimp_layer_new(image,8,8,format,"mode source",.5,mode);
    g_assert_true(gimp_image_add_layer(image,base,layout==1?group:NULL,layout==2?1:0,FALSE));
    g_assert_true(gimp_image_add_layer(image,source,group,0,FALSE));
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(base)),&rect,0,format,a,GEGL_AUTO_ROWSTRIDE);
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),&rect,0,format,b,GEGL_AUTO_ROWSTRIDE);
    lm=gimp_layer_create_mask(source,GIMP_ADD_MASK_WHITE,NULL);
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(lm)),&rect,0,babl_format("Y u8"),mask,GEGL_AUTO_ROWSTRIDE);
    g_assert_nonnull(gimp_layer_add_mask(source,lm,FALSE,&error));g_assert_no_error(error);
    gimp_drawable_update(GIMP_DRAWABLE(base),0,0,8,8);gimp_drawable_update(GIMP_DRAWABLE(source),0,0,8,8);
    file=g_file_new_tmp("painter-mode-roundtrip-XXXXXX.xcf",&io,&error);g_assert_no_error(error);g_assert_nonnull(file);
    g_assert_true(g_io_stream_close(G_IO_STREAM(io),NULL,&error));g_assert_no_error(error);g_object_unref(io);
    for(guint cycle=0;cycle<2;++cycle) {
      GimpPDBStatusType status;GimpImage *copy;
      status=file_save(gimp,image,NULL,file,save_proc,GIMP_RUN_NONINTERACTIVE,FALSE,FALSE,FALSE,&error);
      g_assert_no_error(error);g_assert_cmpint(status,==,GIMP_PDB_SUCCESS);
      copy=file_open_image(gimp,gimp_get_user_context(gimp),NULL,file,0,0,FALSE,load_proc,GIMP_RUN_NONINTERACTIVE,&status,NULL,&error);
      g_assert_no_error(error);g_assert_cmpint(status,==,GIMP_PDB_SUCCESS);g_assert_nonnull(copy);
      source=mode_named_layer(copy,"mode source");
      g_assert_cmpint(gimp_layer_get_mode(source),==,mode);mode_capsule_id(source,raw);
      g_assert_cmpfloat(gimp_layer_get_opacity(source),==,.5);
      lm=gimp_layer_get_mask(source);g_assert_nonnull(lm);g_assert_true(gimp_layer_get_apply_mask(source));
      gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(lm)),&rect,1,babl_format("Y u8"),actual_mask,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      g_assert_cmpmem(actual_mask,sizeof actual_mask,mask,sizeof mask);
      if(layout) {
        group=mode_named_layer(copy,"mode group");
        g_assert_true(GIMP_IS_GROUP_LAYER(group));
        g_assert_cmpint(gimp_layer_get_mode(group),==,layout==1?GIMP_LAYER_MODE_PAINTER_NORMAL:GIMP_LAYER_MODE_PASS_THROUGH);
        g_assert_true(gimp_item_get_parent(GIMP_ITEM(source))==GIMP_ITEM(group));
      }
      gimp_pickable_flush(GIMP_PICKABLE(copy));
      gegl_buffer_get(gimp_pickable_get_buffer(GIMP_PICKABLE(copy)),&rect,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      for(guint p=0;p<sizeof actual;++p)
        if(actual[p]!=expected[p])g_error("XCF layout=%u mode=%u cycle=%u byte=%u expected=%u got=%u",layout,raw,cycle,p,expected[p],actual[p]);
      g_test_message("XCF layout %u mode %u cycle %u: ID, opacity, mask, hierarchy and 64 genuine pixels preserved",layout,raw,cycle+1);
      g_object_unref(image);image=copy;
    }
    g_object_unref(image);g_assert_true(g_file_delete(file,NULL,&error));g_assert_no_error(error);g_object_unref(file);
    g_free(name);g_free(expected);
  }
  g_free(a);g_free(b);
}
