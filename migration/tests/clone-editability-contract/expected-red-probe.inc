#include "core/gimpimage-undo-push.h"
static void clone_drawable_history_legacy_guard(void)
{
 GimpImage *image=new_image(); GimpLayer *source=new_layer(image,"source",NULL,0);
 GimpCloneLayer *clone=new_clone(image,source);
 GeglBuffer *buffer=gegl_buffer_dup(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)));
 gimp_image_undo_free(image);
 g_assert_nonnull(gimp_image_undo_push_drawable(image,"ordinary control",GIMP_DRAWABLE(source),buffer,0,0));
 gimp_image_undo_free(image);
 g_assert_null(gimp_image_undo_push_drawable(image,"legacy derived-cache rejection",GIMP_DRAWABLE(clone),buffer,0,0));
 g_object_unref(buffer);g_object_unref(image);
}
