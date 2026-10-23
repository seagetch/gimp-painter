/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "paint/painter-mypaint-surface/buffer-read.hpp"
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
#include <array>
#include <string>
#include <utility>
using namespace GimpPainter::MyPaint;
namespace {
const GeglRectangle backing_extent = {-7, 4, 11, 9};
const GeglRectangle extent = {-4, 7, 5, 3};
struct Fixture {
  const Babl *format;
  int bpp;
  std::vector<guchar> bytes;
  GeglBuffer *backing, *buffer;
  explicit Fixture (const Babl *f) : format (f), bpp (babl_format_get_bytes_per_pixel (f)),
    bytes (backing_extent.width * backing_extent.height * bpp),
    backing (gegl_buffer_new (&backing_extent, format)),
    buffer (gegl_buffer_create_sub_buffer (backing, &extent))
  {
    // Native-format copies must preserve every bit, including hidden alpha and
    // arbitrary float bit patterns; no color converter should touch them.
    for (std::size_t i = 0; i < bytes.size (); ++i) bytes[i] = 1 + i % 251;
    gegl_buffer_set (backing, &backing_extent, 0, format, bytes.data (), GEGL_AUTO_ROWSTRIDE);
  }
  ~Fixture () { g_object_unref (buffer); g_object_unref (backing); }
  const guchar *at (int x, int y) const
  { return bytes.data () + ((y - backing_extent.y) * backing_extent.width + x - backing_extent.x) * bpp; }
};
bool inside (const GeglRectangle& r, int x, int y)
{ return x >= r.x && y >= r.y && std::int64_t (x) < std::int64_t (r.x) + r.width && std::int64_t (y) < std::int64_t (r.y) + r.height; }
void verify (Fixture& f, const GeglRectangle& request)
{
  const std::size_t row = std::size_t (request.width) * f.bpp, stride = row + 13;
  const std::size_t span = request.width && request.height ? (request.height - 1) * stride + row : 0;
  std::vector<guchar> result (span + 34, 0xa5);
  BufferRead::into (f.buffer, request, f.format, result.data () + 17, span, stride);
  for (std::size_t i = 0; i < 17; ++i) g_assert_cmpuint (result[i], ==, 0xa5);
  for (std::size_t i = 17 + span; i < result.size (); ++i) g_assert_cmpuint (result[i], ==, 0xa5);
  if (!request.width || !request.height) return;
  for (int y = 0; y < request.height; ++y)
    for (int x = 0; x < request.width; ++x)
      for (int c = 0; c < f.bpp; ++c)
        {
          const auto value = inside (extent, request.x + x, request.y + y) ? f.at (request.x + x, request.y + y)[c] : 0;
          g_assert_cmpuint (result[17 + y * stride + x * f.bpp + c], ==, value);
        }
  for (int y = 0; y + 1 < request.height; ++y)
    for (std::size_t x = row; x < stride; ++x) g_assert_cmpuint (result[17 + y * stride + x], ==, 0xa5);
  const auto packed = BufferRead::packed (f.buffer, request);
  g_assert_cmpuint (packed.size (), ==, row * request.height);
  for (int y = 0; y < request.height; ++y)
    g_assert_cmpmem (packed.data () + y * row, row, result.data () + 17 + y * stride, row);
  const auto unchanged = BufferRead::packed (f.backing, backing_extent);
  g_assert_true (unchanged == f.bytes);
  g_assert_true (gegl_rectangle_equal (gegl_buffer_get_extent (f.buffer), &extent));
}
void native_formats_and_partial_reads ()
{
  const GeglRectangle requests[] = {
    {-4,7,5,3}, {-3,8,2,1}, {-6,6,9,5}, {-6,7,3,3}, {0,7,3,3},
    {-4,5,5,3}, {-4,9,5,3}, {-6,7,2,3}, {1,7,2,3}, {-4,5,5,2},
    {-4,10,5,2}, {40,-30,2,2}, {-4,7,0,3}, {-4,7,5,0}, {-4,7,1,1}
  };
  unsigned count = 0;
  for (const char *space_name : {"sRGB", "Rec2020"})
    for (const char *model : {"RGB", "RGBA", "R'G'B'", "R'G'B'A", "R~G~B~", "R~G~B~A", "Y", "YA", "Y'", "Y'A", "Y~", "Y~A"})
      for (const char *type : {"u8", "u16", "u32", "half", "float", "double"})
        {
          Fixture fixture (babl_format_with_space ((std::string (model) + " " + type).c_str (), babl_space (space_name)));
          for (const auto& request : requests) verify (fixture, request);
          ++count;
        }
  g_assert_cmpuint (count, ==, 144);
}
void indexed_native_bytes ()
{
  for (const char *space : {"sRGB", "Rec2020"})
    {
      const Babl *index, *alpha;
      const auto *palette = babl_new_palette_with_space (nullptr, babl_space (space), &index, &alpha);
      guchar colors[] = {255, 0, 0, 0, 255, 0};
      babl_palette_set_palette (palette, babl_format_with_space ("R'G'B' u8", babl_space (space)), colors, 2);
      for (const Babl *format : {index, alpha})
        {
          Fixture fixture (format);
          verify (fixture, {-5, 6, 7, 5});
          verify (fixture, {-3, 8, 2, 1});
        }
    }
}
void explicit_conversion ()
{
  const GeglRectangle box = {-1,3,2,2}, request = {-2,2,4,4};
  auto *buffer = gegl_buffer_new (&box, babl_format ("Y u8"));
  const guchar bytes[] = {0, 255, 128, 64};
  gegl_buffer_set (buffer, &box, 0, babl_format ("Y u8"), bytes, GEGL_AUTO_ROWSTRIDE);
  const auto converted = BufferRead::packed (buffer, request, babl_format ("Y u16"));
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x)
      {
        std::uint16_t actual;
        std::memcpy (&actual, converted.data () + (y * 4 + x) * sizeof actual, sizeof actual);
        const unsigned expected = inside (box, x - 2, y + 2) ? unsigned (bytes[(y - 1) * 2 + x - 1]) * 257 : 0;
        g_assert_cmpuint (actual, ==, expected);
      }
  g_object_unref (buffer);
}
void signed_offsets_and_extent_admission ()
{
  Fixture f (babl_format ("R'G'B'A u8"));
  for (int x : {-100, 0, 100}) for (int y : {-100, 0, 100})
    {
      const GeglRectangle local = {-5,6,7,5};
      const auto image = BufferRead::translate (local, x, y);
      g_assert_cmpint (image.x, ==, local.x + x);
      g_assert_cmpint (image.y, ==, local.y + y);
      const auto restored = BufferRead::translate (image, -std::int64_t (x), -std::int64_t (y));
      g_assert_true (gegl_rectangle_equal (&restored, &local));
      verify (f, restored);
    }
  GeglRectangle result;
  const GeglRectangle request = {-2,-2,4,4}, distant = {G_MAXINT - 3, G_MININT, 4, 4};
  // Existing extents need not have representable exclusive endpoints.
  g_assert_false (BufferRead::intersect (result, request, distant));
  g_assert_true (BufferRead::intersect (result, request, distant, G_MAXINT - 2, G_MININT + 1));
  const GeglRectangle expected = {-1,-1,3,3};
  g_assert_true (gegl_rectangle_equal (&result, &expected));
  g_assert_false (BufferRead::intersect (result, request, {0,0,4,4}, G_MININT, G_MAXINT));
  g_assert_false (BufferRead::intersect (result, request, {0,0,4,4}, G_MAXINT, G_MININT));
  const auto lowest = BufferRead::translate ({G_MININT,0,1,1}, -std::int64_t (G_MININT), 0);
  g_assert_cmpint (lowest.x, ==, 0);
}
void exhaustive_intersections ()
{
  std::vector<GeglRectangle> rectangles;
  for (int x = -2; x < 2; ++x) for (int y = -2; y < 2; ++y)
    for (int w = 0; w < 3; ++w) for (int h = 0; h < 3; ++h)
      rectangles.push_back ({x,y,w,h});
  unsigned pairs = 0;
  for (const auto& a : rectangles) for (const auto& b : rectangles)
    {
      GeglRectangle result;
      const bool found = BufferRead::intersect (result, a, b);
      unsigned expected = 0, actual = 0;
      for (int y = -2; y < 4; ++y) for (int x = -2; x < 4; ++x)
        {
          const bool common = inside (a,x,y) && inside (b,x,y);
          expected += common;
          const bool returned = found && inside (result,x,y);
          actual += returned;
          g_assert_cmpint (returned, ==, common);
        }
      g_assert_cmpint (found, ==, expected != 0);
      g_assert_cmpuint (actual, ==, expected);
      ++pairs;
    }
  g_assert_cmpuint (pairs, ==, 20736);
}
template<class F> void rejects (F f)
{
  bool caught = false;
  try { f (); } catch (const std::invalid_argument&) { caught = true; }
                 catch (const std::length_error&) { caught = true; }
  g_assert_true (caught);
}
void invalid_inputs_leave_output_unchanged ()
{
  Fixture f (babl_format ("R'G'B'A u8"));
  std::array<guchar, 128> bytes; bytes.fill (0xcc);
  const auto before = bytes;
  auto read = [&] (GeglRectangle rect, std::size_t capacity, std::size_t stride) {
    BufferRead::into (f.buffer, rect, f.format, bytes.data (), capacity, stride);
  };
  for (const auto& rect : {GeglRectangle{0,0,-1,2}, {0,0,2,-1}, {G_MAXINT,0,1,1}, {0,G_MAXINT,1,1}})
    rejects ([&] { read (rect, bytes.size (), 16); });
  rejects ([&] { read (extent, bytes.size (), 19); });
  rejects ([&] { read (extent, 59, 20); });
  rejects ([&] { read (extent, bytes.size (), std::size_t (G_MAXINT) + 1); });
  rejects ([&] { BufferRead::into (f.buffer, extent, f.format, nullptr, 60, 20); });
  rejects ([&] { BufferRead::into (nullptr, extent, f.format, bytes.data (), 60, 20); });
  rejects ([&] { BufferRead::into (f.buffer, extent, nullptr, bytes.data (), 60, 20); });
  rejects ([&] { BufferRead::packed (f.buffer, {0,0,G_MAXINT,1}); });
  rejects ([&] { BufferRead::translate ({G_MININT,0,1,1}, -1, 0); });
  rejects ([&] { BufferRead::translate ({G_MAXINT-1,0,1,1}, 1, 0); });
  rejects ([&] { BufferRead::translate ({0,0,1,1}, std::numeric_limits<std::int64_t>::max (), 0); });
  g_assert_true (bytes == before);
  BufferRead::into (f.buffer, {-4,7,0,3}, f.format, nullptr, 0, 0);
  g_assert_true (BufferRead::packed (f.buffer, {-4,7,5,0}).empty ());
  g_assert_true (BufferRead::packed (f.buffer, {0,0,G_MAXINT,0}).empty ());
}
void finalized (gpointer data, GObject *) { ++*static_cast<unsigned *> (data); }
void synchronous_borrow_and_owned_result ()
{
  const GeglRectangle box = {-1,1,2,1};
  auto *buffer = gegl_buffer_new (&box, babl_format ("Y u8"));
  unsigned gone = 0;
  g_object_weak_ref (G_OBJECT (buffer), finalized, &gone);
  const guchar values[] = {42, 99};
  gegl_buffer_set (buffer, &box, 0, babl_format ("Y u8"), values, GEGL_AUTO_ROWSTRIDE);
  auto result = BufferRead::packed (buffer, box);
  gegl_buffer_clear (buffer, nullptr);
  g_assert_cmpmem (result.data (), result.size (), values, sizeof values);
  g_assert_cmpuint (gone, ==, 0);
  g_object_unref (buffer);
  g_assert_cmpuint (gone, ==, 1);
  g_assert_cmpmem (result.data (), result.size (), values, sizeof values);
}
void surface_extreme_selection_offsets ()
{
  const GeglRectangle box = {-2,-2,4,4}, mask_box = {0,0,4,4};
  for (bool wide : {false, true})
    for (const auto& offsets : {std::pair<int,int>{G_MININT,G_MAXINT}, {G_MAXINT,G_MININT}})
      {
        auto *buffer = gegl_buffer_new (&box, babl_format (wide ? "R'G'B'A double" : "R'G'B'A u8"));
        auto *mask = gegl_buffer_new (&mask_box, babl_format (wide ? "Y double" : "Y u8"));
        GeglSurface surface (buffer);
        surface.set_selection (mask, offsets.first, offsets.second);
        surface.set_shape_provider ([] (float,float,float,float) { return ShapeMask{3,3,std::vector<guchar>(9,255)}; });
        surface.begin_session ();
        g_assert_false (surface.draw_dab (0,0,2,1,0,0,1,1));
        float r,g,b,a;
        surface.get_color (0,0,2,&r,&g,&b,&a,1);
        g_assert_cmpfloat (a, ==, 0);
        g_assert_cmpuint (surface.bytes_read (), ==, 0);
        g_assert_cmpuint (surface.bytes_written (), ==, 0);
        surface.end_session ();
        g_object_unref (mask); g_object_unref (buffer);
      }
}
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr); gegl_init (&argc, &argv);
  g_test_add_func ("/buffer-read/native-formats-and-partial-regions", native_formats_and_partial_reads);
  g_test_add_func ("/buffer-read/indexed-native-bytes", indexed_native_bytes);
  g_test_add_func ("/buffer-read/explicit-conversion", explicit_conversion);
  g_test_add_func ("/buffer-read/signed-offsets-and-extents", signed_offsets_and_extent_admission);
  g_test_add_func ("/buffer-read/exhaustive-intersections", exhaustive_intersections);
  g_test_add_func ("/buffer-read/invalid-inputs", invalid_inputs_leave_output_unchanged);
  g_test_add_func ("/buffer-read/ownership", synchronous_borrow_and_owned_result);
  g_test_add_func ("/buffer-read/surface-extreme-selection-offsets", surface_extreme_selection_offsets);
  const int result = g_test_run (); gegl_exit (); return result;
}
