/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include "libgimpconfig/gimpconfig.h"
#include "display/display-types.h"
#include "display/gimpmodifiersmanager.h"
#include "display/gimppainternavigation.h"
#include "tests.h"

static GdkDevice *pointer;
static GimpModifierAction action (GimpModifiersManager *m,guint button,guint state)
{
  const gchar *description=NULL;
  return gimp_modifiers_manager_get_action(m,pointer,button,state,&description);
}
static void check_defaults (GimpModifiersManager *m)
{
  const guint extra[]={0,GDK_MOD1_MASK,GDK_META_MASK,GDK_SUPER_MASK};
  for(guint i=0;i<G_N_ELEMENTS(extra);++i)
    for(guint ctrl=0;ctrl<2;++ctrl) for(guint shift=0;shift<2;++shift)
      {
        guint state=extra[i]|(ctrl ? GDK_CONTROL_MASK:0)|(shift ? GDK_SHIFT_MASK:0);
        g_assert_cmpint(action(m,2,state),==,gimp_painter_navigation_middle_action(state));
        g_assert_true(gimp_modifiers_manager_uses_painter_defaults(m,pointer,2,state));
      }
}
static void preferences_read_is_nonmutating(void)
{
  GimpModifiersManager *m=gimp_modifiers_manager_new();
  gchar *before=gimp_config_serialize_to_string(GIMP_CONFIG(m),NULL),*after;
  GList *states=gimp_modifiers_manager_get_modifiers(m,pointer,2);
  g_assert_cmpuint(g_list_length(states),==,4);g_list_free(states);
  check_defaults(m);
  after=gimp_config_serialize_to_string(GIMP_CONFIG(m),NULL);
  g_assert_cmpstr(before,==,after);g_free(before);g_free(after);g_object_unref(m);
}
static void partial_override_roundtrip(void)
{
  GimpModifiersManager *m=gimp_modifiers_manager_new(),*copy=gimp_modifiers_manager_new();
  GError *error=NULL;
  gchar *text;
  gimp_modifiers_manager_set(m,pointer,2,GDK_MOD1_MASK,GIMP_MODIFIER_ACTION_STEP_ROTATING,NULL);
  g_assert_cmpint(action(m,2,GDK_MOD1_MASK),==,GIMP_MODIFIER_ACTION_STEP_ROTATING);
  g_assert_false(gimp_modifiers_manager_uses_painter_defaults(m,pointer,2,GDK_MOD1_MASK));
  g_assert_cmpint(action(m,2,GDK_SHIFT_MASK|GDK_MOD1_MASK),==,GIMP_MODIFIER_ACTION_ROTATING);
  g_assert_true(gimp_modifiers_manager_uses_painter_defaults(m,pointer,2,GDK_SHIFT_MASK|GDK_MOD1_MASK));
  gimp_modifiers_manager_remove(m,pointer,2,GDK_CONTROL_MASK|GDK_MOD1_MASK);
  g_assert_cmpint(action(m,2,GDK_CONTROL_MASK|GDK_MOD1_MASK),==,GIMP_MODIFIER_ACTION_NONE);
  text=gimp_config_serialize_to_string(GIMP_CONFIG(m),NULL);
  g_assert_nonnull(strstr(text,"painter-defaults"));
  g_assert_true(gimp_config_deserialize_string(GIMP_CONFIG(copy),text,-1,NULL,&error));g_assert_no_error(error);
  for(guint state=0;state<16;++state)
    {
      g_assert_cmpint(action(m,2,state),==,action(copy,2,state));
      g_assert_cmpint(gimp_modifiers_manager_uses_painter_defaults(m,pointer,2,state),==,
                      gimp_modifiers_manager_uses_painter_defaults(copy,pointer,2,state));
    }
  g_free(text);g_object_unref(copy);g_object_unref(m);
}
static void existing_exact_config_stays_exact(void)
{
  /* Existing upstream configurations lack the opt-in inheritance marker. */
  const gchar *text="(mapping \"(no-vendor-id):(no-product-id)-2-8\" (modifiers 8) (mod-action step-rotating))";
  GimpModifiersManager *m=gimp_modifiers_manager_new();GError *error=NULL;
  g_assert_true(gimp_config_deserialize_string(GIMP_CONFIG(m),text,-1,NULL,&error));g_assert_no_error(error);
  g_assert_cmpint(action(m,2,GDK_MOD1_MASK),==,GIMP_MODIFIER_ACTION_STEP_ROTATING);
  g_assert_cmpint(action(m,2,GDK_SHIFT_MASK),==,GIMP_MODIFIER_ACTION_NONE);
  g_assert_false(gimp_modifiers_manager_uses_painter_defaults(m,pointer,2,GDK_MOD1_MASK));
  g_object_unref(m);
}
static void explicit_rotation_any_button(void)
{
  GimpModifiersManager *m=gimp_modifiers_manager_new();
  const guint buttons[]={2,3,4,5,8};
  for(guint i=0;i<G_N_ELEMENTS(buttons);++i)
    {
      gimp_modifiers_manager_set(m,pointer,buttons[i],GDK_MOD1_MASK,GIMP_MODIFIER_ACTION_STEP_ROTATING,NULL);
      g_assert_cmpint(action(m,buttons[i],GDK_MOD1_MASK),==,GIMP_MODIFIER_ACTION_STEP_ROTATING);
      g_assert_false(gimp_modifiers_manager_uses_painter_defaults(m,pointer,buttons[i],GDK_MOD1_MASK));
      gimp_modifiers_manager_set(m,pointer,buttons[i],GDK_CONTROL_MASK,GIMP_MODIFIER_ACTION_ROTATING,NULL);
      g_assert_cmpint(action(m,buttons[i],GDK_CONTROL_MASK),==,GIMP_MODIFIER_ACTION_ROTATING);
    }
  g_object_unref(m);
}
int main(int argc,char **argv)
{
  g_test_init(&argc,&argv,NULL);
  if(!gtk_init_check(&argc,&argv)) return GIMP_EXIT_TEST_SKIPPED;
  pointer=gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_display_get_default()));
  g_test_add_func("/navigation-ui/preferences-read",preferences_read_is_nonmutating);
  g_test_add_func("/navigation-ui/partial-override-roundtrip",partial_override_roundtrip);
  g_test_add_func("/navigation-ui/existing-exact-config",existing_exact_config_stays_exact);
  g_test_add_func("/navigation-ui/explicit-any-button",explicit_rotation_any_button);
  return g_test_run();
}
