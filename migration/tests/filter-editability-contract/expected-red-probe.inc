/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pdb/pdb-types.h"
#include "pdb/gimppdb-utils.h"
#include "core/gimpimage-undo-push.h"
static void
filter_definition_editability_contract (void)
{
  GimpImage *image=image_new (16,16);
  GimpLayer *source=source_new (image,NULL,16,16);
  GimpFilterLayer *filter=contract_gaussian_filter (image);
  GimpItem *locked=NULL;
  GError *error=NULL;
  GBytes *snapshot;
  GBytes *before=g_bytes_new_static ("before",6), *after=g_bytes_new_static ("after",5);
  gimp_item_set_lock_content (GIMP_ITEM (filter),FALSE,FALSE);
  g_assert_true (gimp_item_is_content_locked (GIMP_ITEM (filter),&locked));
  g_assert_true (locked==GIMP_ITEM (filter));
  g_assert_false (gimp_pdb_item_is_modifiable (GIMP_ITEM (filter),GIMP_PDB_ITEM_CONTENT,&error));
  g_assert_nonnull (error);g_clear_error (&error);
  g_assert_true (gimp_pdb_item_is_modifiable (GIMP_ITEM (filter),GIMP_PDB_ITEM_POSITION,&error));
  g_assert_no_error (error);
  g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-gauss",before,NULL,&error));
  g_assert_no_error (error);
  gimp_image_undo_free (image);
  g_assert_true (gimp_filter_layer_edit_definition (filter,"plug-in-gauss",after,NULL,&error));
  g_assert_no_error (error);
  snapshot=gimp_filter_layer_ref_definition (filter);g_assert_true (g_bytes_equal (snapshot,after));g_bytes_unref (snapshot);
  g_assert_true (gimp_image_undo (image));
  snapshot=gimp_filter_layer_ref_definition (filter);g_assert_true (g_bytes_equal (snapshot,before));g_bytes_unref (snapshot);
  g_assert_true (gimp_image_redo (image));
  snapshot=gimp_filter_layer_ref_definition (filter);g_assert_true (g_bytes_equal (snapshot,after));g_bytes_unref (snapshot);
  g_assert_false (gimp_item_is_content_locked (GIMP_ITEM (source),NULL));
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter),==,0);
  g_bytes_unref (before);g_bytes_unref (after);g_object_unref (image);
}
static void
filter_drawable_history_policy (void)
{
  GimpImage *image=image_new (16,16);
  GimpLayer *source=source_new (image,NULL,16,16);
  GimpFilterLayer *filter=contract_gaussian_filter (image);
  GeglBuffer *buffer=gegl_buffer_dup (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)));
  gimp_image_undo_free (image);
  g_assert_nonnull (gimp_image_undo_push_drawable (image,"ordinary control",GIMP_DRAWABLE (source),buffer,0,0));
  g_assert_null (gimp_image_undo_push_drawable (image,"Filter derived-cache",GIMP_DRAWABLE (filter),buffer,0,0));
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter),==,0);
  g_object_unref (buffer);g_object_unref (image);
}
