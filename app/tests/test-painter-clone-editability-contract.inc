/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pdb/pdb-types.h"
#include "pdb/gimppdb-utils.h"
static void clone_current_editability_contract(void)
{
 GimpImage *image=new_image();GimpLayer *source=new_layer(image,"source",NULL,0);
 GimpCloneLayer *clone=new_clone(image,source);GimpItem *locked=NULL;GError *error=NULL;
 GList *selection=g_list_prepend(NULL,clone);
 gimp_item_set_lock_content(GIMP_ITEM(clone),FALSE,FALSE);
 g_assert_true(gimp_item_is_content_locked(GIMP_ITEM(clone),&locked));g_assert_true(locked==GIMP_ITEM(clone));
 g_assert_false(gimp_pdb_item_is_modifiable(GIMP_ITEM(clone),GIMP_PDB_ITEM_CONTENT,&error));g_assert_nonnull(error);g_clear_error(&error);
 g_assert_true(gimp_pdb_item_is_modifiable(GIMP_ITEM(clone),GIMP_PDB_ITEM_POSITION,&error));g_assert_no_error(error);
 gimp_image_set_selected_layers(image,selection);g_list_free(selection);
 g_assert_nonnull(g_list_find(gimp_image_get_selected_layers(image),clone));
 gimp_item_translate(GIMP_ITEM(clone),3,5,TRUE);
 g_assert_cmpint(gimp_item_get_offset_x(GIMP_ITEM(clone)),==,3);g_assert_cmpint(gimp_item_get_offset_y(GIMP_ITEM(clone)),==,5);
 g_assert_true(gimp_image_undo(image));g_assert_cmpint(gimp_item_get_offset_x(GIMP_ITEM(clone)),==,0);
 g_assert_true(gimp_image_redo(image));g_assert_cmpint(gimp_item_get_offset_y(GIMP_ITEM(clone)),==,5);
 gimp_item_set_lock_position(GIMP_ITEM(clone),TRUE,FALSE);
 g_assert_false(gimp_pdb_item_is_modifiable(GIMP_ITEM(clone),GIMP_PDB_ITEM_POSITION,&error));g_assert_nonnull(error);g_clear_error(&error);
 gimp_item_set_lock_position(GIMP_ITEM(clone),FALSE,FALSE);
 g_assert_true(gimp_pdb_item_is_modifiable(GIMP_ITEM(clone),GIMP_PDB_ITEM_POSITION,&error));g_assert_no_error(error);
 g_object_unref(image);
}

/* The old four Undo source duties reject Clone cache pixel history, before
 * dirty/redo publication. User content locks on ordinary layers are different. */
#include "core/gimpimage-undo-push.h"
#include "core/gimpdrawableundo.h"
#include "core/gimpundostack.h"
static void clone_drawable_history_policy(void)
{
 GType derived=g_type_register_static_simple(GIMP_TYPE_DRAWABLE_UNDO,"PainterClonePixelUndoProbe",sizeof(GimpDrawableUndoClass),NULL,sizeof(GimpDrawableUndo),NULL,0);
 for(guint method=0;method<3;++method)for(guint subtype=0;subtype<2;++subtype)for(guint frozen=0;frozen<2;++frozen){
  if(method==0 && subtype)continue;
  GType type=subtype?derived:GIMP_TYPE_DRAWABLE_UNDO;
  GimpImage *image=new_image();GimpLayer *source=new_layer(image,"source",NULL,0);
  GimpCloneLayer *clone=new_clone(image,source);GeglBuffer *buffer=gegl_buffer_dup(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)));
  GimpUndo *redo,*undo,*weak=NULL;GError *error=NULL;
  gimp_image_undo_free(image);
  gimp_item_set_lock_content(GIMP_ITEM(source),TRUE,FALSE);
  g_assert_nonnull(gimp_image_undo_push_drawable(image,"locked ordinary control",GIMP_DRAWABLE(source),buffer,0,0));
  g_assert_true(gimp_image_undo(image));
  redo=gimp_undo_stack_peek(gimp_image_get_redo_stack(image));g_assert_nonnull(redo);
  gimp_image_clean_all(image);g_assert_false(gimp_image_is_dirty(image));
  if(frozen)g_assert_true(gimp_image_undo_freeze(image));
  if(method==2){
   undo=g_object_new(type,"name","Clone cache must not publish","image",image,"undo-type",GIMP_UNDO_DRAWABLE,"dirty-mask",GIMP_DIRTY_ITEM|GIMP_DIRTY_DRAWABLE,"item",clone,"buffer",buffer,"x",0,"y",0,NULL);
   weak=undo;g_object_add_weak_pointer(G_OBJECT(undo),(gpointer*)&weak);
   g_assert_null(gimp_image_undo_push_prepared(image,undo,&error));
   g_assert_null(weak);g_assert_error(error,G_IO_ERROR,G_IO_ERROR_NOT_SUPPORTED);g_clear_error(&error);
  }else if(method==1)g_assert_null(gimp_image_undo_push(image,type,GIMP_UNDO_DRAWABLE,"Clone direct generic rejection",GIMP_DIRTY_ITEM|GIMP_DIRTY_DRAWABLE,"item",clone,"buffer",buffer,"x",0,"y",0,NULL));
  else g_assert_null(gimp_image_undo_push_drawable(image,"Clone cache must not publish",GIMP_DRAWABLE(clone),buffer,0,0));
  g_assert_false(gimp_image_is_dirty(image));
  g_assert_null(gimp_undo_stack_peek(gimp_image_get_undo_stack(image)));
  g_assert_true(gimp_undo_stack_peek(gimp_image_get_redo_stack(image))==redo);
  if(frozen)g_assert_true(gimp_image_undo_thaw(image));
  g_assert_true(gimp_image_redo(image));
  g_object_unref(buffer);g_object_unref(image);
 }
 g_test_message("10 Clone pixel-history refusals preserve dirty/redo, consume prepared objects; locked ordinary layer remains supported");
}

typedef struct { GimpImage *image; guint calls; } CloneHistoryToggle;
static void clone_history_buffer_toggle(gpointer data,GObject *object,gboolean last)
{
 CloneHistoryToggle *state=data;
 if(!last && state->image){GimpImage *image=state->image;state->image=NULL;++state->calls;g_object_unref(image);}
}
static void clone_history_preflight_image_lease(void)
{
 GimpImage *image=new_image(),*weak=image;GimpLayer *source=new_layer(image,"source",NULL,0);
 GimpCloneLayer *clone=new_clone(image,source);GeglBuffer *buffer=gegl_buffer_dup(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)));
 CloneHistoryToggle state={NULL,0};
 g_object_add_weak_pointer(G_OBJECT(image),(gpointer*)&weak);
 g_object_add_toggle_ref(G_OBJECT(buffer),clone_history_buffer_toggle,&state);
 g_object_unref(buffer);state.image=image;
 g_assert_null(gimp_image_undo_push_drawable(image,"toggle close during value collection",GIMP_DRAWABLE(clone),buffer,0,0));
 g_assert_cmpuint(state.calls,==,1);g_assert_null(state.image);g_assert_null(weak);
 g_object_remove_toggle_ref(G_OBJECT(buffer),clone_history_buffer_toggle,&state);
}

static void clone_history_count_collection(gpointer data,GObject *object,gboolean last)
{
 guint *count=data;if(!last)++*count;
}
static void clone_history_ordinary_collect_once(void)
{
 GimpImage *image=new_image();GimpLayer *source=new_layer(image,"ordinary locked source",NULL,0);
 GeglBuffer *buffer=gegl_buffer_dup(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)));
 guint count=0;
 gimp_item_set_lock_content(GIMP_ITEM(source),TRUE,FALSE);
 g_object_add_toggle_ref(G_OBJECT(buffer),clone_history_count_collection,&count);
 g_object_unref(buffer);
 g_assert_nonnull(gimp_image_undo_push_drawable(image,"one property collection",GIMP_DRAWABLE(source),buffer,0,0));
 g_assert_cmpuint(count,==,1);
 g_object_unref(image);
 g_object_remove_toggle_ref(G_OBJECT(buffer),clone_history_count_collection,&count);
}
