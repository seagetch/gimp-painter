/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "gimpfilterpaths.hpp"
#include <glib.h>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <vector>
#ifdef G_OS_WIN32
#include <windows.h>
#elif defined (__APPLE__)
#include <mach-o/dyld.h>
#endif

#if !defined (GIMP_PAINTER_FILTER_BUILD_ROOT) || \
    !defined (GIMP_PAINTER_FILTER_BIN_TO_WORKER) || \
    !defined (GIMP_PAINTER_FILTER_WORKER_TO_PLUGINS) || \
    !defined (GIMP_PAINTER_FILTER_EXECUTABLE_SUFFIX)
#error The Filter executable layout must be provided by the build system
#endif
#ifndef GIMP_PAINTER_FILTER_PATHS_OVERLAY_DIR
#define GIMP_PAINTER_FILTER_PATHS_OVERLAY_DIR ""
#endif

namespace GimpPainter {
namespace {
struct Free { void operator() (gchar *p) const noexcept { g_free (p); } };
using Text = std::unique_ptr<gchar, Free>;

std::string joined (const std::string& base, const char *tail)
{
  Text value (g_build_filename (base.c_str (), tail, nullptr));
  Text canonical (g_canonicalize_filename (value.get (), nullptr));
  return canonical.get ();
}

std::string executable_directory ()
{
  Text executable;
#ifdef G_OS_WIN32
  std::vector<wchar_t> buffer (256);
  for (;;)
    {
      const DWORD used = GetModuleFileNameW (nullptr, buffer.data (), DWORD (buffer.size ()));
      if (!used) throw std::runtime_error ("Cannot locate the running Filter host");
      if (used < buffer.size ())
        {
          executable.reset (g_utf16_to_utf8 (reinterpret_cast<const gunichar2 *> (buffer.data ()),
                                             used, nullptr, nullptr, nullptr));
          break;
        }
      if (buffer.size () >= 32768) throw std::runtime_error ("Filter host path exceeds Windows limits");
      buffer.resize (buffer.size () * 2);
    }
#elif defined (__APPLE__)
  std::uint32_t size = 1024;
  std::vector<char> buffer (size);
  if (_NSGetExecutablePath (buffer.data (), &size)) buffer.resize (size);
  if (!size || _NSGetExecutablePath (buffer.data (), &size))
    throw std::runtime_error ("Cannot locate the running Filter host");
  std::unique_ptr<char, decltype (&std::free)> resolved (realpath (buffer.data (), nullptr), &std::free);
  if (resolved) executable.reset (g_strdup (resolved.get ()));
#elif defined (__linux__)
  executable.reset (g_file_read_link ("/proc/self/exe", nullptr));
#else
  throw std::runtime_error ("Executable-relative Filter paths are unavailable on this platform");
#endif
  if (!executable || !g_path_is_absolute (executable.get ()))
    throw std::runtime_error ("Cannot locate the running Filter host");
  Text directory (g_path_get_dirname (executable.get ()));
  Text canonical (g_canonicalize_filename (directory.get (), nullptr));
  return canonical.get ();
}

bool build_location (const std::string& directory)
{
  return directory == joined (GIMP_PAINTER_FILTER_BUILD_ROOT, "app") ||
         directory == joined (GIMP_PAINTER_FILTER_BUILD_ROOT, "app/tests");
}

bool overlay_location (const std::string& directory)
{
  /* Instrumentation changes this compile-time constant for its private copy.
   * Production has no runtime environment or saved-data overlay switch. */
  return *GIMP_PAINTER_FILTER_PATHS_OVERLAY_DIR &&
         directory == joined (GIMP_PAINTER_FILTER_PATHS_OVERLAY_DIR, ".");
}

std::string checked (const std::string& candidate)
{
  if (!g_path_is_absolute (candidate.c_str ()) ||
      !g_file_test (candidate.c_str (), G_FILE_TEST_IS_REGULAR) ||
      !g_file_test (candidate.c_str (), G_FILE_TEST_IS_EXECUTABLE))
    throw std::runtime_error ("Bundled Filter executable is missing or not executable: " + candidate);
  return candidate;
}
}

std::string filter_worker_path ()
{
  const auto directory = executable_directory ();
  constexpr const char *name = "gimp-painter-filter-worker" GIMP_PAINTER_FILTER_EXECUTABLE_SUFFIX;
  if (overlay_location (directory)) return checked (joined (directory, name));
  if (build_location (directory))
    return checked (joined (joined (GIMP_PAINTER_FILTER_BUILD_ROOT, "app"), name));
  /* Installed hosts use their configured relative bin-to-libexec layout.
   * In particular, never prefer a still-present developer build after moving
   * an installation to another directory or launching through a symlink. */
  return checked (joined (directory, GIMP_PAINTER_FILTER_BIN_TO_WORKER));
}

std::string filter_plugin_path (FilterProcedure procedure)
{
  const char *name = nullptr;
  switch (procedure)
    {
    case FilterProcedure::blinds: name = "blinds"; break;
    default: throw std::invalid_argument ("No bundled executable for this Filter procedure");
    }
  const auto directory = executable_directory ();
  const auto executable = std::string (name) + GIMP_PAINTER_FILTER_EXECUTABLE_SUFFIX;
  if (overlay_location (directory)) return checked (joined (directory, executable.c_str ()));
  if (build_location (directory))
    return checked (joined (joined (GIMP_PAINTER_FILTER_BUILD_ROOT, "plug-ins/common"), executable.c_str ()));
  return checked (joined (joined (joined (directory, GIMP_PAINTER_FILTER_WORKER_TO_PLUGINS), name), executable.c_str ()));
}
}
