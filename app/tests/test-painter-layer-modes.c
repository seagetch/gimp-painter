/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <string.h>
#include <math.h>
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
#include "core/gimpgrouplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimplayermask.h"
#include "core/gimppickable.h"
#include "operations/operations-types.h"
#include "operations/layer-modes/gimp-layer-modes.h"
#include "operations/layer-modes-legacy/gimpoperationpainterlegacy.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
static guchar *read_fixture (const gchar *name)
{
  gchar *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),
                                   g_str_has_prefix (name,"mode-0-") || g_str_has_prefix (name,"mode-3-") ?
                                   "migration/fixtures/legacy-normal-multiply" : "migration/fixtures/legacy-modes", name, NULL);
  gchar *bytes = NULL;
  gsize size = 0;
  g_assert_true (g_file_get_contents (path, &bytes, &size, NULL));
  g_assert_cmpuint (size, ==, 256);
  g_free (path);
  return (guchar *) bytes;
}
static void genuine_projection_and_kernel (void)
{
  static const guint raw_modes[] = {0,3,23,24,25,26,27,28,29};
  static const gint opacities[] = {100,50,0,100,50,100,1,99};
  static const gint masks[] = {-1,-1,-1,128,128,0,254,1};
  const Babl *format = babl_format ("R'G'B'A u8");
  const GeglRectangle rect = {0,0,8,8};
  guchar *a = read_fixture ("backdrop.rgba"), *b = read_fixture ("source.rgba");
  for (guint mode_index = 0; mode_index < G_N_ELEMENTS (raw_modes); ++mode_index)
    for (guint c = 0; c < G_N_ELEMENTS (masks); ++c)
      {
        guint raw = raw_modes[mode_index];
        gchar *mask_name = masks[c] < 0 ? g_strdup ("none") : g_strdup_printf ("%d", masks[c]);
        gchar *name = g_strdup_printf ("mode-%u-o%d-m%s-projection.rgba",raw,opacities[c],mask_name);
        guchar *expected = read_fixture (name), actual[256], kernel[256], mask_bytes[64];
        GimpLayerMode mode;
        guint32 roundtrip;
        GeglNode *graph = gegl_node_new (), *bottom, *top, *blend, *mask_node;
        GeglBuffer *ab = gegl_buffer_new (&rect, format), *bb = gegl_buffer_new (&rect, format);
        GeglBuffer *mb = gegl_buffer_new (&rect, babl_format ("Y u8"));
        guint m = masks[c] < 0 ? 255 : masks[c];
        g_assert_true (gimp_painter_layer_mode_from_legacy (raw, &mode));
        g_assert_cmpint (mode, !=, raw);
        g_assert_true (gimp_painter_layer_mode_to_legacy (mode, &roundtrip));
        g_assert_cmpuint (roundtrip, ==, raw);
        for (guint p = 0; p < 64; ++p)
          gimp_painter_legacy_composite_u8 (a+4*p,b+4*p,kernel+4*p,opacities[c]*255/100,masks[c] < 0 ? 256 : m,raw);
        g_assert_cmpmem (kernel,sizeof kernel,expected,256);
        gegl_buffer_set (ab,&rect,0,format,a,GEGL_AUTO_ROWSTRIDE);
        gegl_buffer_set (bb,&rect,0,format,b,GEGL_AUTO_ROWSTRIDE);
        memset (mask_bytes,m,sizeof mask_bytes);
        gegl_buffer_set (mb,&rect,0,babl_format ("Y u8"),mask_bytes,GEGL_AUTO_ROWSTRIDE);
        bottom = gegl_node_new_child (graph,"operation","gegl:buffer-source","buffer",ab,NULL);
        top = gegl_node_new_child (graph,"operation","gegl:buffer-source","buffer",bb,NULL);
        blend = gegl_node_new_child (graph,"operation","gimp:painter-legacy-mode",
                                     "layer-mode",mode,"opacity",opacities[c]/100.0,NULL);
        gegl_node_connect (blend,"input",bottom,"output");
        gegl_node_connect (blend,"aux",top,"output");
        if (masks[c] >= 0)
          {
            mask_node = gegl_node_new_child (graph,"operation","gegl:buffer-source","buffer",mb,NULL);
            gegl_node_connect (blend,"aux2",mask_node,"output");
          }
        gegl_node_blit (blend,1.0,&rect,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_DEFAULT);
        g_test_message ("reference %s",name);
        for (guint p = 0; p < sizeof actual; ++p)
          if (actual[p] != expected[p]) g_error ("%s byte%u expected%u got%u",name,p,expected[p],actual[p]);
        {
          GimpImage *image = gimp_image_new (gimp,8,8,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
          GimpLayer *base = gimp_layer_new (image,8,8,format,"backdrop",1.0,GIMP_LAYER_MODE_NORMAL_LEGACY);
          GimpLayer *source = gimp_layer_new (image,8,8,format,"source",opacities[c]/100.0,mode);
          gimp_image_add_layer (image,base,NULL,0,FALSE);
          gimp_image_add_layer (image,source,NULL,0,FALSE);
          gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (base)),&rect,0,format,a,GEGL_AUTO_ROWSTRIDE);
          gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),&rect,0,format,b,GEGL_AUTO_ROWSTRIDE);
          if (masks[c] >= 0)
            {
              GimpLayerMask *lm = gimp_layer_create_mask (source,GIMP_ADD_MASK_WHITE,NULL);
              GError *error = NULL;
              gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (lm)),&rect,0,babl_format("Y u8"),mask_bytes,GEGL_AUTO_ROWSTRIDE);
              g_assert_nonnull (gimp_layer_add_mask (source,lm,FALSE,&error));
              g_assert_no_error (error);
            }
          gimp_drawable_update (GIMP_DRAWABLE(base),0,0,8,8);
          gimp_drawable_update (GIMP_DRAWABLE(source),0,0,8,8);
          gimp_pickable_flush (GIMP_PICKABLE(image));
          gegl_buffer_get (gimp_pickable_get_buffer (GIMP_PICKABLE(image)),&rect,1.0,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
          for (guint p = 0; p < sizeof actual; ++p)
            if (actual[p] != expected[p]) g_error ("app %s byte%u expected%u got%u",name,p,expected[p],actual[p]);
          g_object_unref(image);
        }
        g_object_unref (graph);g_object_unref(ab);g_object_unref(bb);g_object_unref(mb);
        g_free(expected);g_free(name);g_free(mask_name);
      }
  g_free(a);g_free(b);
}
static void direct_paint_operation (void)
{
  static const guint raw_modes[] = {0,3,23,24,25,26,27,28,29};
  static const double opacities[] = {0,.1,.5,.65,.99,1};
  static const gint masks[] = {-1,0,128,255};
  guchar *a = read_fixture ("backdrop.rgba"), *b = read_fixture ("source.rgba");
  const GeglRectangle rect = {13,27,64,1};
  for (guint k=0;k<G_N_ELEMENTS(raw_modes);++k)
    for (guint j=0;j<G_N_ELEMENTS(opacities);++j)
      for (guint m=0;m<G_N_ELEMENTS(masks);++m)
        {
          GimpLayerMode mode; gfloat in[256],aux[256],out[256],mask[64];
          gimp_painter_layer_mode_from_legacy(raw_modes[k],&mode);
          GimpOperationLayerMode *operation = GIMP_OPERATION_LAYER_MODE(gimp_layer_mode_get_operation(mode));
          operation->opacity = opacities[j];
          for (guint i=0;i<256;++i) { in[i]=a[i]/255.f; aux[i]=b[i]/255.f; }
          for (guint i=0;i<64;++i) mask[i]=masks[m]/255.f;
          g_assert_true(operation->function(GEGL_OPERATION(operation),in,aux,masks[m]<0?NULL:mask,out,64,&rect,0));
          for (guint i=0;i<64;++i) {
            guchar expected[4];
            gimp_painter_legacy_composite_u8(a+4*i,b+4*i,expected,(guint)(opacities[j]*255.999),masks[m]<0?256:masks[m],raw_modes[k]);
            for (guint c=0;c<4;++c) g_assert_cmpfloat(out[4*i+c],==,expected[c]/255.f);
          }
        }
  g_free(a);g_free(b);
}
/* Storage precision must not silently select modern blend math.  These are
 * genuine old application pixels through differently typed GEGL buffers. */
static void precision_and_offset_projection (void)
{
  static const gchar *formats[] = {"R'G'B'A u8", "R'G'B'A u16",
    "R'G'B'A u32", "R'G'B'A half", "R'G'B'A float", "R'G'B'A double"};
  const Babl *bytes = babl_format ("R'G'B'A u8");
  const GeglRectangle backdrop = {-3,-2,14,13}, source = {0,0,8,8};
  guchar *a = read_fixture ("backdrop.rgba"), *b = read_fixture ("source.rgba");
  guchar padded[14*13*4], actual[sizeof padded], expected[sizeof padded], masks[64];
  memset (masks,128,sizeof masks);
  for (guint p=0;p<14*13;++p)
    { padded[4*p]=41; padded[4*p+1]=173; padded[4*p+2]=229; padded[4*p+3]=191; }
  for (guint y=0;y<8;++y)
    memcpy (padded+4*((y+2)*14+3),a+4*y*8,8*4);
  for (guint f=0;f<G_N_ELEMENTS(formats);++f)
    for (guint raw=23;raw<=29;++raw)
      {
        gchar *name=g_strdup_printf("mode-%u-o50-m128-projection.rgba",raw);
        guchar *reference=read_fixture(name);
        GimpLayerMode mode;
        GeglNode *graph=gegl_node_new(),*bottom,*top,*mask_node,*blend;
        GeglBuffer *ab=gegl_buffer_new(&backdrop,babl_format(formats[f]));
        GeglBuffer *bb=gegl_buffer_new(&source,babl_format(formats[f]));
        GeglBuffer *mb=gegl_buffer_new(&source,babl_format("Y u8"));
        memcpy(expected,padded,sizeof expected);
        for(guint y=0;y<8;++y)
          memcpy(expected+4*((y+2)*14+3),reference+4*y*8,8*4);
        gegl_buffer_set(ab,&backdrop,0,bytes,padded,GEGL_AUTO_ROWSTRIDE);
        gegl_buffer_set(bb,&source,0,bytes,b,GEGL_AUTO_ROWSTRIDE);
        gegl_buffer_set(mb,&source,0,babl_format("Y u8"),masks,GEGL_AUTO_ROWSTRIDE);
        g_assert_true(gimp_painter_layer_mode_from_legacy(raw,&mode));
        bottom=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",ab,NULL);
        top=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",bb,NULL);
        mask_node=gegl_node_new_child(graph,"operation","gegl:buffer-source","buffer",mb,NULL);
        blend=gegl_node_new_child(graph,"operation","gimp:painter-legacy-mode","layer-mode",mode,"opacity",.5,NULL);
        gegl_node_connect(blend,"input",bottom,"output");
        gegl_node_connect(blend,"aux",top,"output");
        gegl_node_connect(blend,"aux2",mask_node,"output");
        gegl_node_blit(blend,1.0,&backdrop,bytes,actual,GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_DEFAULT);
        g_test_message("mode %u format %s: 64 old pixels + 118 outside-source pixels",raw,formats[f]);
        for(guint p=0;p<sizeof actual;++p)
          if(actual[p]!=expected[p])
            g_error("mode %u format %s byte %u expected %u got %u",raw,formats[f],p,expected[p],actual[p]);
        g_object_unref(graph);g_object_unref(ab);g_object_unref(bb);g_object_unref(mb);
        g_free(name);g_free(reference);
      }
  g_free(a);g_free(b);
}

/* The old pixel references are child-composite references, not historical
 * group captures. This checks current isolated/pass-through connection order. */
static void group_projection_routes (void)
{
  const guint raw_modes[]={0,3,23,24,25,26,27,28,29};
  const Babl *format=babl_format("R'G'B'A u8");
  const GeglRectangle rect={0,0,8,8};
  guchar *a=read_fixture("backdrop.rgba"),*b=read_fixture("source.rgba"),actual[256],mask[64];
  memset(mask,128,sizeof mask);
  for(guint depth=0;depth<3;++depth)
    for(guint pass=0;pass<2;++pass) for(guint index=0;index<G_N_ELEMENTS(raw_modes);++index) {
    guint raw=raw_modes[index];
    GimpLayerMode mode;GError *error=NULL;
    GimpImage *image=gimp_image_new(gimp,8,8,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
    GimpLayer *group=gimp_group_layer_new(image);
    GimpLayer *base=gimp_layer_new(image,8,8,format,"backdrop",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
    GimpLayer *source;
    GimpLayerMask *lm;
    gchar *name=g_strdup_printf("mode-%u-o50-m128-projection.rgba",raw);
    guchar *expected=read_fixture(name);
    gimp_painter_layer_mode_from_legacy(raw,&mode);
    source=gimp_layer_new(image,8,8,format,"source",.5,mode);
    gimp_layer_set_mode(group,pass?GIMP_LAYER_MODE_PASS_THROUGH:GIMP_LAYER_MODE_NORMAL_LEGACY,FALSE);
    g_assert_true(gimp_image_add_layer(image,group,NULL,0,FALSE));
    for(guint nested=0;nested<depth;++nested) {
      GimpLayer *inner=gimp_group_layer_new(image);
      gimp_layer_set_mode(inner,pass?GIMP_LAYER_MODE_PASS_THROUGH:GIMP_LAYER_MODE_NORMAL_LEGACY,FALSE);
      g_assert_true(gimp_image_add_layer(image,inner,group,0,FALSE));group=inner;
    }
    g_assert_true(gimp_image_add_layer(image,base,pass?NULL:group,pass?1:0,FALSE));
    g_assert_true(gimp_image_add_layer(image,source,group,0,FALSE));
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(base)),&rect,0,format,a,GEGL_AUTO_ROWSTRIDE);
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),&rect,0,format,b,GEGL_AUTO_ROWSTRIDE);
    lm=gimp_layer_create_mask(source,GIMP_ADD_MASK_WHITE,NULL);
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(lm)),&rect,0,babl_format("Y u8"),mask,GEGL_AUTO_ROWSTRIDE);
    g_assert_nonnull(gimp_layer_add_mask(source,lm,FALSE,&error));g_assert_no_error(error);
    gimp_drawable_update(GIMP_DRAWABLE(base),0,0,8,8);
    gimp_drawable_update(GIMP_DRAWABLE(source),0,0,8,8);
    gimp_pickable_flush(GIMP_PICKABLE(image));
    gegl_buffer_get(gimp_pickable_get_buffer(GIMP_PICKABLE(image)),&rect,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    for(guint p=0;p<sizeof actual;++p)
      if(actual[p]!=expected[p])g_error("group depth=%u pass=%u mode=%u byte=%u expected=%u got=%u",depth,pass,raw,p,expected[p],actual[p]);
    g_test_message("group depth %u %s mode %u: 64 pixels match old child-composite reference",depth,pass?"pass-through":"isolated",raw);
    g_free(name);g_free(expected);g_object_unref(image);
  }
  g_free(a);g_free(b);
}

static void modern_group_strength_reduction (void)
{
  const GimpLayerMode modes[]={GIMP_LAYER_MODE_NORMAL,GIMP_LAYER_MODE_MULTIPLY,
                               GIMP_LAYER_MODE_ERASE,GIMP_LAYER_MODE_SCREEN};
  for(guint m=0;m<G_N_ELEMENTS(modes);++m) {
    GimpImage *image=gimp_image_new(gimp,8,8,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
    GimpLayer *group=gimp_group_layer_new(image);
    GimpLayer *child=gimp_layer_new(image,8,8,babl_format("R'G'B'A u8"),"modern child",.5,modes[m]);
    GimpLayerMode effective;GimpLayerColorSpace blend,composite;GimpLayerCompositeMode region;
    gimp_layer_set_mode(group,GIMP_LAYER_MODE_PASS_THROUGH,FALSE);
    g_assert_true(gimp_image_add_layer(image,group,NULL,0,FALSE));
    g_assert_true(gimp_image_add_layer(image,child,group,0,FALSE));
    gimp_layer_get_effective_mode(group,&effective,&blend,&composite,&region);
    g_assert_cmpint(effective,==,modes[m]);
    g_test_message("modern mode %d retains single-child strength reduction",modes[m]);
    g_object_unref(image);
  }
}
#include "test-painter-mode-xcf.inc"
#include "test-painter-mode-group-undo.inc"
#include "test-painter-float-modes.inc"
#include "test-painter-float-storage.inc"
#include "test-painter-group-strength.inc"
#include "test-painter-float-source-extent.inc"

int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc,&argv,NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/painter-layer-modes/genuine_projection_and_kernel",genuine_projection_and_kernel);
  g_test_add_func ("/painter-layer-modes/direct_paint_operation",direct_paint_operation);
  g_test_add_func ("/painter-layer-modes/precision_and_offset_projection",precision_and_offset_projection);
  g_test_add_func ("/painter-layer-modes/group_projection_routes",group_projection_routes);
  g_test_add_func ("/painter-layer-modes/modern_group_strength_reduction",modern_group_strength_reduction);
  g_test_add_func ("/painter-layer-modes/modes_xcf_roundtrip",modes_xcf_roundtrip);
  g_test_add_func ("/painter-layer-modes/group_reorder_undo_projection",group_reorder_undo_projection);
  g_test_add_func ("/painter-layer-modes/float_native_routes",float_native_routes);
  g_test_add_func ("/painter-layer-modes/float_storage_routes",float_storage_routes);
  g_test_add_func ("/painter-layer-modes/group_mask_opacity_order",group_mask_opacity_order);
  g_test_add_func ("/painter-layer-modes/group_reparent_mask_undo",group_reparent_mask_undo);
  g_test_add_func ("/painter-layer-modes/float_zero_opacity_source_extent",float_zero_opacity_source_extent);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");
  gimp_exit (gimp,TRUE); return result;
}
