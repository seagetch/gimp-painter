/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <exception>
#include <initializer_list>
#include <memory>
#include <utility>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "config/gimprc.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimp-contexts.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "tools/gimp-tools.h"
#include "gimp-log.h"
#include "gimp-app-test-utils.h"
}
#include "xcf/painter-xcf-arguments.hpp"

namespace {
Gimp *application;
const char *names[] = {"GimpCloneLayer", "GimpFilterLayer", "GimpPerspectiveGuide"};
struct VariantFree { void operator() (GVariant *p) const { g_variant_unref (p); } };
using Variant = std::unique_ptr<GVariant, VariantFree>;

void cold_legacy_arguments ()
{
  for (const char *name : names)
    {
      GVariantBuilder builder;
      g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sv)"));
      g_variant_builder_add (&builder, "(sv)", name, g_variant_new ("(uu)", 0u, 0u));
      Variant input (g_variant_ref_sink (g_variant_builder_end (&builder)));
      try
        {
          auto *args = GimpPainterXcf::decode_arguments (input.get (), nullptr);
          g_assert_cmpuint (gimp_value_array_length (args), ==, 1);
          const GValue *value = gimp_value_array_index (args, 0);
          g_assert_cmpstr (G_VALUE_TYPE_NAME (value), ==, name);
          g_assert_null (g_value_get_object (value));
          gimp_value_array_unref (args);
        }
      catch (const std::exception& error)
        { g_test_message ("cold %s: %s", name, error.what ()); g_test_fail (); }
    }
}

void cold_snapshot_arguments ()
{
  for (const char *name : names)
    {
      GVariantBuilder references, builder;
      g_variant_builder_init (&references, G_VARIANT_TYPE ("a(uuxsbb)"));
      g_variant_builder_add (&references, "(uuxsbb)", 0u, 0u, gint64 (0), name, FALSE, FALSE);
      g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sbv)"));
      g_variant_builder_add (&builder, "(sbv)", name, TRUE, g_variant_builder_end (&references));
      Variant input (g_variant_ref_sink (g_variant_builder_end (&builder)));
      try
        {
          auto *snapshot = GimpPainterXcf::decode_snapshot (input.get (), nullptr);
          g_assert_cmpuint (gimp_filter_arguments_snapshot_count (snapshot), ==, 1);
          g_assert_cmpstr (g_type_name (gimp_filter_arguments_snapshot_type (snapshot, 0)), ==, name);
          g_assert_true (gimp_filter_arguments_snapshot_is_null (snapshot, 0));
          GimpFilterArgumentReference reference = {};
          g_assert_cmpuint (gimp_filter_arguments_snapshot_reference_count (snapshot, 0), ==, 1);
          g_assert_true (gimp_filter_arguments_snapshot_reference (snapshot, 0, 0, &reference));
          g_assert_cmpstr (g_type_name (reference.object_type), ==, name);
          g_assert_false (reference.was_set);
          g_assert_false (reference.expired);
          gimp_filter_arguments_snapshot_free (snapshot);
        }
      catch (const std::exception& error)
        { g_test_message ("cold snapshot %s: %s", name, error.what ()); g_test_fail (); }
    }
}

void factory_and_paint_types ()
{
  for (const char *name : {"GimpToolItem", "GimpPaintInfo", "GimpPainterMybrush",
                           "GimpLayerPreset", "GimpPainterPaintGate", "GimpPainterMybrushOptions",
                           "GimpFillBrush", "GimpFillBrushOptions", "GimpPainterSmudge",
                           "GimpPainterSmudgeOptions"})
    {
      const auto type = g_type_from_name (name);
      g_test_message ("registered before data/config load: %s", name);
      g_assert_cmpuint (type, !=, G_TYPE_INVALID);
      g_assert_true (g_type_is_a (type, G_TYPE_OBJECT));
    }
  g_assert_cmpuint (gimp_container_get_child_type (
    gimp_data_factory_get_container (application->painter_mybrush_factory)), ==,
    g_type_from_name ("GimpPainterMybrush"));
  g_assert_cmpuint (gimp_container_get_child_type (
    gimp_data_factory_get_container (application->layer_preset_factory)), ==,
    g_type_from_name ("GimpLayerPreset"));
  g_assert_true (gimp_get_data_factory (application, g_type_from_name ("GimpPainterMybrush")) ==
                 application->painter_mybrush_factory);
  g_assert_true (gimp_get_data_factory (application, g_type_from_name ("GimpLayerPreset")) ==
                 application->layer_preset_factory);
}

void unknown_name_rejected ()
{
  const char *unknown = "PainterUnknownSavedType08001";
  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sv)"));
  g_variant_builder_add (&builder, "(sv)", unknown, g_variant_new ("(uu)", 0u, 0u));
  Variant input (g_variant_ref_sink (g_variant_builder_end (&builder)));
  bool rejected = false;
  try
    { auto *args = GimpPainterXcf::decode_arguments (input.get (), nullptr); gimp_value_array_unref (args); }
  catch (const std::exception&) { rejected = true; }
  g_assert_true (rejected);
  g_assert_cmpuint (g_type_from_name (unknown), ==, G_TYPE_INVALID);
}

void cold_tool_group_load ()
{
  /* The upstream loader registers its own types before name-based parsing. */
  g_assert_cmpuint (g_type_from_name ("GimpToolGroup"), ==, G_TYPE_INVALID);
  GError *error = nullptr;
  auto *scanner = gimp_scanner_new_string (
    "(file-version 1) (GimpToolGroup \"startup\" (visible yes) (children))", -1, &error);
  g_assert_no_error (error);
  g_assert_true (gimp_tools_deserialize (application, application->tool_item_list, scanner));
  g_assert_no_error (error);
  gimp_scanner_unref (scanner);
  g_assert_cmpint (gimp_container_get_n_children (application->tool_item_list), ==, 1);
  auto *group = gimp_container_get_first_child (application->tool_item_list);
  g_assert_cmpstr (G_OBJECT_TYPE_NAME (group), ==, "GimpToolGroup");
  g_assert_cmpstr (gimp_object_get_name (group), ==, "startup");
}

void resource_config_properties ()
{
  /* Construct the real configuration class without loading profile files. */
  auto *config = GIMP_CONFIG (g_object_new (GIMP_TYPE_RC, "gimp", application, nullptr));
  GError *error = nullptr;
  g_assert_true (gimp_config_deserialize_string (config,
    "(painter-mypaint-brush-path \"/synthetic/brushes\") "
    "(painter-mypaint-brush-path-writable \"/synthetic/user-brushes\") "
    "(layer-presets-path \"/synthetic/presets\") "
    "(layer-presets-path-writable \"/synthetic/user-presets\")", -1, nullptr, &error));
  g_assert_no_error (error);
  for (const auto& property : {std::make_pair ("painter-mypaint-brush-path", "/synthetic/brushes"),
                               std::make_pair ("painter-mypaint-brush-path-writable", "/synthetic/user-brushes"),
                               std::make_pair ("layer-presets-path", "/synthetic/presets"),
                               std::make_pair ("layer-presets-path-writable", "/synthetic/user-presets")})
    {
      gchar *value = nullptr;
      g_object_get (config, property.first, &value, nullptr);
      g_assert_cmpstr (value, ==, property.second);
      g_free (value);
    }
  g_object_unref (config);
}
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp_log_init ();
  gegl_init (nullptr, nullptr);
  /* Do not call a painter get_type() or construct any feature before startup.
   * Literal saved names must work without the normal XCF loader warming them. */
  for (const char *name : names) g_assert_cmpuint (g_type_from_name (name), ==, G_TYPE_INVALID);
  application = gimp_new ("Painter type startup test", nullptr, nullptr,
                          FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE,
                          GIMP_STACK_TRACE_NEVER, GIMP_PDB_COMPAT_OFF);
  g_assert_null (application->config);
  g_assert_false (application->restored);
  g_test_add_func ("/painter-type-startup/cold-legacy-arguments", cold_legacy_arguments);
  g_test_add_func ("/painter-type-startup/cold-snapshot-arguments", cold_snapshot_arguments);
  g_test_add_func ("/painter-type-startup/factory-and-paint-types", factory_and_paint_types);
  g_test_add_func ("/painter-type-startup/unknown-name-rejected", unknown_name_rejected);
  g_test_add_func ("/painter-type-startup/cold-tool-group-load", cold_tool_group_load);
  g_test_add_func ("/painter-type-startup/resource-config-properties", resource_config_properties);
  const int result = g_test_run ();
  /* This fixture stops before config loading. Pair constructed's contexts
   * without running the normal exit handlers that save a loaded profile. */
  gimp_contexts_exit (application);
  g_object_unref (application);
  return result;
}
