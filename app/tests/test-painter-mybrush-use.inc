/* SPDX-License-Identifier: GPL-3.0-or-later */
typedef struct { GimpMypaintBrush parent; guint begins, ends, mode; gboolean entered; gboolean *dead; GString *events; } UseBrush;
typedef struct { GimpMypaintBrushClass parent; } UseBrushClass;
static GType use_brush_get_type (void);
G_DEFINE_TYPE (UseBrush, use_brush, GIMP_TYPE_MYPAINT_BRUSH)
static gboolean use_constructor_probe;
static void use_begin (GimpMypaintBrush *brush)
{
  UseBrush *b = (UseBrush *) brush;
  b->begins++; g_string_append_c (b->events, 'B');
  if (!b->entered)
    {
      b->entered = TRUE;
      if (b->mode == 1) { gimp_mypaint_brush_end_use (brush); gimp_mypaint_brush_begin_use (brush); }
      if (b->mode == 3) { g_object_run_dispose (G_OBJECT (brush)); gimp_mypaint_brush_end_use (brush); gimp_mypaint_brush_begin_use (brush); }
      if (b->mode == 4) { g_object_unref (brush); g_assert_false (*b->dead); }
    }
}
static void use_end (GimpMypaintBrush *brush)
{
  UseBrush *b = (UseBrush *) brush;
  b->ends++; g_string_append_c (b->events, 'E');
  if (b->mode == 2 && !b->entered) { b->entered = TRUE; gimp_mypaint_brush_begin_use (brush); }
  if (b->mode == 5) { g_object_unref (brush); g_assert_false (*b->dead); }
}
static void use_constructed (GObject *object)
{
  UseBrush *b = (UseBrush *) object;
  if (use_constructor_probe) { gimp_mypaint_brush_begin_use ((GimpMypaintBrush *) b); g_assert_cmpuint (b->begins, ==, 0); }
  G_OBJECT_CLASS (use_brush_parent_class)->constructed (object);
  if (use_constructor_probe) { gimp_mypaint_brush_begin_use ((GimpMypaintBrush *) b); gimp_mypaint_brush_end_use ((GimpMypaintBrush *) b); }
}
static void use_finalize (GObject *object)
{
  UseBrush *b = (UseBrush *) object;
  if (b->dead) *b->dead = TRUE;
  g_string_free (b->events, TRUE);
  G_OBJECT_CLASS (use_brush_parent_class)->finalize (object);
}
static void use_brush_class_init (UseBrushClass *klass)
{
  GIMP_MYPAINT_BRUSH_CLASS (klass)->begin_use = use_begin;
  GIMP_MYPAINT_BRUSH_CLASS (klass)->end_use = use_end;
  G_OBJECT_CLASS (klass)->constructed = use_constructed;
  G_OBJECT_CLASS (klass)->finalize = use_finalize;
}
static void use_brush_init (UseBrush *b) { b->events = g_string_new (""); }
static UseBrush *use_new (void) { return g_object_new (use_brush_get_type (), "name", "Nested use", NULL); }
static void nested_use_transitions (void)
{
  UseBrush *a = use_new (), *b = use_new ();
  void (*begin) (GimpMypaintBrush *) = gimp_mypaint_brush_begin_use;
  void (*end) (GimpMypaintBrush *) = gimp_mypaint_brush_end_use;
  begin ((GimpMypaintBrush *) a); begin ((GimpMypaintBrush *) a); begin ((GimpMypaintBrush *) b);
  end ((GimpMypaintBrush *) a); g_assert_cmpuint (a->ends, ==, 0);
  end ((GimpMypaintBrush *) b); end ((GimpMypaintBrush *) a);
  g_assert_cmpstr (a->events->str, ==, "BE"); g_assert_cmpstr (b->events->str, ==, "BE");
  begin ((GimpMypaintBrush *) a); end ((GimpMypaintBrush *) a);
  g_assert_cmpstr (a->events->str, ==, "BEBE");
  g_object_unref (a); g_object_unref (b);
}
static void nested_use_preconditions (void)
{
  UseBrush *b = use_new ();
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*impl.use_count > 0*failed*");
  gimp_mypaint_brush_end_use ((GimpMypaintBrush *) b); g_test_assert_expected_messages ();
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*GIMP_IS_MYPAINT_BRUSH*failed*");
  gimp_mypaint_brush_begin_use (NULL); g_test_assert_expected_messages ();
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*GIMP_IS_MYPAINT_BRUSH*failed*");
  gimp_mypaint_brush_end_use (NULL); g_test_assert_expected_messages ();
  gimp_mypaint_brush_begin_use ((GimpMypaintBrush *) b); gimp_mypaint_brush_end_use ((GimpMypaintBrush *) b);
  g_assert_cmpstr (b->events->str, ==, "BE"); g_object_unref (b);
}
static gpointer use_wrong_thread (gpointer data)
{ gimp_mypaint_brush_begin_use (data); gimp_mypaint_brush_end_use (data); return NULL; }
static void nested_use_admission (void)
{
  UseBrush *b = use_new (); GThread *thread;
  thread = g_thread_new ("brush-use", use_wrong_thread, b); g_thread_join (thread);
  g_assert_cmpstr (b->events->str, ==, "");
  gimp_mypaint_brush_begin_use ((GimpMypaintBrush *) b);
  thread = g_thread_new ("brush-use", use_wrong_thread, b); g_thread_join (thread);
  gimp_mypaint_brush_end_use ((GimpMypaintBrush *) b); g_assert_cmpstr (b->events->str, ==, "BE");
  gimp_mypaint_brush_begin_use ((GimpMypaintBrush *) b); g_object_run_dispose (G_OBJECT (b));
  gimp_mypaint_brush_end_use ((GimpMypaintBrush *) b); gimp_mypaint_brush_begin_use ((GimpMypaintBrush *) b);
  g_assert_cmpstr (b->events->str, ==, "BEB"); g_object_unref (b);
}
static void nested_use_reentry (void)
{
  UseBrush *b = use_new (); b->mode = 1;
  gimp_mypaint_brush_begin_use ((GimpMypaintBrush *) b); g_assert_cmpstr (b->events->str, ==, "BEB");
  gimp_mypaint_brush_end_use ((GimpMypaintBrush *) b); g_assert_cmpstr (b->events->str, ==, "BEBE"); g_object_unref (b);
  b = use_new (); b->mode = 2;
  gimp_mypaint_brush_begin_use ((GimpMypaintBrush *) b); b->entered = FALSE;
  gimp_mypaint_brush_end_use ((GimpMypaintBrush *) b); g_assert_cmpstr (b->events->str, ==, "BEB");
  b->mode = 0; gimp_mypaint_brush_end_use ((GimpMypaintBrush *) b); g_object_unref (b);
  b = use_new (); b->mode = 3;
  gimp_mypaint_brush_begin_use ((GimpMypaintBrush *) b); g_assert_cmpstr (b->events->str, ==, "B"); g_object_unref (b);
}
static void nested_use_retained_lifetime (void)
{
  gboolean dead = FALSE; UseBrush *b = use_new (); b->dead = &dead; b->mode = 4;
  gimp_mypaint_brush_begin_use ((GimpMypaintBrush *) b); g_assert_true (dead);
  dead = FALSE; b = use_new (); b->dead = &dead;
  gimp_mypaint_brush_begin_use ((GimpMypaintBrush *) b); b->mode = 5;
  gimp_mypaint_brush_end_use ((GimpMypaintBrush *) b); g_assert_true (dead);
}
static void nested_use_constructor (void)
{
  UseBrush *b; use_constructor_probe = TRUE; b = use_new (); use_constructor_probe = FALSE;
  g_assert_cmpstr (b->events->str, ==, "BE");
  gimp_mypaint_brush_begin_use ((GimpMypaintBrush *) b); gimp_mypaint_brush_end_use ((GimpMypaintBrush *) b);
  g_assert_cmpstr (b->events->str, ==, "BEBE"); g_object_unref (b);
}
