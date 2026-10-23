/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <gio/gio.h>
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundo.h"
#include "core/gimpundostack.h"
#include "core/gimpcontainer.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
GimpUndo *gimp_image_undo_push_prepared(GimpImage*,GimpUndo*,GError**);
void prepared_undo_cpp_signature(void);
static Gimp *application;
static GimpImage *image;
static gboolean fail_init,reenter_init,release_image;
static guint reentries;
static gpointer released_undo;
typedef struct {GimpUndo parent;} TestPreparedUndo;
typedef struct {GimpUndoClass parent;} TestPreparedUndoClass;
static void initable_iface(GInitableIface*);
GType test_prepared_undo_get_type(void);
G_DEFINE_TYPE_WITH_CODE(TestPreparedUndo,test_prepared_undo,GIMP_TYPE_UNDO,G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE,initable_iface))
static gboolean initialize(GInitable*o,GCancellable*c,GError**error){
 (void)c;
 if(fail_init){g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,"injected prepared failure");return FALSE;}
 if(reenter_init){
  GimpUndo*u=GIMP_UNDO(g_object_ref(o));reenter_init=FALSE;++reentries;
  g_test_expect_message("Gimp-Core",G_LOG_LEVEL_CRITICAL,"*gimp-undo-publishing*failed*");
  g_assert_null(gimp_image_undo_push_prepared(image,u,NULL));g_test_assert_expected_messages();g_object_unref(u);
 }
 if(release_image){release_image=FALSE;released_undo=o;g_object_add_weak_pointer(G_OBJECT(o),&released_undo);g_object_unref(image);}
 return TRUE;
}
static void initable_iface(GInitableIface*i){i->init=initialize;}
static void test_prepared_undo_class_init(TestPreparedUndoClass*c){(void)c;}
static void test_prepared_undo_init(TestPreparedUndo*u){(void)u;}
static gboolean begin(void){
 if(!g_test_subprocess()){g_test_trap_subprocess(NULL,30*G_TIME_SPAN_SECOND,0);g_test_trap_assert_passed();return FALSE;}
 gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");application=gimp_init_for_testing();image=gimp_image_new(application,16,16,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);return TRUE;
}
static void finish(void){if(image)g_object_unref(image);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(application,TRUE);}
static GimpUndo*make(void){return g_object_new(test_prepared_undo_get_type(),"image",image,"undo-type",GIMP_UNDO_PAINT,"name","Prepared fixture","dirty-mask",GIMP_DIRTY_NONE,NULL);}
static void seed_redo(void){g_assert_nonnull(gimp_image_undo_push(image,GIMP_TYPE_UNDO,GIMP_UNDO_PAINT,"Old history",GIMP_DIRTY_NONE,NULL));g_assert_true(gimp_image_undo(image));g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_redo_stack(image)),==,1);}
static void success(void){
 GimpUndo*u;gpointer weak;
 if(!begin())return;prepared_undo_cpp_signature();u=make();weak=u;g_object_add_weak_pointer(G_OBJECT(u),&weak);
 g_assert_true(gimp_image_undo_push_prepared(image,u,NULL)==u);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
 g_assert_true(gimp_image_undo(image));g_assert_true(gimp_image_redo(image));g_assert_nonnull(weak);gimp_image_undo_free(image);g_assert_null(weak);finish();
}
static void failure(void){
 GimpUndo*u,*redo;gpointer weak;GError*error=NULL;gboolean dirty;
 if(!begin())return;seed_redo();redo=gimp_undo_stack_peek(gimp_image_get_redo_stack(image));dirty=gimp_image_is_dirty(image);u=make();weak=u;g_object_add_weak_pointer(G_OBJECT(u),&weak);fail_init=TRUE;
 g_assert_null(gimp_image_undo_push_prepared(image,u,&error));g_assert_error(error,G_IO_ERROR,G_IO_ERROR_FAILED);g_clear_error(&error);g_assert_null(weak);g_assert_true(redo==gimp_undo_stack_peek(gimp_image_get_redo_stack(image)));g_assert_cmpint(gimp_image_is_dirty(image),==,dirty);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);finish();
}
static void frozen(void){
 GimpUndo*u,*redo;gpointer weak;GError*error=NULL;
 if(!begin())return;seed_redo();redo=gimp_undo_stack_peek(gimp_image_get_redo_stack(image));gimp_image_undo_freeze(image);u=make();weak=u;g_object_add_weak_pointer(G_OBJECT(u),&weak);
 g_assert_null(gimp_image_undo_push_prepared(image,u,&error));g_assert_error(error,G_IO_ERROR,G_IO_ERROR_FAILED);g_clear_error(&error);g_assert_null(weak);g_assert_true(redo==gimp_undo_stack_peek(gimp_image_get_redo_stack(image)));gimp_image_undo_thaw(image);finish();
}
static void invalid(void){
 GimpUndo*u;GimpImage*other;
 if(!begin())return;u=make();other=gimp_image_new(application,16,16,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
 g_test_expect_message("Gimp-Core",G_LOG_LEVEL_CRITICAL,"*undo->image == image*failed*");g_assert_null(gimp_image_undo_push_prepared(other,u,NULL));g_test_assert_expected_messages();g_assert_cmpuint(G_OBJECT(u)->ref_count,==,1);g_object_unref(u);g_object_unref(other);finish();
}
static void duplicate(void){
 GimpUndo*u;
 if(!begin())return;u=make();g_assert_nonnull(gimp_image_undo_push_prepared(image,u,NULL));g_object_ref(u);
 g_test_expect_message("Gimp-Core",G_LOG_LEVEL_CRITICAL,"*gimp-undo-published*failed*");g_assert_null(gimp_image_undo_push_prepared(image,u,NULL));g_test_assert_expected_messages();g_object_unref(u);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);finish();
}
static void grouped(void){
 if(!begin())return;g_assert_true(gimp_image_undo_group_start(image,GIMP_UNDO_GROUP_PAINT,"Prepared group"));g_assert_nonnull(gimp_image_undo_push_prepared(image,make(),NULL));g_assert_nonnull(gimp_image_undo_push_prepared(image,make(),NULL));g_assert_true(gimp_image_undo_group_end(image));g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);g_assert_true(gimp_image_undo(image));g_assert_true(gimp_image_redo(image));finish();
}
static void reentry(void){if(!begin())return;reenter_init=TRUE;g_assert_nonnull(gimp_image_undo_push_prepared(image,make(),NULL));g_assert_cmpuint(reentries,==,1);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);finish();}
static void expire(GimpImage*i,GimpUndoEvent event,GimpUndo*u,gpointer d){(void)u;(void)d;if(event==GIMP_UNDO_EVENT_UNDO_PUSHED)gimp_image_undo_free(i);}
static void immediate_expiry(void){
 GimpUndo*u;gpointer weak;
 if(!begin())return;u=make();weak=u;g_object_add_weak_pointer(G_OBJECT(u),&weak);g_signal_connect(image,"undo-event",G_CALLBACK(expire),NULL);g_assert_null(gimp_image_undo_push_prepared(image,u,NULL));g_assert_null(weak);finish();
}
static void last_image_owner(void){
 gpointer weak_image,weak_undo;GimpUndo*u;
 if(!begin())return;u=make();weak_image=image;weak_undo=u;g_object_add_weak_pointer(G_OBJECT(image),&weak_image);g_object_add_weak_pointer(G_OBJECT(u),&weak_undo);release_image=TRUE;g_assert_null(gimp_image_undo_push_prepared(image,u,NULL));g_assert_null(weak_image);g_assert_null(weak_undo);image=NULL;finish();
}
static void toggle_reenter(gpointer data,GObject*object,gboolean last){
 GimpUndo*u=data;(void)object;
 if(!last && reentries==0){
  ++reentries;g_object_ref(u);
  g_test_expect_message("Gimp-Core",G_LOG_LEVEL_CRITICAL,"*gimp-undo-publishing*failed*");g_assert_null(gimp_image_undo_push_prepared(image,u,NULL));g_test_assert_expected_messages();g_object_unref(u);
 }
}
static void toggle_owner_reentry(void){
 GimpUndo*u;gpointer weak;
 if(!begin())return;u=make();weak=image;g_object_add_weak_pointer(G_OBJECT(image),&weak);g_object_add_toggle_ref(G_OBJECT(image),toggle_reenter,u);g_object_unref(image);
 g_assert_nonnull(gimp_image_undo_push_prepared(image,u,NULL));g_assert_cmpuint(reentries,==,1);g_object_remove_toggle_ref(G_OBJECT(image),toggle_reenter,u);g_assert_null(weak);image=NULL;finish();
}
static void legacy_constructor_reentry(void){
 if(!begin())return;reenter_init=TRUE;g_assert_nonnull(gimp_image_undo_push(image,test_prepared_undo_get_type(),GIMP_UNDO_PAINT,"Legacy constructor",GIMP_DIRTY_NONE,NULL));g_assert_cmpuint(reentries,==,1);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);finish();
}
static void legacy_last_owner(void){
 gpointer weak;
 if(!begin())return;weak=image;g_object_add_weak_pointer(G_OBJECT(image),&weak);release_image=TRUE;g_assert_null(gimp_image_undo_push(image,test_prepared_undo_get_type(),GIMP_UNDO_PAINT,"Legacy constructor",GIMP_DIRTY_NONE,NULL));g_assert_null(weak);g_assert_null(released_undo);image=NULL;finish();
}
int main(int argc,char**argv){g_test_init(&argc,&argv,NULL);g_test_add_func("/prepared-undo/success",success);g_test_add_func("/prepared-undo/failure-preserves-redo",failure);g_test_add_func("/prepared-undo/frozen",frozen);g_test_add_func("/prepared-undo/invalid-owner",invalid);g_test_add_func("/prepared-undo/duplicate",duplicate);g_test_add_func("/prepared-undo/group",grouped);g_test_add_func("/prepared-undo/reentry",reentry);g_test_add_func("/prepared-undo/immediate-expiry",immediate_expiry);g_test_add_func("/prepared-undo/last-image-owner",last_image_owner);g_test_add_func("/prepared-undo/toggle-owner-reentry",toggle_owner_reentry);g_test_add_func("/prepared-undo/legacy-constructor-reentry",legacy_constructor_reentry);g_test_add_func("/prepared-undo/legacy-last-owner",legacy_last_owner);return g_test_run();}
