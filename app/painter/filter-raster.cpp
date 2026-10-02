/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-raster.hpp"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <vector>
#ifdef G_OS_WIN32
#include <io.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <sys/statvfs.h>
#endif

namespace GimpPainter {
namespace {
[[noreturn]] void io_error (const char *operation)
{
  throw std::runtime_error (std::string ("Filter temporary raster ") + operation +
                           ": " + (errno ? g_strerror (errno) : "unexpected end or incomplete I/O"));
}
}
std::uint64_t filter_available_space (const std::string& directory)
{
  if (directory.empty () || directory.find ('\0') != std::string::npos)
    throw std::invalid_argument ("Invalid filter spill directory");
#ifdef G_OS_WIN32
  // Quota-aware64-bit API, including UTF-8/UNC paths. Native Windows tests remain required.
  // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getdiskfreespaceexw
  std::string path = directory;
  if (path.back () != '/' && path.back () != '\\') path += '\\';
  std::unique_ptr<gunichar2, decltype (&g_free)> wide (g_utf8_to_utf16 (path.c_str (), -1, nullptr, nullptr, nullptr), g_free);
  if (!wide) throw std::invalid_argument ("Invalid UTF-8 filter spill directory");
  ULARGE_INTEGER available;
  if (!GetDiskFreeSpaceExW (reinterpret_cast<LPCWSTR> (wide.get ()), &available, nullptr, nullptr))
    throw std::runtime_error ("Filter filesystem capacity query failed");
  return available.QuadPart;
#else
  struct statvfs data;
  if (statvfs (directory.c_str (), &data) != 0) io_error ("filesystem capacity query failed");
  const auto unit = std::uint64_t (data.f_frsize ? data.f_frsize : data.f_bsize);
  const auto blocks = std::uint64_t (data.f_bavail);
  if (!unit) throw std::runtime_error ("Filter filesystem has no allocation unit");
  return blocks > std::numeric_limits<std::uint64_t>::max () / unit ?
    std::numeric_limits<std::uint64_t>::max () : blocks * unit;
#endif
}
TemporaryFilterRaster::TemporaryFilterRaster (const std::string& directory, std::uint64_t size)
  : size_ (size), thread_ (std::this_thread::get_id ())
{
  if (directory.empty () || directory.find ('\0') != std::string::npos ||
      size > std::uint64_t (std::numeric_limits<std::int64_t>::max ()))
    throw std::invalid_argument ("Invalid filter temporary raster location/size");
  gchar *path = g_build_filename (directory.c_str (), "gimp-painter-filter-XXXXXX", nullptr);
  int flags = O_RDWR;
#ifdef G_OS_WIN32
  flags |= _O_BINARY | _O_TEMPORARY;
#else
#ifdef O_CLOEXEC
  flags |= O_CLOEXEC;
#endif
#endif
  const int fd = g_mkstemp_full (path, flags, 0600);
  if (fd == -1) { const int saved = errno; g_free (path); errno = saved; io_error ("creation failed"); }
#ifdef G_OS_WIN32
  file_ = _fdopen (fd, "w+b");
#else
  static_assert (sizeof (off_t) >= 8, "Filter spill requires 64-bit file offsets");
  file_ = fdopen (fd, "w+b");
#endif
  if (!file_)
    {
      const int saved = errno;
      g_close (fd, nullptr); g_unlink (path); g_free (path);
      errno = saved; io_error ("stream creation failed");
    }
#ifndef G_OS_WIN32
  if (g_unlink (path) != 0)
    {
      const int saved = errno;
      std::fclose (file_); file_ = nullptr; g_free (path);
      errno = saved; io_error ("unlink failed");
    }
#endif
  g_free (path);
}
TemporaryFilterRaster::~TemporaryFilterRaster () noexcept
{ if (file_) std::fclose (file_); }
void TemporaryFilterRaster::position (std::uint64_t offset, std::size_t count, const void *bytes)
{
  if (thread_ != std::this_thread::get_id ())
    throw std::logic_error ("Filter temporary raster accessed outside its worker thread");
  if (offset > size_ || count > size_ - offset || (count && !bytes))
    throw std::invalid_argument ("Filter temporary raster access exceeds its extent");
  errno = 0;
#ifdef G_OS_WIN32
  const int result = _fseeki64 (file_, static_cast<__int64> (offset), SEEK_SET);
#else
  const int result = fseeko (file_, static_cast<off_t> (offset), SEEK_SET);
#endif
  if (result != 0) io_error ("seek failed");
}
void TemporaryFilterRaster::read (std::uint64_t offset, std::size_t count, std::uint8_t *bytes)
{
  position (offset, count, bytes);
  if (count && std::fread (bytes, 1, count, file_) != count) io_error ("read failed");
}
void TemporaryFilterRaster::write (std::uint64_t offset, std::size_t count, const std::uint8_t *bytes)
{
  position (offset, count, bytes);
  if (count && std::fwrite (bytes, 1, count, file_) != count) io_error ("write failed");
}
void TemporaryFilterRaster::flush ()
{
  if (thread_ != std::this_thread::get_id ())
    throw std::logic_error ("Filter temporary raster flushed outside its worker thread");
  errno = 0;
  if (std::fflush (file_) != 0) io_error ("flush failed");
}
bool transpose_filter_rgba (FilterRaster& input, FilterRaster& output,
                            std::size_t width, std::size_t height,
                            std::atomic<bool>& cancel, std::size_t side, std::size_t bytes_per_pixel)
{
  if (!bytes_per_pixel || bytes_per_pixel > 32 || !width || !height || !side || side > 1024 || &input == &output ||
      std::uint64_t (width) > std::uint64_t (std::numeric_limits<std::int64_t>::max ()) / bytes_per_pixel / height ||
      input.size () != std::uint64_t (width) * height * bytes_per_pixel || output.size () != input.size ())
    throw std::invalid_argument ("Invalid filter transpose extent/storage");
  if (cancel.load (std::memory_order_relaxed)) return false;
  std::vector<std::uint8_t> tile (side * side * bytes_per_pixel), column (side * bytes_per_pixel);
  for (std::size_t y = 0; y < height; y += std::min (side, height - y))
    for (std::size_t x = 0; x < width; x += std::min (side, width - x))
      {
        const auto rows = std::min (side, height - y), columns = std::min (side, width - x);
        for (std::size_t row = 0; row < rows; ++row)
          {
            if (cancel.load (std::memory_order_relaxed)) return false;
            input.read ((std::uint64_t (y + row) * width + x) * bytes_per_pixel, columns * bytes_per_pixel, tile.data () + row * columns * bytes_per_pixel);
          }
        for (std::size_t col = 0; col < columns; ++col)
          {
            if (cancel.load (std::memory_order_relaxed)) return false;
            for (std::size_t row = 0; row < rows; ++row)
              std::copy_n (tile.data () + (row * columns + col) * bytes_per_pixel, bytes_per_pixel, column.data () + row * bytes_per_pixel);
            output.write ((std::uint64_t (x + col) * height + y) * bytes_per_pixel, rows * bytes_per_pixel, column.data ());
          }
      }
  output.flush ();
  return !cancel.load (std::memory_order_relaxed);
}
} // namespace GimpPainter
