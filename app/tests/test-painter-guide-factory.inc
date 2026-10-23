/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-painter-guide-factory.h"
static void guide_factory_fault (gconstpointer data) {
  guint encoded=GPOINTER_TO_UINT(data),kind=encoded%10,stage=(encoded/10)%10,caller=encoded/100;
  GimpPerspectiveGuide *guide;
  guint id=999;
  gdouble angle=99;
  GimpPerspectiveGuideState state={0};
  state.id=999;
  painter_guide_fault_arm(kind,stage);
  guide=caller ? gimp_perspective_guide_new(73) : g_object_new(GIMP_TYPE_PERSPECTIVE_GUIDE,"id",73,NULL);
  g_assert_true(painter_guide_fault_consumed());
  if (caller) g_assert_null(guide);
  else {
    g_assert_nonnull(guide); g_assert_true(guide->binding_failed);
    g_assert_true(painter_guide_fault_closed(G_OBJECT(guide)));
    g_object_set(guide,"id",81,"angle",1.2,NULL);
    g_object_get(guide,"id",&id,"angle",&angle,NULL);
    g_assert_cmpuint(id,==,0); g_assert_cmpfloat(angle,==,0);
    g_assert_false(gimp_perspective_guide_get_state(guide,&state));g_assert_cmpuint(state.id,==,999);
    g_assert_cmpint(gimp_perspective_guide_add_vanish_points(guide,1,2),==,-1);
    g_assert_false(gimp_perspective_guide_set_vanish_points(guide,0,1,2));
    g_assert_false(gimp_perspective_guide_remove_vanish_points(guide,0));
    g_object_run_dispose(G_OBJECT(guide));g_object_run_dispose(G_OBJECT(guide));g_object_unref(guide);
  }
  g_assert_cmpuint(painter_guide_fault_live(),==,0);
}
static void guide_c_operation_contract (void) {
  GType (*type) (void)=gimp_perspective_guide_get_type;
  GimpPerspectiveGuide *(*create) (guint32)=gimp_perspective_guide_new;
  gint (*count) (GimpPerspectiveGuide *)=gimp_perspective_guide_get_vanish_point_length;
  gint (*add) (GimpPerspectiveGuide *,gdouble,gdouble)=gimp_perspective_guide_add_vanish_points;
  gboolean (*remove) (GimpPerspectiveGuide *,gint)=gimp_perspective_guide_remove_vanish_points;
  gboolean (*get) (GimpPerspectiveGuide *,gint,gdouble *,gdouble *)=gimp_perspective_guide_get_vanish_points;
  gboolean (*set) (GimpPerspectiveGuide *,gint,gdouble,gdouble)=gimp_perspective_guide_set_vanish_points;
  GimpPerspectiveGuide *a=create(0),*b=create(G_MAXUINT32);
  gdouble x=999,y=999;guint id=0;
  g_assert_cmpuint(type(),==,G_OBJECT_TYPE(a));g_assert_true(a!=b);
  g_assert_false(g_object_is_floating(a));g_assert_cmpuint(G_OBJECT(a)->ref_count,==,1);
  g_object_get(b,"id",&id,NULL);g_assert_cmpuint(id,==,G_MAXUINT32);
  g_assert_cmpint(add(a,10,20),==,0);g_assert_cmpint(add(a,30,40),==,1);
  g_assert_cmpint(count(a),==,2);g_assert_cmpint(count(b),==,0);
  g_assert_true(set(a,1,50,60));g_assert_true(remove(a,0));
  g_assert_true(get(a,0,&x,&y));g_assert_cmpfloat(x,==,50);g_assert_cmpfloat(y,==,60);
  g_assert_false(get(a,1,&x,&y));g_assert_cmpfloat(x,==,50);g_assert_cmpfloat(y,==,60);
  g_object_unref(a);g_object_unref(b);
}
static void register_guide_factory_faults (void) {
  for (guint caller=0;caller<2;++caller) for(guint stage=0;stage<2;++stage) for(guint kind=1;kind<=4;++kind) {
    gchar *name=g_strdup_printf("/perspective/factory/stage%u/caller%u/kind%u",stage,caller,kind);
    g_test_add_data_func(name,GUINT_TO_POINTER(caller*100+stage*10+kind),guide_factory_fault);g_free(name);
  }
  g_test_add_func("/perspective/factory/facade-faults",painter_guide_factory_handle_faults);
  g_test_add_func("/perspective/factory/c-operation-contract",guide_c_operation_contract);
}
