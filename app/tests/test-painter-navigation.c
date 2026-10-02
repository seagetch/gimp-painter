/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <math.h>
#include <stdio.h>
#include <cairo.h>
#include "display/gimppainternavigation.h"

static const gchar *fixture, *zoom_fixture;
static gdouble norm (gdouble a) { a = fmod (a,360); return a < 0 ? a+360 : a; }
static void golden_rotation (void)
{
  FILE *in = fopen (fixture,"r");
  gint w,h,mirror,sx,sy,x,y;
  guint state,count=0;
  gdouble angle,expected;
  g_assert_nonnull (in);
  while (fscanf (in,"%d%d%d%lf%d%d%d%d%u%lf",&w,&h,&mirror,&angle,&sx,&sy,&x,&y,&state,&expected)==10)
    {
      gdouble anchor = gimp_painter_navigation_begin (w,h,sx,sy,norm (mirror ? -angle : angle),mirror,FALSE);
      gdouble actual = gimp_painter_navigation_rotate (w,h,x,y,anchor,mirror,FALSE,state);
      gdouble a=13.75,b=-29.0,c=a,d=b;
      cairo_matrix_t old,modern;
      g_assert_cmpfloat_with_epsilon (actual,norm (mirror ? -expected : expected),1e-10);
      cairo_matrix_init_identity (&old);
      cairo_matrix_translate (&old,w/2,h/2);
      if (mirror) cairo_matrix_scale (&old,-1,1);
      cairo_matrix_rotate (&old,expected/180*G_PI);
      cairo_matrix_translate (&old,-w/2,-h/2);
      cairo_matrix_init_identity (&modern);
      cairo_matrix_translate (&modern,w/2,h/2);
      cairo_matrix_rotate (&modern,actual/180*G_PI);
      if (mirror) cairo_matrix_scale (&modern,-1,1);
      cairo_matrix_translate (&modern,-w/2,-h/2);
      cairo_matrix_transform_point (&old,&a,&b);
      cairo_matrix_transform_point (&modern,&c,&d);
      g_assert_cmpfloat_with_epsilon (a,c,1e-10);
      g_assert_cmpfloat_with_epsilon (b,d,1e-10);
      ++count;
    }
  g_assert_true (feof (in)); fclose (in);
  g_assert_cmpuint (count,==,5184);
}
static void golden_zoom (void)
{
  FILE *in=fopen(zoom_fixture,"r");
  gint w,h,mirror,sx,sy,x,y;guint count=0;
  gdouble scale_x,scale_y,expected;
  g_assert_nonnull(in);
  while(fscanf(in,"%d%d%d%lf%lf%d%d%d%d%lf",&w,&h,&mirror,&scale_x,&scale_y,&sx,&sy,&x,&y,&expected)==10)
    {
      gdouble anchor=gimp_painter_navigation_zoom_begin(w,h,sx,sy,scale_x,scale_y);
      gdouble actual=gimp_painter_navigation_zoom(w,h,x,y,anchor);
      g_assert_cmpfloat_with_epsilon(actual,expected,1e-12);
      ++count;
    }
  g_assert_true(feof(in));fclose(in);g_assert_cmpuint(count,==,2400);
}
static void modifier_bits (void)
{
  const guint extras[]={0,GDK_MOD1_MASK,GDK_SUPER_MASK,GDK_META_MASK,GDK_HYPER_MASK,
                         GDK_MOD1_MASK|GDK_META_MASK|GDK_BUTTON2_MASK};
  for (guint i=0;i<G_N_ELEMENTS(extras);++i)
    {
      g_assert_cmpint (gimp_painter_navigation_middle_action(extras[i]),==,GIMP_MODIFIER_ACTION_PANNING);
      g_assert_cmpint (gimp_painter_navigation_middle_action(extras[i]|GDK_SHIFT_MASK),==,GIMP_MODIFIER_ACTION_ROTATING);
      g_assert_cmpint (gimp_painter_navigation_middle_action(extras[i]|GDK_CONTROL_MASK),==,GIMP_MODIFIER_ACTION_ZOOMING);
      g_assert_cmpint (gimp_painter_navigation_middle_action(extras[i]|GDK_SHIFT_MASK|GDK_CONTROL_MASK),==,GIMP_MODIFIER_ACTION_STEP_ROTATING);
    }
}
static void midpoint_and_toggle (void)
{
  const gdouble angles[]={7.5,22.5,337.5,352.5};
  const gdouble snapped[]={15,30,345,0};
  for (guint i=0;i<G_N_ELEMENTS(angles);++i)
    {
      gdouble anchor=gimp_painter_navigation_begin(101,113,50,56,angles[i],FALSE,FALSE);
      for (guint j=0;j<100;++j)
        {
          /* Stationary Ctrl/extra modifier transitions never alter the anchor. */
          g_assert_cmpfloat(gimp_painter_navigation_rotate(101,113,50,56,anchor,FALSE,FALSE,GDK_CONTROL_MASK|GDK_MOD1_MASK),==,snapped[i]);
          g_assert_cmpfloat(gimp_painter_navigation_rotate(101,113,50,56,anchor,FALSE,FALSE,GDK_MOD1_MASK),==,angles[i]);
        }
    }
}
static void custom_rotation_actions (void)
{
  for (guint state=0;state<16;++state)
    {
      g_assert_cmpint (gimp_painter_navigation_rotation_action (GIMP_MODIFIER_ACTION_STEP_ROTATING,FALSE,state),==,GIMP_MODIFIER_ACTION_STEP_ROTATING);
      g_assert_cmpint (gimp_painter_navigation_rotation_action (GIMP_MODIFIER_ACTION_ROTATING,FALSE,state),==,GIMP_MODIFIER_ACTION_ROTATING);
      g_assert_cmpint (gimp_painter_navigation_rotation_action (GIMP_MODIFIER_ACTION_STEP_ROTATING,TRUE,state),==,
        state & GDK_CONTROL_MASK ? GIMP_MODIFIER_ACTION_STEP_ROTATING : GIMP_MODIFIER_ACTION_ROTATING);
    }
}
static void mirrored_keys (void)
{
  const guint keys[]={GDK_KEY_Left,GDK_KEY_Right,GDK_KEY_Up,GDK_KEY_Down,GDK_KEY_a};
  for(guint i=0;i<G_N_ELEMENTS(keys);++i)
    {
      g_assert_cmpuint(gimp_painter_navigation_key(keys[i],FALSE,TRUE),==,keys[i]);
      g_assert_cmpuint(gimp_painter_navigation_key(keys[i],TRUE,FALSE),==,keys[i]);
      g_assert_cmpuint(gimp_painter_navigation_key(keys[i],TRUE,TRUE),==,
                      keys[i]==GDK_KEY_Left ? GDK_KEY_Right : keys[i]==GDK_KEY_Right ? GDK_KEY_Left : keys[i]);
    }
}
int main(int argc,char **argv)
{
  g_test_init(&argc,&argv,NULL); g_assert_cmpint(argc,==,3); fixture=argv[1]; zoom_fixture=argv[2];
  g_test_add_func("/navigation/golden-rotation",golden_rotation);
  g_test_add_func("/navigation/golden-zoom",golden_zoom);
  g_test_add_func("/navigation/modifier-bits",modifier_bits);
  g_test_add_func("/navigation/midpoint-toggle",midpoint_and_toggle);
  g_test_add_func("/navigation/mirrored-keys",mirrored_keys);
  g_test_add_func("/navigation/custom-rotation",custom_rotation_actions);
  return g_test_run();
}
