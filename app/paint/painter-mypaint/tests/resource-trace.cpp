/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "resource.hpp"
#include <cstdio>
#include <fstream>
#include <vector>
#include <algorithm>
using GimpPainter::MyPaint::Resource;
static std::vector<std::string> paths;
static void walk (const std::string& dir)
{
  auto *handle = g_dir_open (dir.c_str (), 0, nullptr); if (!handle) throw std::runtime_error (dir);
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
    std::ifstream file (path); std::string data {std::istreambuf_iterator<char> (file), {}};
    const auto file_name = path.substr (path.find_last_of ("/")+1);
    auto r = Resource::decode (data, file_name.substr (0,file_name.size ()-4));
    std::printf ("MYB B %s\n", path.substr (std::string (argv[1]).size ()+1).c_str ());
    for (int i = 0; i < BRUSH_MAPPING_COUNT; ++i) {
      std::printf ("MYB V %d %a\n", i, double (float (r.base_value (i))));
      for (int j = 0; j < INPUT_COUNT; ++j) {
        auto points = r.curve (i,j); std::printf ("MYB C %d %d %zu", i,j,points.size ());
        for (auto p : points) std::printf (" %a %a", double (float (p.x)), double (float (p.y)));
        std::puts ("");
      }
    }
    for (int i = BRUSH_BOOL_BASE; i < BRUSH_BOOL_END; ++i) std::printf ("MYB S %d %d\n", i,r.switch_value (i));
    for (int i = BRUSH_TEXT_BASE; i < BRUSH_TEXT_END; ++i) std::printf ("MYB T %d %d %s\n", i,!r.text_is_null (i),r.text_value (i).c_str ());
    std::printf ("MYB N %s\n", r.parent_brush_name ().c_str ()); std::printf ("MYB G %s\n", r.group ().c_str ());
  }
}
