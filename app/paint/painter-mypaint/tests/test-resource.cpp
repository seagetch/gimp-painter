/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "resource.hpp"
#include "engine.hpp"
#include <glib.h>
#include <json-glib/json-glib.h>
#include <cmath>
#include <fstream>
#include <sstream>
using namespace GimpPainter::MyPaint;
static const char *corpus;
static std::string read (const std::string& path)
{ std::ifstream file (path); g_assert_true (file.good ()); return {std::istreambuf_iterator<char> (file), {}}; }
static void equal_semantics (const Resource& a, const Resource& b)
{
  for (int i = 0; i < BRUSH_MAPPING_COUNT; ++i) {
    g_assert_cmpfloat (float (a.base_value (i)), ==, float (b.base_value (i)));
    for (int j = 0; j < INPUT_COUNT; ++j) {
      auto ac = a.curve (i,j), bc = b.curve (i,j); g_assert_cmpuint (ac.size (), ==, bc.size ());
      for (unsigned k = 0; k < ac.size (); ++k) {
        g_assert_cmpfloat (float (ac[k].x), ==, float (bc[k].x)); g_assert_cmpfloat (float (ac[k].y), ==, float (bc[k].y));
      }
    }
  }
  for (int i = BRUSH_BOOL_BASE; i < BRUSH_BOOL_END; ++i) g_assert_true (a.switch_value (i) == b.switch_value (i));
  for (int i = BRUSH_TEXT_BASE; i < BRUSH_TEXT_END; ++i) {
    g_assert_true (a.text_is_null (i) == b.text_is_null (i)); g_assert_true (a.text_value (i) == b.text_value (i));
  }
  g_assert_true (a.parent_brush_name () == b.parent_brush_name ()); g_assert_true (a.group () == b.group ());
}
static void walk (const std::string& path, unsigned& v2, unsigned& v3, unsigned& ignored)
{
  GDir *dir = g_dir_open (path.c_str (), 0, nullptr); g_assert_nonnull (dir);
  while (const auto *name = g_dir_read_name (dir)) {
    auto file = path+"/"+name;
    if (g_file_test (file.c_str (), G_FILE_TEST_IS_DIR)) walk (file, v2, v3, ignored);
    else if (g_str_has_suffix (name, ".myb")) {
      auto r = Resource::decode (read (file));
      if (r.source_version () == 2) ++v2; else if (r.source_version () == 3) ++v3; else g_assert_not_reached ();
      equal_semantics (r, Resource::decode (r.encode ()));
      for (const auto& diagnostic : r.diagnostics ()) {
        if (diagnostic.kind == Diagnostic::Kind::Unsupported) g_error ("%s: %s", file.c_str (), diagnostic.path.c_str ());
        ++ignored;
      }
      Engine engine (r); // actual extended evaluator configuration, not only JSON syntax
    }
  }
  g_dir_close (dir);
}
static void test_corpus ()
{
  unsigned v2 = 0, v3 = 0, ignored = 0; walk (corpus, v2, v3, ignored);
  g_assert_cmpuint (v2, ==, 1); g_assert_cmpuint (v3, ==, 176); g_assert_cmpuint (ignored, ==, 221);
  g_test_message ("177 pinned brushes loaded/configured/round-tripped; 221 legacy-ignored fields retained");
}
static void test_defaults ()
{
  auto r = Resource::decode ("{\"version\":3}");
  g_assert_false (r.has_member ("switches", "use_gimp_brushmark"));
  g_assert_false (r.switch_value (BRUSH_USE_GIMP_BRUSHMARK));
  g_assert_cmpuint (r.curve (BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE).size (), ==, 2);
  r.set_switch (BRUSH_USE_GIMP_BRUSHMARK, false); g_assert_true (r.has_member ("switches", "use_gimp_brushmark"));
  r.set_curve (BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE, {});
  g_assert_true (Resource::decode (r.encode ()).curve (BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE).empty ());
  g_assert_true (r.text_is_null (BRUSH_BRUSHMARK_NAME)); r.set_text (BRUSH_BRUSHMARK_NAME, "");
  g_assert_false (r.text_is_null (BRUSH_BRUSHMARK_NAME));
}
static void test_unknown ()
{
  const std::string json = R"({"version":3,"vendor":{"nested":[true,null,{"x":7.5}]},"settings":{"hardness":{"base_value":0.7,"note":"keep","inputs":{"future":[[0,0],[1,2]]}},"future_setting":{"opaque":[7,8]}},"switches":{"future":true},"texts":{"future":"abc"}})";
  auto r = Resource::decode (json); auto copy = r;
  copy.set_base_value (BRUSH_HARDNESS, .6); copy.set_curve (BRUSH_HARDNESS, INPUT_PRESSURE, {{0,0},{1,1}});
  g_assert_cmpfloat (r.base_value (BRUSH_HARDNESS), ==, .7);
  auto encoded = copy.encode ();
  for (const auto *key : {"vendor", "nested", "note", "keep", "future_setting", "future", "abc"}) g_assert_nonnull (strstr (encoded.c_str (), key));
  g_assert_cmpuint (r.diagnostics ().size (), ==, 4);
  bool refused = false; try { Engine engine (r); } catch (const std::invalid_argument&) { refused = true; }
  g_assert_true (refused);
}
static void test_legacy_curves ()
{
  auto r = Resource::decode ("version 1\nparent_brush_name hello%20world\ngroup x%20y\ncolor_hue 360 | pressure 0 0 1 360\ncolor_saturation 256 | pressure 0 0 1 256\ncolor 255 0 0\n");
  g_assert_cmpfloat (r.base_value (BRUSH_CHANGE_COLOR_H), ==, 64);
  g_assert_cmpfloat (r.curve (BRUSH_CHANGE_COLOR_H, INPUT_PRESSURE)[1].y, ==, 64);
  g_assert_cmpfloat (r.base_value (BRUSH_CHANGE_COLOR_HSV_S), ==, 128);
  g_assert_cmpfloat (r.base_value (BRUSH_COLOR_V), ==, 1);
  g_assert_true (r.parent_brush_name () == "hello world"); g_assert_true (r.group () == "x%20y");
  equal_semantics (r, Resource::decode (r.encode ()));
  for (const auto *bad : {"0 0 1", "0 0", "1 0 0 1", "0 NaN 1 2"}) {
    bool caught = false;
    try { Resource::decode (std::string ("version 1\nhardness 1 | pressure ")+bad+"\n"); }
    catch (const std::invalid_argument&) { caught = true; }
    g_assert_true (caught);
  }
}
static void test_legacy_unknown_input ()
{
  for (const auto *text : {"version 1\nopaque 1 | future_input 0 0 1 1\n",
                          "version 2\nopaque 1 | future_input (0 0), (1 1)\n",
                          "version 1\nopaque 1 | speed 0 0 1 1\n"}) {
    auto r = Resource::decode (text);
    auto diagnostics = r.diagnostics ();
    g_assert_cmpuint (diagnostics.size (), ==, 1);
    g_assert_true (diagnostics[0].kind == Diagnostic::Kind::Unsupported);
    const char *input_name = strstr(text,"future_input") ? "future_input" : "speed";
    g_assert_true (diagnostics[0].path == std::string("settings.opaque.inputs.")+input_name);
    auto saved = r.encode ();
    g_assert_nonnull (strstr (saved.c_str (), input_name));
    g_assert_nonnull (strstr (saved.c_str (), "painter_legacy"));
    auto roundtrip = Resource::decode (saved);
    g_assert_cmpuint (roundtrip.diagnostics ().size (), ==, 1);
    for (const auto *resource : {&r, &roundtrip}) {
      bool caught = false;
      try { Engine engine (*resource); } catch (const std::invalid_argument&) { caught = true; }
      g_assert_true (caught);
    }
  }
}
static void test_mapping ()
{
  Mapping a; a.set_n (0, 3); a.set_point (0,0,0,1); a.set_point (0,1,0,2); a.set_point (0,2,1,3); a.base_value = .5;
  Mapping b = a; b.set_point (0,0,0,4); a = a;
  float inputs[9]{}; g_assert_cmpfloat (a.calculate (inputs), ==, 1.5); g_assert_cmpfloat (b.calculate (inputs), ==, 4.5);
  inputs[0] = 2; g_assert_cmpfloat (a.calculate (inputs), ==, 4.5);
  Mapping c = std::move (b); g_assert_cmpint (c.get_n (0), ==, 3);
}
static void test_stream ()
{
  Resource r; r.set_text (BRUSH_TEXTURE_NAME, "missing-pattern-name");
  auto *stream = g_memory_output_stream_new_resizable (); GError *error = nullptr;
  g_assert_true (r.save (stream, nullptr, &error)); g_assert_no_error (error);
  auto size = g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (stream));
  auto *bytes = static_cast<const char *> (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (stream)));
  auto *input = g_memory_input_stream_new_from_data (bytes, size, nullptr);
  equal_semantics (r, Resource::load (input)); g_object_unref (input);
  g_assert_true (g_output_stream_close (stream, nullptr, &error));
  g_assert_false (r.save (stream, nullptr, &error)); g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CLOSED);
  g_clear_error (&error); g_object_unref (stream);
}
static void test_malformed ()
{
  for (const auto *s : {"", "{", "[]", "{\"version\":3,\"switches\":{\"non_incremental\":5}}", "{\"version\":3,\"settings\":{\"hardness\":{}}}", "version 99\n", "version 2\nhardness 1 | pressure (0 0), (1)"}) {
    bool caught = false; try { Resource::decode (s); } catch (const std::exception&) { caught = true; } g_assert_true (caught);
  }
  auto r = Resource::decode (R"({"version":3,"settings":{"hardness":{"base_value":1,"inputs":{"pressure":[[0,1]]}}}})");
  g_assert_cmpuint (r.diagnostics ().size (), ==, 1); g_assert_cmpuint (r.curve (BRUSH_HARDNESS, INPUT_PRESSURE).size (), ==, 1);
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr); g_assert_cmpint (argc, ==, 2); corpus = argv[1];
  g_test_add_func ("/painter/myb/corpus", test_corpus); g_test_add_func ("/painter/myb/defaults", test_defaults);
  g_test_add_func ("/painter/myb/unknown", test_unknown); g_test_add_func ("/painter/myb/legacy-curves", test_legacy_curves);
  g_test_add_func ("/painter/myb/legacy-unknown-input", test_legacy_unknown_input);
  g_test_add_func ("/painter/myb/mapping", test_mapping); g_test_add_func ("/painter/myb/stream", test_stream);
  g_test_add_func ("/painter/myb/malformed", test_malformed); return g_test_run ();
}
