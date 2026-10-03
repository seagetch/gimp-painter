/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_GIMP_FILTER_PATHS_HPP
#define GIMP_PAINTER_GIMP_FILTER_PATHS_HPP
#include "painter/filter-procedure.hpp"
#include <string>
namespace GimpPainter {
/* Resolve fixed bundled executables from this process's actual location.
 * These functions may perform filesystem I/O; call on the independent worker
 * or private helper, never during an owner/main-context preparation quantum.
 * No GIMP object, saved definition, environment path or registry is consulted.
 */
std::string filter_worker_path ();
std::string filter_plugin_path (FilterProcedure procedure);
}
#endif
