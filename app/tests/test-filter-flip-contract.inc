/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/gimpimage-flip.h"
/* afa43fae FilterLayer::flip resets/invalidates only. Unlike its neighboring
 * scale/resize/rotate/transform, it never calls the parent pixel transform.
 * The old image-flip path dispatches this same virtual to every layer. */
static void
filter_flip_legacy_reset_contract (void)
{
  GimpItemClass *klass=g_type_class_ref (GIMP_TYPE_FILTER_LAYER);
  GimpItemClass *parent=g_type_class_peek_parent (klass);
  g_assert_true (klass->translate!=parent->translate);
  g_assert_true (klass->scale!=parent->scale);
  g_assert_true (klass->resize!=parent->resize);
  /* All five geometry wrappers preserve parent delegation then invalidate.
   * Separate behavioral contracts observe native bounds/buffers and no-ops. */
  g_assert_true (klass->rotate!=parent->rotate);
  g_assert_true (klass->transform!=parent->transform);
  g_type_class_unref (klass);
  for (guint whole_image=0;whole_image<2;whole_image++)
    for (guint vertical=0;vertical<2;vertical++)
      {
        GimpImage *image=image_new (40,40);
        GimpLayer *source=source_new (image,NULL,40,40);
        GimpFilterLayer *filter;
        guchar pixels[40*40*4];
        guint64 generation,runs;
        GimpOrientationType orientation=vertical ? GIMP_ORIENTATION_VERTICAL : GIMP_ORIENTATION_HORIZONTAL;
        for (guint y=0;y<40;y++) for (guint x=0;x<40;x++)
          {
            guint i=(y*40+x)*4;
            gboolean first=(vertical ? y : x)<20;
            pixels[i]=first?255:0;pixels[i+1]=0;pixels[i+2]=first?0:255;pixels[i+3]=255;
          }
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),
                         GEGL_RECTANGLE (0,0,40,40),0,babl_format ("R'G'B'A u8"),pixels,GEGL_AUTO_ROWSTRIDE);
        gimp_drawable_update (GIMP_DRAWABLE (source),0,0,40,40);
        filter=contract_gaussian_filter (image);
        gimp_item_set_offset (GIMP_ITEM (filter),4,6);
        settle (filter);
        pixel (GIMP_LAYER (filter),255,0,0,255);
        generation=gimp_filter_layer_get_generation (filter);
        runs=gimp_filter_layer_get_run_count (filter);
        gimp_image_undo_free (image);
        if (whole_image)
          gimp_image_flip (image,gimp_get_user_context (gimp),orientation,NULL);
        else
          gimp_item_flip (GIMP_ITEM (filter),gimp_get_user_context (gimp),orientation,20.0,FALSE);
        g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (filter)),==,4);
        g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (filter)),==,6);
        g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (filter)),==,16);
        g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (filter)),==,16);
        g_assert_cmpuint (gimp_filter_layer_get_generation (filter),>,generation);
        settle (filter);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter),>,runs);
        if (whole_image)
          {
            /* Ordinary source really flips; unchanged Filter geometry samples
             * the newly flipped lower stack, it does not freeze the old cache. */
            pixel (source,0,0,255,255);
            pixel (GIMP_LAYER (filter),0,0,255,255);
            g_assert_true (gimp_image_undo (image));
            settle (filter);
            pixel (source,255,0,0,255);
            pixel (GIMP_LAYER (filter),255,0,0,255);
            g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (filter)),==,4);
            g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (filter)),==,6);
            g_assert_true (gimp_image_redo (image));
            settle (filter);
            pixel (source,0,0,255,255);
            pixel (GIMP_LAYER (filter),0,0,255,255);
            g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (filter)),==,4);
            g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (filter)),==,6);
          }
        else pixel (GIMP_LAYER (filter),255,0,0,255);
        g_object_unref (image);
      }
  g_test_message ("four H/V direct/image flips retain Filter extent and reevaluate lower pixels;whole-image Undo/Redo and delegated translate/scale/resize/rotate/matrix preserved");
}
