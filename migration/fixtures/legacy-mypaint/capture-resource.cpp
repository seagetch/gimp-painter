/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Link against the rebuilt pinned application's real resource loader/model.
 * No port Resource implementation participates in this capture. */
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
extern "C" {
#include "config.h"
#include <gegl.h>
#include <cairo.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"
#include "app/core/core-types.h"
#include "app/core/gimpmypaintbrush.h"
#include "app/core/gimpmypaintbrush-load.h"
}
#include "app/core/gimpmypaintbrush-private.hpp"
static std::vector<std::string> paths;
static void walk (const std::string& dir)
{
  auto *handle = g_dir_open (dir.c_str (), 0, nullptr); if (!handle) return;
  while (auto *name = g_dir_read_name (handle)) {
    auto path = dir+"/"+name;
    if (g_file_test (path.c_str (), G_FILE_TEST_IS_DIR)) walk (path);
    else if (g_str_has_suffix (name, ".myb")) paths.push_back (path);
  }
  g_dir_close (handle);
}
int main (int argc, char **argv)
{
  if (argc != 2) return 2;
  walk (argv[1]); std::sort (paths.begin (), paths.end ());
  for (const auto& path : paths) {
    GError *error = nullptr;
    auto *list = gimp_mypaint_brush_load (nullptr, path.c_str (), &error);
    if (!list || error) { std::fprintf (stderr, "%s: %s\n", path.c_str (), error ? error->message : "No brush"); return 1; }
    auto *brush = GIMP_MYPAINT_BRUSH (list->data);
    auto *priv = static_cast<GimpMypaintBrushPrivate *> (brush->p);
    std::printf ("MYB B %s\n", path.substr (std::string (argv[1]).size ()+1).c_str ());
    for (int i = 0; i < BRUSH_MAPPING_COUNT; ++i) {
      std::printf ("MYB V %d %a\n", i, double (priv->get_base_value (i)));
      auto *mapping = priv->get_setting (i)->mapping;
      for (int j = 0; j < INPUT_COUNT; ++j) {
        int count = mapping ? mapping->get_n (j) : 0;
        std::printf ("MYB C %d %d %d", i, j, count);
        for (int k = 0; k < count; ++k) {
          float x,y; mapping->get_point (j,k,&x,&y); std::printf (" %a %a", double (x), double (y));
        }
        std::puts ("");
      }
    }
    for (int i = BRUSH_BOOL_BASE; i < BRUSH_BOOL_END; ++i) std::printf ("MYB S %d %d\n", i, priv->get_bool_value (i));
    for (int i = BRUSH_TEXT_BASE; i < BRUSH_TEXT_END; ++i) {
      auto *text = priv->get_text_value (i);
      std::printf ("MYB T %d %d %s\n", i, text != nullptr, text ? text : "");
    }
    std::printf ("MYB N %s\n", priv->get_parent_brush_name ());
    std::printf ("MYB G %s\n", priv->get_group ());
    g_list_free (list); g_object_unref (brush);
  }
  std::fprintf (stderr, "Loaded %zu actual legacy resources\n", paths.size ());
}
