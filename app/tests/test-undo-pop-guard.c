/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundo.h"
#include "core/gimpundostack.h"
#include "core/gimpundopopguard.h"
#include "core/gimpcontainer.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *application;static GimpImage *image;
static guint prepared,finished,popped,nested;static gboolean release_image,clear_in_prepare;
static guint finish_order[8];
typedef struct {GimpUndo parent;guint id;gboolean reject,locked;} TestGuardUndo;
typedef struct {GimpUndoClass parent;} TestGuardUndoClass;
static void guard_iface(GimpUndoPopGuardInterface*);
GType test_guard_undo_get_type(void);
G_DEFINE_TYPE_WITH_CODE(TestGuardUndo,test_guard_undo,GIMP_TYPE_UNDO,G_IMPLEMENT_INTERFACE(GIMP_TYPE_UNDO_POP_GUARD,guard_iface))
static gboolean prepare(GimpUndo*u,GimpUndoMode mode){TestGuardUndo*t=(TestGuardUndo*)u;(void)mode;g_assert_false(t->locked);if(t->reject)return FALSE;t->locked=TRUE;++prepared;if(release_image){release_image=FALSE;g_object_unref(image);}if(clear_in_prepare){clear_in_prepare=FALSE;gimp_image_undo_free(image);}return TRUE;}
static void finish_guard(GimpUndo*u,GimpUndoMode mode){TestGuardUndo*t=(TestGuardUndo*)u;(void)mode;g_assert_true(t->locked);t->locked=FALSE;finish_order[finished++]=t->id;}
static void guard_iface(GimpUndoPopGuardInterface*i){i->prepare=prepare;i->finish=finish_guard;}
static void pop(GimpUndo*u,GimpUndoMode mode,GimpUndoAccumulator*a){g_assert_true(((TestGuardUndo*)u)->locked);++popped;GIMP_UNDO_CLASS(test_guard_undo_parent_class)->pop(u,mode,a);}
static void test_guard_undo_class_init(TestGuardUndoClass*c){GIMP_UNDO_CLASS(c)->pop=pop;}
static void test_guard_undo_init(TestGuardUndo*u){(void)u;}
static gboolean begin(void){if(!g_test_subprocess()){g_test_trap_subprocess(NULL,30*G_TIME_SPAN_SECOND,0);g_test_trap_assert_passed();return FALSE;}gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");application=gimp_init_for_testing();image=gimp_image_new(application,16,16,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);return TRUE;}
static void end(void){if(image)g_object_unref(image);gimp_exit(application,TRUE);g_object_unref(application);}
static TestGuardUndo*push(guint id){TestGuardUndo*t=(TestGuardUndo*)gimp_image_undo_push(image,test_guard_undo_get_type(),GIMP_UNDO_PAINT,"Guard",GIMP_DIRTY_IMAGE,NULL);g_assert_nonnull(t);t->id=id;return t;}
static void ordinary(void){if(!begin())return;g_assert_nonnull(gimp_image_undo_push(image,GIMP_TYPE_UNDO,GIMP_UNDO_PAINT,"Ordinary",GIMP_DIRTY_IMAGE,NULL));g_assert_true(gimp_image_undo(image));g_assert_true(gimp_image_redo(image));g_assert_cmpuint(prepared,==,0);end();}
static void success(void){if(!begin())return;push(1);g_assert_true(gimp_image_undo(image));g_assert_true(gimp_image_redo(image));g_assert_cmpuint(prepared,==,2);g_assert_cmpuint(finished,==,2);g_assert_cmpuint(popped,==,2);end();}
static void rejected(void){if(!begin())return;TestGuardUndo*t=push(1);t->reject=TRUE;gint dirty=gimp_image_is_dirty(image);g_assert_false(gimp_image_undo(image));g_assert_true(gimp_undo_stack_peek(gimp_image_get_undo_stack(image))==GIMP_UNDO(t));g_assert_cmpint(gimp_image_is_dirty(image),==,dirty);g_assert_cmpuint(finished,==,0);g_assert_cmpuint(popped,==,0);t->reject=FALSE;g_assert_true(gimp_image_undo(image));t->reject=TRUE;g_assert_false(gimp_image_redo(image));g_assert_true(gimp_undo_stack_peek(gimp_image_get_redo_stack(image))==GIMP_UNDO(t));end();}
static void group_reject(void){if(!begin())return;g_assert_true(gimp_image_undo_group_start(image,GIMP_UNDO_GROUP_PAINT,"Group"));TestGuardUndo*a=push(1);push(2);push(3);gimp_image_undo_group_end(image);a->reject=TRUE;GimpUndo*top=gimp_undo_stack_peek(gimp_image_get_undo_stack(image));g_assert_false(gimp_image_undo(image));g_assert_true(gimp_undo_stack_peek(gimp_image_get_undo_stack(image))==top);g_assert_cmpuint(prepared,==,2);g_assert_cmpuint(finished,==,2);g_assert_cmpuint(finish_order[0],==,2);g_assert_cmpuint(finish_order[1],==,3);g_assert_cmpuint(popped,==,0);a->reject=FALSE;prepared=finished=0;g_assert_true(gimp_image_undo(image));g_assert_cmpuint(popped,==,3);g_assert_true(gimp_image_redo(image));g_assert_cmpuint(popped,==,6);end();}
static gboolean pending(GimpImage*i,gpointer d){(void)i;(void)d;return TRUE;}
static void pending_reject(void){if(!begin())return;push(1);gulong id=g_signal_connect(image,"query-pending-paint",G_CALLBACK(pending),NULL);g_assert_false(gimp_image_undo(image));g_assert_false(gimp_image_strong_undo(image));g_assert_cmpuint(prepared,==,0);g_signal_handler_disconnect(image,id);g_assert_true(gimp_image_undo(image));end();}
static void removed(GimpContainer*c,GimpObject*o,gpointer d){(void)c;(void)o;TestGuardUndo*t=d;g_assert_true(t->locked);g_assert_false(gimp_image_undo(image));g_assert_false(gimp_image_redo(image));g_assert_false(gimp_image_strong_undo(image));++nested;}
static void reentrant_remove(void){if(!begin())return;TestGuardUndo*t=push(1);gulong id=g_signal_connect(gimp_image_get_undo_stack(image)->undos,"remove",G_CALLBACK(removed),t);g_assert_true(gimp_image_undo(image));g_assert_cmpuint(nested,==,1);g_assert_cmpuint(popped,==,1);g_signal_handler_disconnect(gimp_image_get_undo_stack(image)->undos,id);end();}
static void event(GimpImage*i,GimpUndoEvent e,GimpUndo*u,gpointer d){(void)i;(void)u;TestGuardUndo*t=d;if(e==GIMP_UNDO_EVENT_UNDO||e==GIMP_UNDO_EVENT_REDO){g_assert_true(t->locked);g_assert_false(gimp_image_redo(image));++nested;}}
static void notification_lease(void){if(!begin())return;TestGuardUndo*t=push(1);gulong id=g_signal_connect(image,"undo-event",G_CALLBACK(event),t);g_assert_true(gimp_image_undo(image));g_assert_true(gimp_image_redo(image));g_assert_cmpuint(nested,==,2);g_signal_handler_disconnect(image,id);end();}
static void changed_during_prepare(void){if(!begin())return;push(1);clear_in_prepare=TRUE;g_assert_false(gimp_image_undo(image));g_assert_cmpuint(prepared,==,1);g_assert_cmpuint(finished,==,1);g_assert_cmpuint(popped,==,0);end();}
static void last_owner(void){if(!begin())return;push(1);gpointer weak=image;g_object_add_weak_pointer(G_OBJECT(image),&weak);release_image=TRUE;g_assert_true(gimp_image_undo(image));g_assert_null(weak);g_assert_cmpuint(finished,==,1);image=NULL;end();}
static void nested_group(void){
 if(!begin())return;
 g_assert_true(gimp_image_undo_group_start(image,GIMP_UNDO_GROUP_PAINT,"Outer"));
 GimpUndoStack*outer=GIMP_UNDO_STACK(gimp_undo_stack_peek(gimp_image_get_undo_stack(image)));
 GimpUndoStack*inner=gimp_undo_stack_new(image);GIMP_UNDO(inner)->undo_type=GIMP_UNDO_GROUP_PAINT;
 TestGuardUndo*a=g_object_new(test_guard_undo_get_type(),"image",image,"undo-type",GIMP_UNDO_PAINT,NULL);a->id=1;
 TestGuardUndo*b=g_object_new(test_guard_undo_get_type(),"image",image,"undo-type",GIMP_UNDO_PAINT,NULL);b->id=2;
 gimp_undo_stack_push_undo(inner,GIMP_UNDO(a));gimp_undo_stack_push_undo(inner,GIMP_UNDO(b));gimp_undo_stack_push_undo(outer,GIMP_UNDO(inner));gimp_image_undo_group_end(image);
 a->reject=TRUE;g_assert_false(gimp_image_undo(image));g_assert_cmpuint(prepared,==,1);g_assert_cmpuint(finished,==,1);g_assert_cmpuint(popped,==,0);
 a->reject=FALSE;prepared=finished=0;g_assert_true(gimp_image_undo(image));g_assert_true(gimp_image_redo(image));g_assert_cmpuint(popped,==,4);end();
}
static void strong_reject(void){if(!begin())return;TestGuardUndo*t=push(1);GIMP_UNDO(t)->undo_type=GIMP_UNDO_ITEM_VISIBILITY;t->reject=TRUE;g_assert_false(gimp_image_strong_undo(image));g_assert_cmpuint(popped,==,0);t->reject=FALSE;g_assert_true(gimp_image_undo(image));t->reject=TRUE;g_assert_false(gimp_image_strong_redo(image));g_assert_cmpuint(popped,==,1);end();}
int main(int argc,char**argv){g_test_init(&argc,&argv,NULL);g_test_add_func("/pop-guard/ordinary",ordinary);g_test_add_func("/pop-guard/success",success);g_test_add_func("/pop-guard/rejected",rejected);g_test_add_func("/pop-guard/group-reverse-unwind",group_reject);g_test_add_func("/pop-guard/pending",pending_reject);g_test_add_func("/pop-guard/remove-reentry",reentrant_remove);g_test_add_func("/pop-guard/notification-lease",notification_lease);g_test_add_func("/pop-guard/changed-prepare",changed_during_prepare);g_test_add_func("/pop-guard/last-image-owner",last_owner);g_test_add_func("/pop-guard/nested-group",nested_group);g_test_add_func("/pop-guard/strong-rejected",strong_reject);return g_test_run();}
