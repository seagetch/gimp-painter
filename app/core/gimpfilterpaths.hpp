/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_GIMP_FILTER_PATHS_HPP
#define GIMP_PAINTER_GIMP_FILTER_PATHS_HPP
#include "../painter/gimp-painter-visibility.h"
#include "painter/filter-procedure.hpp"
#include <string>
namespace GimpPainter GIMP_PAINTER_PRIVATE {
/* Resolve fixed bundled executables from this process's actual location.
 * These functions may perform filesystem I/O; call on the independent worker
 * or private helper, never during an owner/main-context preparation quantum.
 * No GIMP object, saved definition, environment path or registry is consulted.
 */
std::string filter_worker_path ();
std::string filter_plugin_path (FilterProcedure procedure);
/* Owner metadata provenance only: derive the fixed bundled path from the
 * running host and configured layout, without stat/query/launch. This does not
 * establish executable availability and must not replace the worker checks. */
std::string filter_registered_plugin_path (FilterProcedure procedure);
}
#endif
