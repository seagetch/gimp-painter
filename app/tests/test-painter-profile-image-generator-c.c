/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bounded saved-profile controls for the absent ImageGenerator placeholder.
 * Compile this translation unit as C and register it from the native profile
 * test. These controls exercise the real public C entries and native loaders;
 * they do not implement ImageGenerator or close 31.011, 31.012, or 08.018.
 *
 * The real legacy writer fixture contains no ImageGenerator option fields.
 * The old rate/eraser property installation was inside #if 0. Values below
 * that use those names are expressly synthetic unknown-data controls, never
 * evidence of an old working generator or of native option compatibility.
 */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <string.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimppainterprofile.h"
#include "core/gimptoolgroup.h"
#include "core/gimptoolinfo.h"
#include "core/gimptoolpreset.h"
#include "tools/tools-types.h"
#include "tools/gimp-tools.h"

#define IMAGE_GENERATOR "gimp-image-generator-tool"
#define IMAGE_GENERATOR_OPTIONS "tool-options/" IMAGE_GENERATOR

void gimp_test_painter_profile_image_generator_register (Gimp *gimp);

static gchar *
image_generator_read (const gchar *root,
                      const gchar *relative)
{
  gchar  *path = g_build_filename (root, relative, NULL);
  gchar  *text = NULL;
  GError *error = NULL;

  g_assert_true (g_file_get_contents (path, &text, NULL, &error));
  g_assert_no_error (error);
  g_free (path);
  return text;
}

static void
image_generator_write (const gchar *root,
                       const gchar *relative,
                       const gchar *text)
{
  gchar  *path = g_build_filename (root, relative, NULL);
  gchar  *parent = g_path_get_dirname (path);
  GError *error = NULL;

  g_assert_cmpint (g_mkdir_with_parents (parent, 0700), ==, 0);
  g_assert_true (g_file_set_contents (path, text, -1, &error));
  g_assert_no_error (error);
  g_free (parent);
  g_free (path);
}

static void
image_generator_assert_file (const gchar *root,
                             const gchar *relative,
                             const gchar *expected)
{
  gchar *text = image_generator_read (root, relative);

  g_assert_cmpstr (text, ==, expected);
  g_free (text);
}

static void
image_generator_assert_archive (const gchar *destination,
                                const gchar *relative,
                                const gchar *original)
{
  gchar *digest = g_compute_checksum_for_string (G_CHECKSUM_SHA256,
                                                  original, -1);
  gchar *archive = g_build_filename ("painter-migration", "originals",
                                     digest, relative, NULL);

  image_generator_assert_file (destination, archive, original);
  g_free (archive);
  g_free (digest);
}

static void
image_generator_remove_tree (const gchar *path)
{
  GDir        *directory;
  const gchar *name;

  if (g_file_test (path, G_FILE_TEST_IS_SYMLINK))
    {
      g_assert_cmpint (g_unlink (path), ==, 0);
      return;
    }

  directory = g_dir_open (path, 0, NULL);
  if (! directory)
    {
      g_assert_cmpint (g_unlink (path), ==, 0);
      return;
    }

  while ((name = g_dir_read_name (directory)))
    {
      gchar *child = g_build_filename (path, name, NULL);

      image_generator_remove_tree (child);
      g_free (child);
    }

  g_dir_close (directory);
  g_assert_cmpint (g_rmdir (path), ==, 0);
}

static gchar *
image_generator_fixture (const gchar *relative,
                         const gchar *expected_sha256)
{
  const gchar *top = g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR");
  gchar       *root;
  gchar       *text;
  gchar       *digest;

  g_assert_nonnull (top);
  root = g_build_filename (top, "migration", "fixtures", "legacy-profile",
                            "profile", NULL);
  text = image_generator_read (root, relative);
  digest = g_compute_checksum_for_string (G_CHECKSUM_SHA256, text, -1);
  /* Hashes are from the committed legacy-profile/manifest.json. */
  g_assert_cmpstr (digest, ==, expected_sha256);
  g_free (digest);
  g_free (root);
  return text;
}

static gchar *
image_generator_save_tools (Gimp *gimp)
{
  GString          *buffer = g_string_new (NULL);
  GimpConfigWriter *writer = gimp_config_writer_new_from_string (buffer);
  GError           *error = NULL;

  g_assert_true (gimp_tools_serialize (gimp, gimp->tool_item_list, writer));
  g_assert_true (gimp_config_writer_finish (writer, NULL, &error));
  g_assert_no_error (error);
  return g_string_free (buffer, FALSE);
}

static void
image_generator_load_tools (Gimp        *gimp,
                            const gchar *text)
{
  GError   *error = NULL;
  GScanner *scanner = gimp_scanner_new_string (text, -1, &error);

  g_assert_no_error (error);
  g_assert_nonnull (scanner);
  g_assert_true (gimp_tools_deserialize (gimp, gimp->tool_item_list, scanner));
  g_assert_no_error (error);
  gimp_scanner_unref (scanner);
}

static void
image_generator_assert_absent (Gimp *gimp)
{
  g_assert_null (gimp_get_tool_info (gimp, IMAGE_GENERATOR));
  g_assert_cmpuint (g_type_from_name ("GimpImageGeneratorOptions"), ==, 0);
  g_assert_cmpuint (g_type_from_name ("GimpImageGeneratorTool"), ==, 0);
}

static void
image_generator_genuine_writer (gconstpointer data)
{
  Gimp          *gimp = GIMP ((gpointer) data);
  GError        *error = NULL;
  gchar         *root = g_dir_make_tmp ("painter-image-generator-writer-XXXXXX", &error);
  gchar         *source;
  gchar         *destination;
  gchar         *old_tools;
  gchar         *old_options;
  gchar         *installed;
  gchar         *roundtrip;
  gchar         *saved_tools;
  GimpToolGroup *group;

  g_assert_no_error (error);
  g_assert_nonnull (root);
  source = g_build_filename (root, "old", NULL);
  destination = g_build_filename (root, "new", NULL);
  old_tools = image_generator_fixture ("toolrc",
    "7a25268ddf2e535147a24d9c3be516132d3a3897ad6a870435a110a316eb67c2");
  old_options = image_generator_fixture (IMAGE_GENERATOR_OPTIONS,
    "045fb2af8afa00b4f44b37589f1d8c4f4c49eeb8550eb32bc8631e1f7d520cbd");
  g_assert_nonnull (strstr (old_tools, "(GimpToolInfo \"" IMAGE_GENERATOR "\""));
  g_assert_null (strchr (old_options, '('));
  image_generator_write (source, "toolrc", old_tools);
  image_generator_write (source, IMAGE_GENERATOR_OPTIONS, old_options);
  g_assert_true (gimp_painter_profile_detect (source));
  g_assert_true (gimp_painter_profile_migrate (source, destination, &error));
  g_assert_no_error (error);
  image_generator_assert_archive (destination, "toolrc", old_tools);
  image_generator_assert_archive (destination, IMAGE_GENERATOR_OPTIONS, old_options);
  image_generator_assert_file (source, "toolrc", old_tools);
  image_generator_assert_file (source, IMAGE_GENERATOR_OPTIONS, old_options);

  installed = image_generator_read (destination, IMAGE_GENERATOR_OPTIONS);
  g_assert_true (g_str_has_prefix (installed, old_options));
  g_assert_nonnull (strstr (installed, "(paint-mode painter-normal)"));
  g_assert_null (strstr (installed, "(rate "));
  g_assert_null (strstr (installed, "(eraser-mode "));
  g_free (installed);

  gimp_set_focused_once (gimp);
  saved_tools = image_generator_save_tools (gimp);
  installed = image_generator_read (destination, "toolrc");
  g_assert_nonnull (strstr (installed, IMAGE_GENERATOR));
  image_generator_load_tools (gimp, installed);
  image_generator_assert_absent (gimp);
  group = GIMP_TOOL_GROUP (gimp_container_get_first_child (gimp->tool_item_list));
  g_assert_true (GIMP_IS_TOOL_GROUP (group));
  g_assert_cmpstr (gimp_tool_group_get_active_tool (group), ==, "gimp-rect-select-tool");
  roundtrip = image_generator_save_tools (gimp);
  g_assert_null (strstr (roundtrip, IMAGE_GENERATOR));
  g_assert_nonnull (strstr (roundtrip, "gimp-painter-mypaint-tool"));
  image_generator_load_tools (gimp, roundtrip);
  image_generator_load_tools (gimp, saved_tools);
  /* Runtime omission must never be confused with loss of the saved source. */
  image_generator_assert_archive (destination, "toolrc", old_tools);

  g_free (roundtrip);
  g_free (saved_tools);
  g_free (installed);
  g_free (old_options);
  g_free (old_tools);
  image_generator_remove_tree (root);
  g_free (destination);
  g_free (source);
  g_free (root);
}

static void
image_generator_missing_selection (gconstpointer data)
{
  /* Authored input: the genuine fixture does not select the placeholder. */
  const gchar *old_tools =
    "(file-version 1)\n"
    "(GimpToolGroup \"placeholder selection\" (visible yes) (expanded no)\n"
    " (active-tool \"" IMAGE_GENERATOR "\") (children\n"
    "  (GimpToolInfo \"" IMAGE_GENERATOR "\" (visible no))\n"
    "  (GimpToolInfo \"gimp-mypaint-tool\" (visible yes))\n"
    "  (GimpToolInfo \"gimp-smudge-tool\" (visible no))))\n";
  const gchar *old_context = "(tool \"" IMAGE_GENERATOR "\")\n";
  Gimp          *gimp = GIMP ((gpointer) data);
  GError        *error = NULL;
  gchar         *root = g_dir_make_tmp ("painter-image-generator-selection-XXXXXX", &error);
  gchar         *source;
  gchar         *destination;
  gchar         *installed;
  gchar         *saved_tools;
  gchar         *roundtrip;
  GimpToolGroup *group;
  GimpContainer *children;
  GimpContext   *context;
  GimpContext   *user_context;
  GimpToolInfo  *saved_tool;
  GimpToolInfo  *outside_tool;
  GimpToolInfo  *smudge_tool;

  g_assert_no_error (error);
  g_assert_nonnull (root);
  source = g_build_filename (root, "old", NULL);
  destination = g_build_filename (root, "new", NULL);
  image_generator_write (source, "toolrc", old_tools);
  image_generator_write (source, "contextrc", old_context);
  g_assert_true (gimp_painter_profile_migrate (source, destination, &error));
  g_assert_no_error (error);
  image_generator_assert_archive (destination, "toolrc", old_tools);
  image_generator_assert_archive (destination, "contextrc", old_context);

  gimp_set_focused_once (gimp);
  saved_tools = image_generator_save_tools (gimp);
  user_context = gimp_get_user_context (gimp);
  saved_tool = gimp_context_get_tool (user_context);
  g_assert_nonnull (saved_tool);
  g_object_ref (saved_tool);
  outside_tool = gimp_get_tool_info (gimp, "gimp-rect-select-tool");
  smudge_tool = gimp_get_tool_info (gimp, "gimp-painter-smudge-tool");
  g_assert_nonnull (outside_tool);
  g_assert_nonnull (smudge_tool);
  /* The native tool manager listens to ancestry-changed on the current tool.
   * When that tool joins a group, it becomes active even if another child was
   * inserted first. Keep the selected tool outside this group to isolate the
   * first-valid-child fallback from that intentional current-tool preference.
   * The earlier installer test may have left Painter Smudge selected. */
  gimp_context_set_tool (user_context, outside_tool);
  g_assert_true (gimp_context_get_tool (user_context) == outside_tool);
  installed = image_generator_read (destination, "toolrc");
  image_generator_load_tools (gimp, installed);
  g_assert_true (gimp_context_get_tool (user_context) == outside_tool);
  group = GIMP_TOOL_GROUP (gimp_container_get_first_child (gimp->tool_item_list));
  g_assert_true (GIMP_IS_TOOL_GROUP (group));
  g_assert_cmpstr (gimp_object_get_name (group), ==, "placeholder selection");
  children = gimp_viewable_get_children (GIMP_VIEWABLE (group));
  g_assert_cmpint (gimp_container_get_n_children (children), ==, 2);
  g_assert_null (gimp_container_get_child_by_name (children, IMAGE_GENERATOR));
  g_assert_cmpstr (gimp_object_get_name (gimp_container_get_first_child (children)),
                   ==, "gimp-painter-mypaint-tool");
  g_assert_cmpstr (gimp_tool_group_get_active_tool (group), ==, "gimp-painter-mypaint-tool");
  g_assert_false (gimp_viewable_get_expanded (GIMP_VIEWABLE (group)));
  g_assert_false (gimp_tool_item_get_visible (GIMP_TOOL_ITEM (
    gimp_container_get_child_by_name (children, "gimp-painter-smudge-tool"))));
  roundtrip = image_generator_save_tools (gimp);
  g_assert_null (strstr (roundtrip, IMAGE_GENERATOR));
  image_generator_load_tools (gimp, roundtrip);
  group = GIMP_TOOL_GROUP (gimp_container_get_first_child (gimp->tool_item_list));
  g_assert_cmpstr (gimp_tool_group_get_active_tool (group), ==, "gimp-painter-mypaint-tool");
  g_assert_false (gimp_viewable_get_expanded (GIMP_VIEWABLE (group)));
  g_free (roundtrip);

  /* Now deliberately exercise the other precise contract: a selected valid
   * later child wins through the real tool-manager ancestry callback when the
   * same migrated source still names the absent placeholder as active. */
  gimp_context_set_tool (user_context, smudge_tool);
  g_assert_true (gimp_context_get_tool (user_context) == smudge_tool);
  image_generator_load_tools (gimp, installed);
  g_free (installed);
  group = GIMP_TOOL_GROUP (gimp_container_get_first_child (gimp->tool_item_list));
  children = gimp_viewable_get_children (GIMP_VIEWABLE (group));
  g_assert_cmpstr (gimp_object_get_name (gimp_container_get_first_child (children)),
                   ==, "gimp-painter-mypaint-tool");
  g_assert_cmpstr (gimp_tool_group_get_active_tool (group), ==, "gimp-painter-smudge-tool");
  g_assert_true (gimp_context_get_tool (user_context) == smudge_tool);
  g_assert_false (gimp_viewable_get_expanded (GIMP_VIEWABLE (group)));
  roundtrip = image_generator_save_tools (gimp);
  g_assert_null (strstr (roundtrip, IMAGE_GENERATOR));
  image_generator_load_tools (gimp, roundtrip);
  group = GIMP_TOOL_GROUP (gimp_container_get_first_child (gimp->tool_item_list));
  g_assert_cmpstr (gimp_tool_group_get_active_tool (group), ==, "gimp-painter-smudge-tool");
  g_assert_false (gimp_viewable_get_expanded (GIMP_VIEWABLE (group)));

  installed = image_generator_read (destination, "contextrc");
  g_assert_nonnull (strstr (installed, IMAGE_GENERATOR));
  context = gimp_context_new (gimp, "image-generator-missing-selection", NULL);
  g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (context), installed,
                                                -1, NULL, &error));
  g_assert_no_error (error);
  g_assert_nonnull (gimp_tool_info_get_standard (gimp));
  g_assert_true (gimp_context_get_tool (context) == gimp_tool_info_get_standard (gimp));
  image_generator_assert_absent (gimp);
  g_object_unref (context);
  /* Restore the selected context before restoring its original group layout. */
  gimp_context_set_tool (user_context, saved_tool);
  image_generator_load_tools (gimp, saved_tools);
  g_assert_true (gimp_context_get_tool (user_context) == saved_tool);
  g_object_unref (saved_tool);
  image_generator_assert_archive (destination, "contextrc", old_context);

  g_free (installed);
  g_free (roundtrip);
  g_free (saved_tools);
  image_generator_remove_tree (root);
  g_free (destination);
  g_free (source);
  g_free (root);
}

static void
image_generator_unknown_saved_data (gconstpointer data)
{
  const gchar *unproven_tools =
    "(file-version 1) (GimpToolInfo \"" IMAGE_GENERATOR "\" (visible no))\n";
  const gchar *proven_tools =
    "(file-version 1) (GimpToolInfo \"gimp-mypaint-tool\" (visible yes))\n"
    "(GimpToolInfo \"" IMAGE_GENERATOR "\" (visible no))\n";
  /* Synthetic unknown data; rate/eraser were not installed legacy properties. */
  const gchar *options =
    "# authored unknown-data control\n(rate 37) (eraser-mode yes)\n"
    "(future-generator (tool \"gimp-smudge-tool\") (payload \"opaque\"))\n";
  const gchar *preset =
    "(name \"Authored placeholder preset\")\n"
    "(tool-options \"GimpImageGeneratorOptions\"\n"
    " (tool \"" IMAGE_GENERATOR "\") (rate 37) (eraser-mode yes)\n"
    " (future-generator (payload \"opaque\")))\n";
  const gchar *options_edit = "# destination edit\n(rate 82)\n";
  const gchar *preset_edit = "# destination preset edit\n(name \"Keep me\")\n";
  const gchar *changed_options = "# changed source\n(future-generator (payload 2))\n";
  Gimp           *gimp = GIMP ((gpointer) data);
  GError         *error = NULL;
  gchar          *root = g_dir_make_tmp ("painter-image-generator-unknown-XXXXXX", &error);
  gchar          *source;
  gchar          *destination;
  gchar          *installed;
  gchar          *active_path;
  GimpToolPreset *native_preset;

  g_assert_no_error (error);
  g_assert_nonnull (root);
  source = g_build_filename (root, "old", NULL);
  destination = g_build_filename (root, "new", NULL);
  image_generator_write (source, "toolrc", unproven_tools);
  image_generator_write (source, IMAGE_GENERATOR_OPTIONS, options);
  image_generator_write (source, "tool-presets/image-generator.gtp", preset);
  /* A placeholder reference and invented property values do not prove origin. */
  g_assert_false (gimp_painter_profile_detect (source));
  g_assert_false (gimp_painter_profile_migrate (source, destination, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_clear_error (&error);
  g_assert_true (gimp_painter_profile_preserve (source, destination, &error));
  g_assert_no_error (error);
  image_generator_assert_archive (destination, "toolrc", unproven_tools);
  image_generator_assert_archive (destination, IMAGE_GENERATOR_OPTIONS, options);
  image_generator_assert_archive (destination, "tool-presets/image-generator.gtp", preset);
  active_path = g_build_filename (destination, IMAGE_GENERATOR_OPTIONS, NULL);
  g_assert_false (g_file_test (active_path, G_FILE_TEST_EXISTS));
  g_free (active_path);

  image_generator_write (source, "toolrc", proven_tools);
  g_assert_true (gimp_painter_profile_detect (source));
  g_assert_true (gimp_painter_profile_migrate (source, destination, &error));
  g_assert_no_error (error);
  installed = image_generator_read (destination, IMAGE_GENERATOR_OPTIONS);
  g_assert_true (g_str_has_prefix (installed, options));
  g_assert_nonnull (strstr (installed, "(paint-mode painter-normal)"));
  g_free (installed);
  installed = image_generator_read (destination, "tool-presets/image-generator.gtp");
  g_assert_nonnull (strstr (installed, "GimpImageGeneratorOptions"));
  g_assert_nonnull (strstr (installed, "(tool \"" IMAGE_GENERATOR "\")"));
  g_assert_nonnull (strstr (installed, "(rate 37) (eraser-mode yes)"));
  g_assert_nonnull (strstr (installed, "(future-generator (payload \"opaque\"))"));
  image_generator_assert_absent (gimp);
  native_preset = g_object_new (GIMP_TYPE_TOOL_PRESET, "gimp", gimp, NULL);
  /* Explicit bounded rejection, not a fabricated native options replacement. */
  g_assert_false (gimp_config_deserialize_string (GIMP_CONFIG (native_preset),
                                                 installed, -1, NULL, &error));
  g_assert_error (error, GIMP_CONFIG_ERROR, GIMP_CONFIG_ERROR_PARSE);
  g_assert_nonnull (strstr (error->message, "GimpImageGeneratorOptions"));
  g_assert_null (native_preset->tool_options);
  g_clear_error (&error);
  g_object_unref (native_preset);
  g_free (installed);

  image_generator_write (destination, IMAGE_GENERATOR_OPTIONS, options_edit);
  image_generator_write (destination, "tool-presets/image-generator.gtp", preset_edit);
  image_generator_write (source, IMAGE_GENERATOR_OPTIONS, changed_options);
  g_assert_true (gimp_painter_profile_migrate (source, destination, &error));
  g_assert_no_error (error);
  image_generator_assert_file (destination, IMAGE_GENERATOR_OPTIONS, options_edit);
  image_generator_assert_file (destination, "tool-presets/image-generator.gtp", preset_edit);
  image_generator_assert_file (source, IMAGE_GENERATOR_OPTIONS, changed_options);
  image_generator_assert_file (source, "tool-presets/image-generator.gtp", preset);
  image_generator_assert_archive (destination, IMAGE_GENERATOR_OPTIONS, options);
  image_generator_assert_archive (destination, IMAGE_GENERATOR_OPTIONS, changed_options);
  image_generator_assert_archive (destination, "tool-presets/image-generator.gtp", preset);

  image_generator_remove_tree (root);
  g_free (destination);
  g_free (source);
  g_free (root);
}

void
gimp_test_painter_profile_image_generator_register (Gimp *gimp)
{
  g_test_add_data_func ("/painter-profile-native/image-generator/genuine-writer",
                        gimp, image_generator_genuine_writer);
  g_test_add_data_func ("/painter-profile-native/image-generator/missing-selection",
                        gimp, image_generator_missing_selection);
  g_test_add_data_func ("/painter-profile-native/image-generator/unknown-saved-data",
                        gimp, image_generator_unknown_saved_data);
}
