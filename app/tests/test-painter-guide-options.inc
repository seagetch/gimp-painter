/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "libgimpconfig/gimpconfig.h"
#include "core/gimpcontainer.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "tools/tools-types.h"
#include "tools/gimpperspectiveguidetool.h"
void gimp_test_guide_options_handles (Gimp *gimp, GimpToolInfo *info);
void gimp_test_guide_register (GimpToolRegisterCallback callback, gpointer data);
static GimpToolInfo *guide_options_info (void) {
  return GIMP_TOOL_INFO (gimp_container_get_child_by_name (gimp->tool_info_list, "gimp-perspective-guide-tool"));
}
static void guide_options_registered_type (void) {
  GimpToolInfo *info = guide_options_info ();
  GType (*entry) (void) = gimp_perspective_guide_options_get_type;
  g_assert_nonnull (info);
  g_assert_cmpstr (g_type_name (info->tool_options_type), ==, "GimpPerspectiveGuideOptions");
  g_assert_cmpuint (entry (), ==, info->tool_options_type);
  g_assert_cmpuint (entry (), ==, entry ());
  g_assert_cmpuint (G_OBJECT_TYPE (info->tool_options), ==, info->tool_options_type);
  g_assert_cmpuint (g_type_parent (entry ()), ==, GIMP_TYPE_TOOL_OPTIONS);
  g_assert_true (GIMP_IS_PERSPECTIVE_GUIDE_OPTIONS (info->tool_options));
  g_assert_false (g_object_is_floating (info->tool_options));
  g_assert_nonnull (info->presets);
}
static void guide_options_inherited_properties (void) {
  GObjectClass *base = g_type_class_ref (GIMP_TYPE_TOOL_OPTIONS);
  GObjectClass *sub = g_type_class_ref (GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS);
  guint nb, ns;
  GParamSpec **bp = g_object_class_list_properties (base, &nb);
  GParamSpec **sp = g_object_class_list_properties (sub, &ns);
  g_assert_cmpuint (nb, ==, ns);
  for (guint i = 0; i < nb; ++i) {
    GParamSpec *p = g_object_class_find_property (sub, bp[i]->name);
    g_assert_true (p == bp[i]);
    g_assert_cmpuint (p->owner_type, !=, GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS);
  }
  g_free (bp); g_free (sp); g_type_class_unref (sub); g_type_class_unref (base);
}
static void guide_options_config_roundtrip (void) {
  GimpToolOptions *one = g_object_new (GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS, "gimp", gimp, NULL);
  GimpToolOptions *two = g_object_new (GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS, "gimp", gimp, NULL);
  GError *error = NULL;
  gimp_context_set_opacity (GIMP_CONTEXT (one), .375);
  gimp_context_set_opacity (GIMP_CONTEXT (two), .875);
  gchar *text = gimp_config_serialize_to_string (GIMP_CONFIG (one), NULL);
  g_assert_nonnull (text);
  g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (two), text, -1, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpfloat (gimp_context_get_opacity (GIMP_CONTEXT (two)), ==, .375);
  gimp_context_set_opacity (GIMP_CONTEXT (two), .625);
  g_assert_cmpfloat (gimp_context_get_opacity (GIMP_CONTEXT (one)), ==, .375);
  g_free (text); g_object_unref (one); g_object_unref (two);
}
static guint guide_options_callback_count;
static void guide_options_callback (GType tool, GType options, GimpToolOptionsGUIFunc gui,
 GimpContextPropMask mask, const gchar *id, const gchar *label, const gchar *tip,
 const gchar *menu, const gchar *accel, const gchar *domain, const gchar *help,
 const gchar *icon, gpointer data) {
  g_assert_true (data == &guide_options_callback_count); ++guide_options_callback_count;
  g_assert_cmpuint (tool, ==, GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL);
  g_assert_cmpuint (options, ==, GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS);
  g_assert_nonnull (gui); g_assert_cmpuint (mask, ==, 0);
  g_assert_cmpstr (id, ==, "gimp-perspective-guide-tool");
  g_assert_nonnull (label); g_assert_nonnull (tip); g_assert_nonnull (menu);
  g_assert_cmpstr (accel, ==, "g"); g_assert_null (domain);
  g_assert_cmpstr (help, ==, "gimp-perspective-guide"); g_assert_nonnull (icon);
}
static void guide_options_registration (void) {
  void (*entry) (GimpToolRegisterCallback, gpointer) = gimp_perspective_guide_tool_register;
  guide_options_callback_count = 0;
  entry (guide_options_callback, &guide_options_callback_count);
  g_assert_cmpuint (guide_options_callback_count, ==, 1);
  gimp_test_guide_register (guide_options_callback, &guide_options_callback_count);
  g_assert_cmpuint (guide_options_callback_count, ==, 2);
}
static void guide_options_handle_contract (void) {
  gimp_test_guide_options_handles (gimp, guide_options_info ());
}
static void register_guide_options_tests (void) {
  g_test_add_func ("/perspective/options/registered-type", guide_options_registered_type);
  g_test_add_func ("/perspective/options/inherited-properties", guide_options_inherited_properties);
  g_test_add_func ("/perspective/options/config-roundtrip", guide_options_config_roundtrip);
  g_test_add_func ("/perspective/options/registration-callback", guide_options_registration);
  g_test_add_func ("/perspective/options/typed-handles", guide_options_handle_contract);
}
