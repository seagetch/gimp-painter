/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Shared by the native runtime and the immutable-old-header negative control.
 * Every expression calls the real C implementation. No GTK initialization. */
static void
painter_check_navigation (void)
{
  const GdkModifierType no_modifiers = static_cast<GdkModifierType> (0);
  g_assert_cmpfloat_with_epsilon (
    gimp_painter_navigation_begin (100, 100, 50, 50, 30.0, FALSE, FALSE),
    30.0, 1e-12);
  g_assert_cmpfloat_with_epsilon (
    gimp_painter_navigation_rotate (100, 100, 50, 50, 30.0, FALSE, FALSE,
                                     no_modifiers), 30.0, 1e-12);
  g_assert_cmpfloat_with_epsilon (
    gimp_painter_navigation_zoom_begin (100, 100, 50, 50, 1.0, 2.0),
    600.0, 1e-12);
  g_assert_cmpfloat_with_epsilon (
    gimp_painter_navigation_zoom (100, 100, 50, 50, 600.0), 2.0, 1e-12);
  g_assert_cmpint (gimp_painter_navigation_middle_action (no_modifiers),
                   ==, GIMP_MODIFIER_ACTION_PANNING);
  g_assert_cmpint (gimp_painter_navigation_rotation_action (
                    GIMP_MODIFIER_ACTION_ROTATING, TRUE, GDK_CONTROL_MASK),
                   ==, GIMP_MODIFIER_ACTION_STEP_ROTATING);
  g_assert_cmpuint (gimp_painter_navigation_key (GDK_KEY_Left, TRUE, TRUE),
                    ==, GDK_KEY_Right);
}
