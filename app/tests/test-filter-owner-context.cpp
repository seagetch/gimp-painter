/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Independent owner-phase tests. Expected pixels are genuine old native
 * captures; these tests do not claim executor or FilterLayer scheduling coverage. */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
}
#include "core/gimpfiltercontext.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <vector>
extern "C" {
void gimp_test_filter_owner_context (Gimp *, const gchar *);
void gimp_test_filter_owner_context_phases (Gimp *, const gchar *);
void gimp_test_filter_owner_context_expansion (Gimp *, const gchar *);
void gimp_test_filter_owner_context_retry (Gimp *, const gchar *);
}
using namespace GimpPainter;
using Bytes = std::vector<std::uint8_t>;

namespace {
struct Record
{
  std::string id;
  unsigned width, height, channels, components, empty;
};
struct Bounds
{
  int width, height, ox, oy, empty, sx1, sy1, sx2, sy2, selected, x1, y1, x2, y2;
};
Bytes load (const char *directory, const std::string& name, std::size_t size)
{
  gchar *path = g_build_filename (directory, name.c_str (), nullptr);
  gchar *data = nullptr; gsize length = 0; GError *error = nullptr;
  g_assert_true (g_file_get_contents (path, &data, &length, &error));
  g_assert_no_error (error); g_assert_cmpuint (length, ==, size);
  Bytes result (data, data + length); g_free (path); g_free (data);
  return result;
}
std::vector<Record> records (const char *directory)
{
  std::ifstream stream (std::string (directory) + "/merges.tsv");
  g_assert_true (stream.good ());
  std::vector<Record> result; Record record;
  while (stream >> record.id >> record.width >> record.height >> record.channels >> record.components >> record.empty)
    result.push_back (record);
  g_assert_true (stream.eof ()); g_assert_false (result.empty ());
  return result;
}
std::map<std::string, Bounds> bounds (const char *directory)
{
  std::ifstream stream (std::string (directory) + "/bounds.tsv");
  g_assert_true (stream.good ());
  std::map<std::string, Bounds> result; std::string id; Bounds b;
  while (stream >> id >> b.width >> b.height >> b.ox >> b.oy >> b.empty >> b.sx1 >> b.sy1 >> b.sx2 >> b.sy2
         >> b.selected >> b.x1 >> b.y1 >> b.x2 >> b.y2)
    g_assert_true (result.emplace (id, b).second);
  g_assert_true (stream.eof ()); g_assert_false (result.empty ());
  return result;
}
void assert_equal_bytes (const Bytes& actual, const Bytes& expected, const std::string& label)
{
  g_test_message ("%s", label.c_str ());
  g_assert_cmpmem (actual.data (), actual.size (), expected.data (), expected.size ());
}
Bytes carrier (const Bytes& native, unsigned channels)
{
  if (channels == 4) return native;
  g_assert_cmpuint (channels, ==, 2);
  Bytes result (native.size () * 2);
  for (std::size_t i = 0; i < native.size () / 2; ++i)
    {
      result[i * 4] = result[i * 4 + 1] = result[i * 4 + 2] = native[i * 2];
      result[i * 4 + 3] = native[i * 2 + 1];
    }
  return result;
}
Bytes pixels (const char *directory, const Record& record, const char *label)
{ return carrier (load (directory, record.id + "-" + label + ".raw",
                         record.width * record.height * record.channels), record.channels); }

struct Scene
{
  Scene (Gimp *application, unsigned width, unsigned height, unsigned channels,
         unsigned image_width = 0, unsigned image_height = 0, int ox = 0, int oy = 0)
    : width (width), height (height), channels (channels),
      image_width (image_width ? image_width : width), image_height (image_height ? image_height : height)
  {
    image = gimp_image_new (application, this->image_width, this->image_height,
                            channels == 2 ? GIMP_GRAY : GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
    parent = gimp_group_layer_new (image);
    target = gimp_layer_new (image, width, height, babl_format (channels == 2 ? "Y'A u8" : "R'G'B'A u8"),
                             "Owner context", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
    gimp_image_add_layer (image, parent, nullptr, 0, FALSE);
    gimp_image_add_layer (image, target, parent, 0, FALSE);
    gimp_item_set_offset (GIMP_ITEM (target), ox, oy);
  }
  ~Scene () { g_object_unref (image); }
  GimpDrawable *drawable () const { return GIMP_DRAWABLE (target); }
  unsigned width, height, channels, image_width, image_height;
  GimpImage *image;
  GimpLayer *parent, *target;
};
void set_selection (Scene& scene, const Bytes& global)
{
  g_assert_cmpuint (global.size (), ==, scene.image_width * scene.image_height);
  auto *selection = gimp_image_get_mask (scene.image);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (selection)), nullptr, 0,
                   babl_format ("Y u8"), global.data (), GEGL_AUTO_ROWSTRIDE);
  selection->bounds_known = FALSE;
  gimp_drawable_invalidate_boundary (GIMP_DRAWABLE (selection));
  gimp_drawable_update (GIMP_DRAWABLE (selection), 0, 0, scene.image_width, scene.image_height);
  gimp_image_mask_changed (scene.image);
}
/* These are the archived capture-harness.c selection inputs, not expected
 * output formulas. Reconstruct the entire image mask, including outside pixels. */
Bytes selection (unsigned width, unsigned height, unsigned kind)
{
  Bytes result (width * height, 0);
  for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x)
    {
      auto& value = result[y * width + x];
      if (kind == 1 && x >= width / 4 && x < 3 * width / 4 && y >= height / 4 && y < 3 * height / 4) value = 255;
      if (kind == 2 && x > 0 && y > 0 && x < width - 1 && y < height - 1) value = (x * 17 + y * 29) % 256;
      if (kind == 3 && x == width - 1 && y == height - 1) value = 255;
      if (kind == 4 && x >= width / 3 && x < 2 * width / 3 && y >= 3 && y < height - 3) value = 255;
    }
  return result;
}
void active (Scene& scene, unsigned mask)
{
  if (scene.channels == 4)
    {
      gimp_image_set_component_active (scene.image, GIMP_CHANNEL_RED, mask & 1);
      gimp_image_set_component_active (scene.image, GIMP_CHANNEL_GREEN, mask & 2);
      gimp_image_set_component_active (scene.image, GIMP_CHANNEL_BLUE, mask & 4);
    }
  else gimp_image_set_component_active (scene.image, GIMP_CHANNEL_GRAY, mask & 1);
  gimp_image_set_component_active (scene.image, GIMP_CHANNEL_ALPHA, mask & (1U << (scene.channels - 1)));
}
void capture (FilterOwnerContext& context, Scene& scene, const Bytes& input)
{
  for (unsigned y = 0; y < scene.height; ++y) for (unsigned x = 0; x < scene.width; x += 7)
    {
      const auto count = std::min (7U, scene.width - x), offset = y * scene.width + x;
      context.capture_input (scene.drawable (), offset, count, input.data () + offset * 4);
    }
}
unsigned prepare (FilterOwnerContext& context, Scene& scene, FilterProcedureRequest& request, unsigned budget = 11)
{
  request.width = scene.width; request.height = scene.height;
  unsigned quanta = 0;
  while (!context.before_process (scene.drawable (), request, budget)) g_assert_cmpuint (++quanta, <, 20000);
  g_assert_true (request.raw_shadow);
  return quanta;
}
unsigned seal (FilterOwnerContext& context, Scene& scene, unsigned budget = 7)
{
  unsigned quanta = 0;
  while (!context.before_import (scene.drawable (), FilterProcedureDisposition::shadow, budget))
    g_assert_cmpuint (++quanta, <, 20000);
  return quanta;
}
Bytes merge (FilterOwnerContext& context, Scene& scene, const Bytes& shadow)
{
  Bytes result;
  for (unsigned y = 0; y < scene.height; ++y) for (unsigned x = 0; x < scene.width; x += 5)
    {
      const auto count = std::min (5U, scene.width - x), offset = y * scene.width + x;
      Bytes chunk;
      context.merge_chunk (offset, count, shadow.data () + offset * 4, chunk);
      result.insert (result.end (), chunk.begin (), chunk.end ());
    }
  return result;
}
void region (const FilterProcedureRequest& request, const Bounds& b)
{
  g_assert_cmpint (request.start_region.selected, ==, b.selected);
  g_assert_cmpint (request.start_region.intersects, ==, b.x1 < b.x2 && b.y1 < b.y2);
  g_assert_cmpint (request.start_region.x1, ==, b.x1); g_assert_cmpint (request.start_region.y1, ==, b.y1);
  g_assert_cmpint (request.start_region.x2, ==, b.x2); g_assert_cmpint (request.start_region.y2, ==, b.y2);
}
void unrestricted (const FilterProcedureRequest& request, const Scene& scene)
{
  g_assert_false (request.start_region.selected); g_assert_true (request.start_region.intersects);
  g_assert_cmpint (request.start_region.x1, ==, 0); g_assert_cmpint (request.start_region.y1, ==, 0);
  g_assert_cmpint (request.start_region.x2, ==, scene.width); g_assert_cmpint (request.start_region.y2, ==, scene.height);
}
void background (GimpContext *context, unsigned red, unsigned green, unsigned blue)
{
  const double rgba[] = { red / 255.0, green / 255.0, blue / 255.0, 1.0 };
  auto *color = gegl_color_new (nullptr);
  gegl_color_set_pixel (color, babl_format ("R'G'B'A double"), rgba);
  gimp_context_set_background (context, color); g_object_unref (color);
}
void recorded_final (FilterOwnerContext& context, Scene& scene, const char *directory,
                     const Record& record, bool lock)
{
  const auto mask = load (directory, record.id + "-mask.raw", record.width * record.height);
  set_selection (scene, record.empty ? Bytes (mask.size (), 0) : mask);
  active (scene, record.components);
  gimp_layer_set_lock_alpha (scene.target, lock, FALSE);
  context.selection_changed ();
}
}

extern "C" void gimp_test_filter_owner_context (Gimp *application, const gchar *directory)
{
  const auto fixture_bounds = bounds (directory);
  unsigned cases = 0;
  for (const auto& record : records (directory))
    {
      if (record.id.find ("native-") != 0 || (record.channels != 2 && record.channels != 4)) continue;
      const auto& b = fixture_bounds.at (record.id);
      const auto selection_kind = unsigned (std::stoul (record.id.substr (record.id.find ("-s") + 2)));
      const auto component_mask = unsigned (std::stoul (record.id.substr (record.id.find ("-a") + 2)));
      const auto lock = unsigned (std::stoul (record.id.substr (record.id.find ("-l") + 2)));
      Scene scene (application, record.width, record.height, record.channels, 39, 29, b.ox, b.oy);
      const auto global = selection (39, 29, selection_kind);
      set_selection (scene, global); active (scene, component_mask);
      Bytes local (record.width * record.height, 255);
      if (!record.empty)
        for (unsigned y = 0; y < record.height; ++y) for (unsigned x = 0; x < record.width; ++x)
          local[y * record.width + x] = global[(y + b.oy) * 39 + x + b.ox];
      assert_equal_bytes (local, load (directory, record.id + "-mask.raw", local.size ()), record.id + " mask input");
      if (lock == 1) gimp_layer_set_lock_alpha (scene.target, TRUE, FALSE);
      if (lock == 2) scene.parent->lock_alpha = TRUE; // Explicitly synthetic, as in the old oracle.
      FilterOwnerContext context; FilterProcedureRequest request;
      const auto input = pixels (directory, record, "input"), shadow = pixels (directory, record, "shadow");
      capture (context, scene, input);
      g_assert_cmpuint (prepare (context, scene, request), >, 1);
      g_assert_false (gimp_image_get_mask (scene.image)->bounds_known);
      region (request, b); seal (context, scene);
      // A sealed import cannot resample live context on any later chunk.
      set_selection (scene, Bytes (39 * 29, 0)); active (scene, 0);
      gimp_layer_set_lock_alpha (scene.target, TRUE, FALSE); context.selection_changed ();
      assert_equal_bytes (merge (context, scene, shadow), pixels (directory, record, "output"), record.id);
      auto next_input = input; next_input[0] ^= 127;
      if (record.channels == 2) next_input[1] = next_input[2] = next_input[0];
      capture (context, scene, next_input);
      g_assert_true (context.before_import (scene.drawable (), FilterProcedureDisposition::no_merge, 7));
      assert_equal_bytes (merge (context, scene, shadow), next_input, record.id + " subsequent no-merge");
      ++cases;
    }
  g_assert_cmpuint (cases, ==, 240);
}

extern "C" void gimp_test_filter_owner_context_phases (Gimp *application, const gchar *directory)
{
  const auto fixture_bounds = bounds (directory);
  GimpContext *user = gimp_get_user_context (application);
  GeglColor *saved = gegl_color_duplicate (gimp_context_get_background (user));
  unsigned cases = 0;
  for (const auto& record : records (directory))
    {
      const bool running = record.id.find ("running-") == 0, idle = record.id.find ("idle-") == 0;
      if (!running && !idle) continue;
      const auto kind = unsigned (std::stoul (record.id.substr (record.id.find ("-k") + 2)));
      const bool second = record.id.find ("-merge2") != std::string::npos;
      const auto prefix = record.id.substr (0, record.id.find ("-merge"));
      Scene scene (application, record.width, record.height, record.channels);
      FilterOwnerContext context; FilterProcedureRequest request;
      const auto input = pixels (directory, record, "input"), shadow = pixels (directory, record, "shadow");
      assert_equal_bytes (input, load (directory, prefix + (second ? "-start2.raw" : "-start1.raw"), input.size ()), prefix + " prepared input");
      set_selection (scene, Bytes (record.width * record.height, 0)); active (scene, 15);
      if (second) recorded_final (context, scene, directory, record, kind == 5);
      background (user, second && kind == 6 ? 217 : 31, second && kind == 6 ? 31 : 121,
                   second && kind == 6 ? 121 : 217);
      capture (context, scene, input); prepare (context, scene, request);
      if (second) region (request, fixture_bounds.at (record.id)); else unrestricted (request, scene);
      g_assert_cmpuint (request.background[0], ==, second && kind == 6 ? 217 : 31);
      g_assert_cmpuint (request.background[1], ==, second && kind == 6 ? 31 : 121);
      g_assert_cmpuint (request.background[2], ==, second && kind == 6 ? 121 : 217);
      if (running && !second)
        {
          recorded_final (context, scene, directory, record, kind == 5);
          if (kind == 6) background (user, 217, 31, 121);
        }
      seal (context, scene);
      const auto result = merge (context, scene, shadow);
      assert_equal_bytes (result, pixels (directory, record, "output"), record.id);
      assert_equal_bytes (result, load (directory, prefix + (second ? "-rerun.raw" : "-settled.raw"), result.size ()), record.id + " published capture");
      if (idle && !second)
        {
          set_selection (scene, selection (record.width, record.height, kind < 3 ? kind : 0));
          active (scene, kind == 3 ? 1 : kind == 4 ? 8 : 15);
          gimp_layer_set_lock_alpha (scene.target, kind == 5, FALSE);
          if (kind == 6) background (user, 217, 31, 121);
          context.selection_changed ();
          assert_equal_bytes (merge (context, scene, shadow), load (directory, prefix + "-after-context.raw", result.size ()),
                  prefix + " context-only edit retains sealed bytes");
        }
      ++cases;
    }
  gimp_context_set_background (user, saved); g_object_unref (saved);
  g_assert_cmpuint (cases, ==, 24);
}

extern "C" void gimp_test_filter_owner_context_expansion (Gimp *application, const gchar *directory)
{
  const auto fixture_bounds = bounds (directory);
  GimpContext *user = gimp_get_user_context (application);
  GeglColor *saved = gegl_color_duplicate (gimp_context_get_background (user));
  background (user, 31, 121, 217);
  const unsigned alpha_changes[] = {0, 2208, 2426, 2038}, rerun_changes[] = {0, 10813, 11956, 10422};
  unsigned cases = 0;
  for (const auto& record : records (directory))
    {
      const auto kind = unsigned (std::stoul (record.id.substr (record.id.find ("-k") + 2)));
      const bool second = record.id.find ("-merge2") != std::string::npos;
      g_assert_cmpuint (kind, >=, 1); g_assert_cmpuint (kind, <=, 3);
      const auto prefix = record.id.substr (0, record.id.find ("-merge"));
      Scene scene (application, record.width, record.height, record.channels);
      FilterOwnerContext context; FilterProcedureRequest request;
      const auto input = pixels (directory, record, "input"), shadow = pixels (directory, record, "shadow");
      assert_equal_bytes (input, load (directory, prefix + (second ? "-start2.raw" : "-start1.raw"), input.size ()), prefix + " prepared input");
      set_selection (scene, selection (record.width, record.height, 4)); active (scene, 15);
      if (second) recorded_final (context, scene, directory, record, false);
      capture (context, scene, input); prepare (context, scene, request);
      if (second) region (request, fixture_bounds.at (record.id));
      else
        {
          g_assert_true (request.start_region.selected); g_assert_true (request.start_region.intersects);
          g_assert_cmpint (request.start_region.x1, ==, 21); g_assert_cmpint (request.start_region.x2, ==, 42);
          g_assert_cmpint (request.start_region.y1, ==, 3); g_assert_cmpint (request.start_region.y2, ==, 60);
          recorded_final (context, scene, directory, record, false);
        }
      seal (context, scene);
      const auto result = merge (context, scene, shadow);
      assert_equal_bytes (result, pixels (directory, record, "output"), record.id);
      assert_equal_bytes (result, load (directory, prefix + (second ? "-rerun.raw" : "-settled.raw"), result.size ()), record.id + " published capture");
      if (!second)
        {
          unsigned outside = 0, changed = 0, differences = 0;
          const auto rerun = load (directory, prefix + "-rerun.raw", result.size ());
          for (unsigned y = 0; y < 63; ++y) for (unsigned x = 0; x < 63; ++x)
            if (!(21 <= x && x < 42 && 3 <= y && y < 60))
              {
                ++outside; const auto offset = (y * 63 + x) * 4;
                for (unsigned c = 0; c < 4; ++c) g_assert_cmpuint (shadow[offset + c], ==, 0);
                changed += input[offset + 3] != result[offset + 3];
              }
          for (std::size_t i = 0; i < result.size (); ++i) differences += result[i] != rerun[i];
          g_assert_cmpuint (outside, ==, 2772); g_assert_cmpuint (changed, ==, alpha_changes[kind]);
          g_assert_cmpuint (differences, ==, rerun_changes[kind]);
        }
      ++cases;
    }
  gimp_context_set_background (user, saved); g_object_unref (saved);
  g_assert_cmpuint (cases, ==, 6);
}

extern "C" void gimp_test_filter_owner_context_retry (Gimp *application, const gchar *directory)
{
  const auto fixture_bounds = bounds (directory);
  const Record soft {"native-c4-s2-a15-l0", 31, 17, 4, 15, 0};
  const Record hard {"native-c4-s1-a8-l0", 31, 17, 4, 8, 0};
  Scene scene (application, 31, 17, 4, 39, 29, 3, 2);
  FilterOwnerContext context; FilterProcedureRequest request;
  const auto input = pixels (directory, soft, "input"), shadow = pixels (directory, soft, "shadow");
  set_selection (scene, selection (39, 29, 3)); active (scene, 15); capture (context, scene, input);
  request.width = 31; request.height = 17;
  for (unsigned n = 0; n < 4; ++n) g_assert_false (context.before_process (scene.drawable (), request, 11));
  set_selection (scene, selection (39, 29, 2)); context.selection_changed ();
  g_assert_cmpuint (prepare (context, scene, request), >, 1);
  region (request, fixture_bounds.at (soft.id)); g_assert_false (gimp_image_get_mask (scene.image)->bounds_known);
  // The final coverage copy is also restartable, not a splice of two masks.
  for (unsigned n = 0; n < 4; ++n)
    g_assert_false (context.before_import (scene.drawable (), FilterProcedureDisposition::shadow, 7));
  set_selection (scene, selection (39, 29, 1)); context.selection_changed ();
  for (unsigned n = 0; n < 4; ++n)
    g_assert_false (context.before_import (scene.drawable (), FilterProcedureDisposition::shadow, 7));
  set_selection (scene, selection (39, 29, 2)); context.selection_changed ();
  g_assert_cmpuint (seal (context, scene), >, 1);
  assert_equal_bytes (merge (context, scene, shadow), pixels (directory, soft, "output"), "selection scan/copy retry");

  // Mutate after the last mask byte was copied, before the separate seal call.
  capture (context, scene, input); prepare (context, scene, request);
  for (unsigned n = 0; n < 17; ++n)
    g_assert_false (context.before_import (scene.drawable (), FilterProcedureDisposition::shadow, 31));
  set_selection (scene, selection (39, 29, 1)); active (scene, 8); context.selection_changed ();
  seal (context, scene, 31);
  set_selection (scene, selection (39, 29, 0)); active (scene, 15); context.selection_changed ();
  assert_equal_bytes (merge (context, scene, shadow), pixels (directory, hard, "output"), "retry before seal; stable after seal");

  // Outside-at-start means no plug-in merge even if the final selection expands.
  capture (context, scene, input); set_selection (scene, selection (39, 29, 3)); context.selection_changed ();
  prepare (context, scene, request);
  g_assert_true (request.start_region.selected); g_assert_false (request.start_region.intersects);
  set_selection (scene, selection (39, 29, 0)); context.selection_changed ();
  g_assert_true (context.before_import (scene.drawable (), FilterProcedureDisposition::no_merge, 7));
  assert_equal_bytes (merge (context, scene, shadow), input, "outside start then expanded final remains no-merge");
}
