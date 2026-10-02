/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-raster.hpp"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace GimpPainter;
static std::string directory;
static void native_bytes_and_cleanup ()
{
  {
    TemporaryFilterRaster raster (directory,1024);
    std::array<std::uint8_t,1024> input, output;
    for (std::size_t i = 0; i < input.size (); ++i) input[i] = i * 137;
    raster.write (0,input.size (),input.data ()); raster.flush ();
    raster.read (0,output.size (),output.data ()); g_assert_true (input == output);
    raster.write (1024,0,nullptr); raster.read (1024,0,nullptr);
  }
  GDir *dir = g_dir_open (directory.c_str (),0,nullptr);
  g_assert_nonnull (dir); g_assert_null (g_dir_read_name (dir)); g_dir_close (dir);
}
static void large_sparse_offset ()
{
  const std::uint64_t offset = UINT64_C (5) * 1024 * 1024 * 1024;
  TemporaryFilterRaster raster (directory,offset+4);
  const std::uint8_t in[] = {23,47,91,255}; std::uint8_t out[4];
  raster.write (offset,4,in); raster.flush (); raster.read (offset,4,out);
  g_assert_cmpmem (in,4,out,4);
  raster.read (0,4,out); for (auto byte : out) g_assert_cmpuint (byte, ==, 0);
}
static void invalid_access_and_unwritten_read ()
{
  TemporaryFilterRaster raster (directory,4);
  std::uint8_t data[4] = {};
  unsigned caught = 0;
  try { raster.read (0,4,data); } catch (const std::runtime_error&) { ++caught; }
  try { raster.write (1,4,data); } catch (const std::invalid_argument&) { ++caught; }
  try { raster.read (std::numeric_limits<std::uint64_t>::max (),1,data); } catch (const std::invalid_argument&) { ++caught; }
  try { raster.write (0,4,nullptr); } catch (const std::invalid_argument&) { ++caught; }
  try { TemporaryFilterRaster bad (directory,std::numeric_limits<std::uint64_t>::max ()); } catch (const std::invalid_argument&) { ++caught; }
  try { TemporaryFilterRaster bad (directory+"/missing",4); } catch (const std::runtime_error&) { ++caught; }
  g_assert_cmpuint (caught, ==, 6);
  raster.write (0,4,data); raster.read (0,4,data); // EOF does not poison subsequent valid writes
}
static void worker_affinity ()
{
  TemporaryFilterRaster raster (directory,4);
  unsigned caught = 0;
  std::thread worker ([&] {
    std::uint8_t bytes[4] = {};
    try { raster.write (0,4,bytes); } catch (const std::logic_error&) { ++caught; }
    try { raster.read (0,4,bytes); } catch (const std::logic_error&) { ++caught; }
    try { raster.flush (); } catch (const std::logic_error&) { ++caught; }
  }); worker.join (); g_assert_cmpuint (caught, ==, 3);
}
static void tiled_transpose_and_roundtrip ()
{
  for (auto shape : {std::pair<std::size_t,std::size_t> {1,1}, {1,17}, {19,1}, {67,66}, {1031,1027}})
    for (auto side : {std::size_t (3),std::size_t (1024)})
      {
        const auto w = shape.first, h = shape.second, size = w*h*4;
        TemporaryFilterRaster input (directory,size), transposed (directory,size), restored (directory,size);
        std::vector<std::uint8_t> original (size), observed (size);
        for (std::size_t i = 0; i < size; ++i) original[i] = (i * 137 + i / 17) % 256;
        input.write (0,size,original.data ());
        std::atomic<bool> cancel {false};
        g_assert_true (transpose_filter_rgba (input,transposed,w,h,cancel,side));
        transposed.read (0,size,observed.data ());
        for (std::size_t y = 0; y < h; ++y)
          for (std::size_t x = 0; x < w; ++x)
            g_assert_cmpmem (original.data ()+(y*w+x)*4,4,observed.data ()+(x*h+y)*4,4);
        g_assert_true (transpose_filter_rgba (transposed,restored,h,w,cancel,side));
        restored.read (0,size,observed.data ()); g_assert_true (original == observed);
      }
}
static void cancelled_or_failed_output_stays_private ()
{
  TemporaryFilterRaster input (directory,16), output (directory,16);
  const std::uint8_t bytes[16] = {}; input.write (0,16,bytes);
  std::atomic<bool> cancel {true};
  g_assert_false (transpose_filter_rgba (input,output,2,2,cancel));
  unsigned caught = 0;
  try { transpose_filter_rgba (input,input,2,2,cancel); } catch (const std::invalid_argument&) { ++caught; }
  try { transpose_filter_rgba (input,output,2,2,cancel,1025); } catch (const std::invalid_argument&) { ++caught; }
  try { transpose_filter_rgba (input,output,std::numeric_limits<std::size_t>::max (),2,cancel); } catch (const std::invalid_argument&) { ++caught; }
  g_assert_cmpuint (caught, ==, 3);
  cancel.store (false);
  g_assert_true (transpose_filter_rgba (input,output,2,2,cancel));
}
static void available_capacity_is_advisory ()
{
  g_assert_cmpuint (filter_available_space (directory), >, 0);
  unsigned caught = 0;
  try { filter_available_space (directory + "/missing"); } catch (const std::runtime_error&) { ++caught; }
  try { filter_available_space (""); } catch (const std::invalid_argument&) { ++caught; }
  try { filter_available_space (std::string ("a\0b",3)); } catch (const std::invalid_argument&) { ++caught; }
  g_assert_cmpuint (caught, ==, 3);
}
int main (int argc, char **argv)
{
  g_test_init (&argc,&argv,nullptr);
  gchar *path = g_dir_make_tmp ("painter-raster-test-XXXXXX",nullptr);
  g_assert_nonnull (path); directory = path; g_free (path);
#define ADD(name) g_test_add_func ("/filter-raster/" #name,name)
  ADD (available_capacity_is_advisory); ADD (native_bytes_and_cleanup); ADD (large_sparse_offset); ADD (invalid_access_and_unwritten_read);
  ADD (worker_affinity); ADD (tiled_transpose_and_roundtrip); ADD (cancelled_or_failed_output_stays_private);
  const int result = g_test_run ();
  g_assert_cmpint (g_rmdir (directory.c_str ()), ==, 0);
  return result;
}
