/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-perspective-guide.h"
#include "core/gimpimage-undo.h"
#include "display/gimpdisplayshell.h"
#include "painter/gimp-painter-binding.h"
#include "tests.h"
#include "gimp-app-test-utils.h"

static Gimp *gimp;
typedef struct {
  GimpPerspectiveGuide parent_instance;
  guint removed_calls, before_calls, after_calls;
} ProbeGuide;
typedef GimpPerspectiveGuideClass ProbeGuideClass;
G_DEFINE_TYPE (ProbeGuide, probe_guide, GIMP_TYPE_PERSPECTIVE_GUIDE)
static void probe_removed (GimpPerspectiveGuide *guide)
{ ++((ProbeGuide *) guide)->removed_calls; }
static void probe_guide_class_init (ProbeGuideClass *klass)
{ GIMP_PERSPECTIVE_GUIDE_CLASS (klass)->removed = probe_removed; }
static void probe_guide_init (ProbeGuide *guide) { }

/* This is an extracted SOURCE oracle, not a claim to run the old binary. */
#define gimp_display_shell_snap_angle legacy_snap_angle
#define g_print(...) ((void)0)
#include "../../migration/fixtures/legacy-perspective/snap-angle-source.inc"
#undef g_print
#undef gimp_display_shell_snap_angle

static void point (GimpPerspectiveGuide *guide, gint index, gdouble ex, gdouble ey)
{
  gdouble x=-1,y=-1;
  g_assert_true (gimp_perspective_guide_get_vanish_points (guide,index,&x,&y));
  g_assert_cmpfloat (x,==,ex);g_assert_cmpfloat(y,==,ey);
}
static void model_bounds (void)
{
  GimpPerspectiveGuide *guide=gimp_perspective_guide_new(97);
  gdouble x=41,y=42,angle=-1;guint id=0;
  g_assert_cmpint(gimp_perspective_guide_get_vanish_point_length(guide),==,0);
  g_assert_false(gimp_perspective_guide_get_vanish_points(guide,0,&x,&y));
  g_assert_cmpint(gimp_perspective_guide_add_vanish_points(guide,10,20),==,0);
  g_assert_cmpint(gimp_perspective_guide_add_vanish_points(guide,30,40),==,1);
  g_assert_cmpint(gimp_perspective_guide_add_vanish_points(guide,50,60),==,2);
  g_assert_cmpint(gimp_perspective_guide_add_vanish_points(guide,70,80),==,-1);
  g_assert_false(gimp_perspective_guide_remove_vanish_points(guide,-1));
  g_assert_false(gimp_perspective_guide_set_vanish_points(guide,-1,0,0));
  g_assert_false(gimp_perspective_guide_get_vanish_points(guide,-1,&x,&y));
  g_assert_false(gimp_perspective_guide_get_vanish_points(guide,3,&x,&y));
  g_assert_false(gimp_perspective_guide_get_vanish_points(guide,0,NULL,&y));
  g_assert_false(gimp_perspective_guide_set_vanish_points(guide,0,NAN,0));
  g_assert_true(gimp_perspective_guide_remove_vanish_points(guide,1));
  point(guide,0,10,20);point(guide,1,50,60);
  g_object_set(guide,"angle",G_PI/3,NULL);g_object_get(guide,"id",&id,"angle",&angle,NULL);
  g_assert_cmpuint(id,==,97);g_assert_cmpfloat_with_epsilon(angle,G_PI/3,1e-15);
  g_assert_cmpint(gimp_perspective_guide_add_vanish_points(guide,70,80),==,2);
  g_object_unref(guide);
}
static void runtime_model_fixture(void)
{
  gchar *path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration","fixtures","legacy-perspective","runtime-capture.log",NULL),*text=NULL;
  gchar **lines;GError *error=NULL;GimpPerspectiveGuide *guide=gimp_perspective_guide_new(97);guint records=0;
  g_assert_true(g_file_get_contents(path,&text,NULL,&error));g_assert_no_error(error);g_free(path);lines=g_strsplit(text,"\n",-1);
  for(gchar **line=lines;*line;++line)
    {
      gint index,expected,length;gdouble x,y;
      if(sscanf(*line,"ADD %d %d %d",&index,&expected,&length)==3)
        {g_assert_cmpint(gimp_perspective_guide_add_vanish_points(guide,index*10+1,index*10+2),==,expected);g_assert_cmpint(gimp_perspective_guide_get_vanish_point_length(guide),==,length);++records;}
      else if(sscanf(*line,"POINT %d %d %lf %lf",&index,&expected,&x,&y)==4)
        {g_assert_cmpint(expected,==,TRUE);point(guide,index,x,y);++records;}
      else if(sscanf(*line,"MOVE %d",&expected)==1)
        {g_assert_cmpint(gimp_perspective_guide_set_vanish_points(guide,1,30,40),==,expected);++records;}
      else if(sscanf(*line,"REMOVE %d",&expected)==1)
        {g_assert_cmpint(gimp_perspective_guide_remove_vanish_points(guide,1),==,expected);++records;}
      else if(g_str_has_prefix(*line,"PROPERTY "))
        {
          guint id;gdouble angle;
          if(g_str_has_prefix(*line,"PROPERTY angle-set"))g_object_set(guide,"angle",1.25,NULL);
          g_object_get(guide,"id",&id,"angle",&angle,NULL);
          g_assert_cmpuint(id,==,97);g_assert_cmpfloat(angle,==,g_str_has_prefix(*line,"PROPERTY angle-set")?1.25:0);
          ++records; /* Explicit safe correction of old id/angle accessor defect. */
        }
    }
  g_assert_cmpuint(records,==,14);g_strfreev(lines);g_free(text);g_object_unref(guide);
}
static void source_oracle (void)
{
  GimpDisplayShell shell={0};GRand *random=g_rand_new_with_seed(0x70657273);
  const gdouble scales[][2]={{1,1},{0.1,0.1},{4,4},{0.25,3},{2,0.3}};
  guint cases=0;
  for(gint count=1;count<=3;++count)for(guint scale=0;scale<G_N_ELEMENTS(scales);++scale)
    for(gint sample=0;sample<500;++sample)
      {
        GimpPerspectiveGuide *guide=gimp_perspective_guide_new(0);
        gdouble ox=g_rand_double_range(random,-2000,2000),oy=g_rand_double_range(random,-2000,2000);
        gdouble x=g_rand_double_range(random,-2000,2000),y=g_rand_double_range(random,-2000,2000);
        gdouble expected=-99,actual=-99;
        shell.scale_x=scales[scale][0];shell.scale_y=scales[scale][1];
        for(gint i=0;i<count;++i)gimp_perspective_guide_add_vanish_points(guide,g_rand_double_range(random,-2000,2000),g_rand_double_range(random,-2000,2000));
        g_assert_cmpint(gimp_perspective_guide_snap_angle(guide,ox,oy,x,y,shell.scale_x,shell.scale_y,&actual),==,
                        legacy_snap_angle(&shell,x,y,&expected,ox,oy,guide));
        g_assert_cmpfloat_with_epsilon(actual,expected,1e-14);
        g_object_unref(guide);++cases;
      }
  g_test_message("%u exact legacy source-helper cases; runtime/property defect tested separately",cases);
  g_rand_free(random);
}
static void threshold_ties_and_constraint(void)
{
  GimpPerspectiveGuide *guide=gimp_perspective_guide_new(0);gdouble angle=99,x,y;
  gimp_perspective_guide_add_vanish_points(guide,100,100);
  g_assert_false(gimp_perspective_guide_snap_angle(guide,0,0,15.999,0,2,1,&angle));
  g_assert_cmpfloat(angle,==,99);
  g_assert_true(gimp_perspective_guide_snap_angle(guide,0,0,16,0,2,1,&angle));g_assert_cmpfloat(angle,==,0);
  g_assert_true(gimp_perspective_guide_snap_angle(guide,0,0,32,-32,1,1,&angle));
  g_assert_cmpfloat(angle,==,0); /* Horizontal precedes vertical in an exact 45-degree tie. */
  x=41;y=13;gimp_perspective_guide_constrain(G_PI/6,1,2,&x,&y);
  g_assert_cmpfloat(x,==,41);g_assert_cmpfloat_with_epsilon(y,2-40/sqrt(3),1e-12);
  x=41;y=13;gimp_perspective_guide_constrain(G_PI/3,1,2,&x,&y);
  g_assert_cmpfloat(y,==,13);g_assert_cmpfloat_with_epsilon(x,1-11/sqrt(3),1e-12);
  x=41;y=13;gimp_perspective_guide_constrain(G_PI/2,1,2,&x,&y);g_assert_cmpfloat_with_epsilon(x,1,1e-12);
  g_object_unref(guide);
}
static GimpImage *new_image(void)
{
  GimpImage *image = gimp_image_new (gimp, 128, 128, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  g_assert_null (gimp_image_get_perspective_guide (image));
  return image;
}
static void count_signal(GObject *object,gpointer data){++*(guint*)data;}
static void finalized_flag (gpointer data) { *(gboolean *) data = TRUE; }
static void removal_before (GimpPerspectiveGuide *guide, gpointer data)
{
  ProbeGuide *probe = (ProbeGuide *) guide;
  ++probe->before_calls;
  g_assert_cmpuint (probe->removed_calls, ==, probe->before_calls - 1);
}
static void removal_after (GimpPerspectiveGuide *guide, gpointer data)
{
  ProbeGuide *probe = (ProbeGuide *) guide;
  ++probe->after_calls;
  g_assert_cmpuint (probe->removed_calls, ==, probe->after_calls);
}
static void closure_released (gpointer data, GClosure *closure)
{ ++*(guint *) data; }
static void removed_class_slot (void)
{
  GimpImage *image = new_image ();
  ProbeGuide *probe = g_object_new (probe_guide_get_type (), "id", G_MAXUINT32,
                                  "angle", G_PI / 3, NULL);
  GimpPerspectiveGuide *guide = GIMP_PERSPECTIVE_GUIDE (probe);
  GimpPerspectiveGuideClass *klass = GIMP_PERSPECTIVE_GUIDE_GET_CLASS (guide);
  GTypeQuery type = { 0 }; GSignalQuery signal = { 0 };
  guint released = 0; gboolean finalized = FALSE;
  gulong before, after;
  g_assert_true (GIMP_IS_PERSPECTIVE_GUIDE_CLASS (klass));
  g_assert_true (GIMP_PERSPECTIVE_GUIDE_CLASS (G_OBJECT_GET_CLASS (guide)) == klass);
  g_assert_true (klass->removed == probe_removed);
  g_assert_cmpuint (g_type_parent (GIMP_TYPE_PERSPECTIVE_GUIDE), ==, G_TYPE_OBJECT);
  g_type_query (GIMP_TYPE_PERSPECTIVE_GUIDE, &type);
  g_assert_cmpuint (type.instance_size, ==, sizeof (GimpPerspectiveGuide));
  g_assert_cmpuint (type.class_size, ==, sizeof (GimpPerspectiveGuideClass));
  g_signal_query (g_signal_lookup ("removed", GIMP_TYPE_PERSPECTIVE_GUIDE), &signal);
  g_assert_cmpuint (signal.itype, ==, GIMP_TYPE_PERSPECTIVE_GUIDE);
  g_assert_cmpuint (signal.return_type, ==, G_TYPE_NONE);
  g_assert_cmpuint (signal.n_params, ==, 0);
  g_assert_true ((signal.signal_flags & G_SIGNAL_RUN_LAST) != 0);
  g_object_set_data_full (G_OBJECT (guide), "test-08009-finalized", &finalized, finalized_flag);
  before = g_signal_connect_data (guide, "removed", G_CALLBACK (removal_before),
                                  &released, closure_released, 0);
  after = g_signal_connect_data (guide, "removed", G_CALLBACK (removal_after),
                                 &released, closure_released, G_CONNECT_AFTER);
  gimp_image_set_perspective_guide (image, guide);
  gimp_image_set_perspective_guide (image, guide);
  g_assert_cmpuint (probe->removed_calls, ==, 0);
  gimp_image_set_perspective_guide (image, NULL);
  gimp_image_set_perspective_guide (image, NULL);
  g_assert_cmpuint (probe->removed_calls, ==, 1);
  gimp_image_set_perspective_guide (image, guide);
  g_object_unref (image);
  g_assert_cmpuint (probe->removed_calls, ==, 2);
  g_assert_cmpuint (probe->before_calls, ==, 2);
  g_assert_cmpuint (probe->after_calls, ==, 2);
  g_assert_false (finalized);
  g_signal_handler_disconnect (guide, before);
  g_signal_handler_disconnect (guide, after);
  g_assert_cmpuint (released, ==, 2);
  g_object_unref (guide);
  g_assert_true (finalized);
  g_assert_cmpuint (released, ==, 2);
}

typedef struct {
  GimpImage *image;
  GimpPerspectiveGuide *incoming;
  guint calls;
} DropOwners;
static void drop_setter_owners (GimpPerspectiveGuide *previous, gpointer data)
{
  DropOwners *drop = data;
  ++drop->calls;
  g_assert_true (gimp_image_get_perspective_guide (drop->image) == drop->incoming);
  g_object_unref (drop->image);
  g_object_unref (drop->incoming);
  g_object_unref (previous);
  drop->image = NULL; drop->incoming = NULL;
}
static void setter_owner_release (void)
{
  GimpImage *image = new_image ();
  GimpPerspectiveGuide *previous = gimp_perspective_guide_new (1);
  GimpPerspectiveGuide *incoming = gimp_perspective_guide_new (2);
  gboolean image_finalized = FALSE, previous_finalized = FALSE, incoming_finalized = FALSE;
  guint removed = 0;
  DropOwners drop = { image, incoming, 0 };
  g_object_set_data_full (G_OBJECT (image), "test-08009-finalized", &image_finalized, finalized_flag);
  g_object_set_data_full (G_OBJECT (previous), "test-08009-finalized", &previous_finalized, finalized_flag);
  g_object_set_data_full (G_OBJECT (incoming), "test-08009-finalized", &incoming_finalized, finalized_flag);
  gimp_image_set_perspective_guide (image, previous);
  g_signal_connect (previous, "removed", G_CALLBACK (drop_setter_owners), &drop);
  g_signal_connect (incoming, "removed", G_CALLBACK (count_signal), &removed);
  gimp_image_set_perspective_guide (image, incoming);
  g_assert_cmpuint (drop.calls, ==, 1);
  g_assert_cmpuint (removed, ==, 1);
  g_assert_true (image_finalized);
  g_assert_true (previous_finalized);
  g_assert_true (incoming_finalized);
}

static void duplicate_ownership (void)
{
  GimpImage *first = new_image (), *second = new_image ();
  GimpPerspectiveGuide *guide = gimp_perspective_guide_new (G_MAXUINT32), *copy;
  GimpPerspectiveGuideState a = { 0 }, b = { 0 };
  gboolean old_finalized = FALSE, copy_finalized = FALSE;
  g_object_set (guide, "angle", 1.25, NULL);
  for (gint i = 0; i < 3; ++i) gimp_perspective_guide_add_vanish_points (guide, i + .25, i - .5);
  copy = gimp_perspective_guide_duplicate (guide);
  g_assert_nonnull (copy); g_assert_true (copy != guide);
  g_assert_true (gimp_perspective_guide_get_state (guide, &a));
  g_assert_true (gimp_perspective_guide_get_state (copy, &b));
  g_assert_cmpuint (a.id, ==, b.id); g_assert_cmpfloat (a.angle, ==, b.angle);
  g_assert_cmpint (a.n_points, ==, b.n_points);
  for (gint i = 0; i < 3; ++i) point (copy, i, a.points[i].x, a.points[i].y);
  g_object_set_data_full (G_OBJECT (guide), "test-08009-finalized", &old_finalized, finalized_flag);
  g_object_set_data_full (G_OBJECT (copy), "test-08009-finalized", &copy_finalized, finalized_flag);
  gimp_image_set_perspective_guide (first, guide);
  gimp_image_set_perspective_guide (second, copy);
  g_object_unref (guide); g_object_unref (copy);
  g_assert_true (gimp_perspective_guide_set_vanish_points (gimp_image_get_perspective_guide (first), 0, 91, 92));
  point (gimp_image_get_perspective_guide (second), 0, .25, -.5);
  g_object_unref (first); g_assert_true (old_finalized); g_assert_false (copy_finalized);
  point (gimp_image_get_perspective_guide (second), 2, 2.25, 1.5);
  g_object_unref (second); g_assert_true (copy_finalized);
}
static void image_ownership(void)
{
  GimpImage *image=new_image();GimpPerspectiveGuide *guide=gimp_perspective_guide_new(3);
  GimpPerspectiveGuide *weak=guide;guint count=0;
  g_object_add_weak_pointer(G_OBJECT(guide),(gpointer*)&weak);
  g_signal_connect(image,"perspective-guide-changed",G_CALLBACK(count_signal),&count);
  gimp_image_set_perspective_guide(image,guide);g_object_unref(guide);
  g_assert_true(gimp_image_get_perspective_guide(image)==weak);
  gimp_image_set_perspective_guide(image,weak);g_assert_cmpuint(count,==,1);
  gimp_image_set_perspective_guide(image,NULL);g_assert_null(weak);g_assert_cmpuint(count,==,2);
  guide=gimp_perspective_guide_new(4);gimp_perspective_guide_add_vanish_points(guide,1,2);
  gimp_image_set_perspective_guide(image,guide);g_object_unref(image);
  point(guide,0,1,2);g_assert_true(gimp_perspective_guide_set_vanish_points(guide,0,3,4));
  g_object_unref(guide);
}
typedef struct {GimpImage *image;GimpPerspectiveGuide *replacement;guint calls;} Replace;
static void removed_replace(GObject *guide,gpointer data)
{
  Replace *replace=data;++replace->calls;
  gimp_image_set_perspective_guide(replace->image,replace->replacement);
}
static void reentrant_replacement(void)
{
  GimpImage *image=new_image();GimpPerspectiveGuide *a=gimp_perspective_guide_new(1),*b=gimp_perspective_guide_new(2),*c=gimp_perspective_guide_new(3);
  Replace replace={image,c,0};
  gimp_image_set_perspective_guide(image,a);g_signal_connect(a,"removed",G_CALLBACK(removed_replace),&replace);
  gimp_image_set_perspective_guide(image,b);g_assert_true(gimp_image_get_perspective_guide(image)==c);g_assert_cmpuint(replace.calls,==,1);
  g_object_unref(a);g_object_unref(b);g_object_unref(c);g_object_unref(image);
}
static void dispose_reentry(void)
{
  GimpImage *image=new_image();GimpPerspectiveGuide *a=gimp_perspective_guide_new(1),*b=gimp_perspective_guide_new(2);
  Replace replace={image,b,0};
  gimp_image_set_perspective_guide(image,a);g_signal_connect(a,"removed",G_CALLBACK(removed_replace),&replace);
  gimp_image_perspective_guide_dispose(image);g_assert_null(gimp_image_get_perspective_guide(image));
  g_assert_cmpuint(replace.calls,==,1);gimp_image_perspective_guide_dispose(image);g_assert_cmpuint(replace.calls,==,1);
  g_object_unref(a);g_object_unref(b);g_object_unref(image);
}
static void edit_undo(void)
{
  GimpImage *image=new_image();GimpPerspectiveGuide *guide=gimp_perspective_guide_new(42),*before;
  gimp_perspective_guide_add_vanish_points(guide,1,2);
  gimp_image_perspective_guide_push_undo(image,NULL,"Add perspective point");gimp_image_set_perspective_guide(image,guide);
  g_assert_true(gimp_image_undo(image));g_assert_null(gimp_image_get_perspective_guide(image));
  g_assert_true(gimp_image_redo(image));point(gimp_image_get_perspective_guide(image),0,1,2);
  before=gimp_perspective_guide_duplicate(gimp_image_get_perspective_guide(image));
  gimp_perspective_guide_set_vanish_points(gimp_image_get_perspective_guide(image),0,10,20);
  gimp_image_perspective_guide_push_undo(image,before,"Move perspective point");g_object_unref(before);
  g_assert_true(gimp_image_undo(image));point(gimp_image_get_perspective_guide(image),0,1,2);
  g_assert_true(gimp_image_redo(image));point(gimp_image_get_perspective_guide(image),0,10,20);
  before=gimp_perspective_guide_duplicate(gimp_image_get_perspective_guide(image));gimp_image_set_perspective_guide(image,NULL);
  gimp_image_perspective_guide_push_undo(image,before,"Remove perspective point");g_object_unref(before);
  g_assert_true(gimp_image_undo(image));point(gimp_image_get_perspective_guide(image),0,10,20);
  g_assert_true(gimp_image_redo(image));g_assert_null(gimp_image_get_perspective_guide(image));
  g_object_unref(guide);g_object_unref(image);
}
static void changed_unref(GObject *object,gpointer data){g_object_unref(object);}
static void signal_lifetime(void)
{
  GimpPerspectiveGuide *guide=gimp_perspective_guide_new(0),*weak=guide;
  g_object_add_weak_pointer(G_OBJECT(guide),(gpointer*)&weak);
  g_signal_connect(guide,"changed",G_CALLBACK(changed_unref),NULL);
  g_assert_cmpint(gimp_perspective_guide_add_vanish_points(guide,1,2),==,0);g_assert_null(weak);
}
int main(int argc,char **argv)
{
  gint result;g_test_init(&argc,&argv,NULL);
  gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();
#define ADD(name) g_test_add_func("/perspective/" #name,name)
  ADD(model_bounds);ADD(runtime_model_fixture);ADD(source_oracle);ADD(threshold_ties_and_constraint);ADD(image_ownership);
  ADD(reentrant_replacement);ADD(dispose_reentry);ADD(edit_undo);ADD(signal_lifetime);
  ADD(removed_class_slot);ADD(setter_owner_release);ADD(duplicate_ownership);
  result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;
}
