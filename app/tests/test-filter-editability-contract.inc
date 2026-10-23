/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pdb/pdb-types.h"
#include "pdb/gimppdb-utils.h"
#include "core/gimpimage-undo-push.h"
#include "core/gimpdrawableundo.h"
#include "core/gimpundostack.h"
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
static void filter_drawable_history_policy(void)
{
 GType derived=g_type_register_static_simple(GIMP_TYPE_DRAWABLE_UNDO,"PainterFilterPixelUndoProbe",sizeof(GimpDrawableUndoClass),NULL,sizeof(GimpDrawableUndo),NULL,0);
 for(guint method=0;method<3;++method)for(guint subtype=0;subtype<2;++subtype)for(guint frozen=0;frozen<2;++frozen){
  if(method==0 && subtype)continue;
  GType type=subtype?derived:GIMP_TYPE_DRAWABLE_UNDO;
  GimpImage *image=image_new(16,16);GimpLayer *source=source_new(image,NULL,16,16);
  GimpFilterLayer *clone=contract_gaussian_filter(image);GeglBuffer *buffer=gegl_buffer_dup(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)));
  GimpUndo *redo,*undo,*weak=NULL;GError *error=NULL;
  gimp_image_undo_free(image);
  gimp_item_set_lock_content(GIMP_ITEM(source),TRUE,FALSE);
  g_assert_nonnull(gimp_image_undo_push_drawable(image,"locked ordinary control",GIMP_DRAWABLE(source),buffer,0,0));
  g_assert_true(gimp_image_undo(image));
  redo=gimp_undo_stack_peek(gimp_image_get_redo_stack(image));g_assert_nonnull(redo);
  gimp_image_clean_all(image);g_assert_false(gimp_image_is_dirty(image));
  if(frozen)g_assert_true(gimp_image_undo_freeze(image));
  if(method==2){
   undo=g_object_new(type,"name","Filter cache must not publish","image",image,"undo-type",GIMP_UNDO_DRAWABLE,"dirty-mask",GIMP_DIRTY_ITEM|GIMP_DIRTY_DRAWABLE,"item",clone,"buffer",buffer,"x",0,"y",0,NULL);
   weak=undo;g_object_add_weak_pointer(G_OBJECT(undo),(gpointer*)&weak);
   g_assert_null(gimp_image_undo_push_prepared(image,undo,&error));
   g_assert_null(weak);g_assert_error(error,G_IO_ERROR,G_IO_ERROR_NOT_SUPPORTED);g_clear_error(&error);
  }else if(method==1)g_assert_null(gimp_image_undo_push(image,type,GIMP_UNDO_DRAWABLE,"Filter direct generic rejection",GIMP_DIRTY_ITEM|GIMP_DIRTY_DRAWABLE,"item",clone,"buffer",buffer,"x",0,"y",0,NULL));
  else g_assert_null(gimp_image_undo_push_drawable(image,"Filter cache must not publish",GIMP_DRAWABLE(clone),buffer,0,0));
  g_assert_false(gimp_image_is_dirty(image));
  g_assert_null(gimp_undo_stack_peek(gimp_image_get_undo_stack(image)));
  g_assert_true(gimp_undo_stack_peek(gimp_image_get_redo_stack(image))==redo);
  if(frozen)g_assert_true(gimp_image_undo_thaw(image));
  g_assert_true(gimp_image_redo(image));
  g_object_unref(buffer);g_object_unref(image);
 }
 g_test_message("10 Filter pixel-history refusals preserve dirty/redo, consume prepared objects; locked ordinary layer remains supported");
}
