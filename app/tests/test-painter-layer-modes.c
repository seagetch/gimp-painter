/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <string.h>
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimplayermask.h"
#include "core/gimppickable.h"
#include "operations/operations-types.h"
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
int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc,&argv,NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/painter-layer-modes/genuine_projection_and_kernel",genuine_projection_and_kernel);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");
  gimp_exit (gimp,TRUE); return result;
}
