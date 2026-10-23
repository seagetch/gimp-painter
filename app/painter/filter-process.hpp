/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_PROCESS_HPP
#define GIMP_PAINTER_FILTER_PROCESS_HPP
#include "filter-procedure.hpp"
#include "filter-raster.hpp"
#include "filter-progress.hpp"
#include <memory>
#include <string>
namespace GimpPainter {
/* executable is supplied exclusively by the compiled app adapter, never by a
 * Filter definition, wire frame, XCF, plugin, or procedure argument. */
struct FilterProcessOptions { std::string executable; std::string temporary_directory; };
bool filter_process (const FilterProcedureRequest&, FilterRaster&, FilterRaster&,
                     std::atomic<bool>&, const FilterProcessOptions&,
                     std::shared_ptr<FilterProcedureResult> result = {},
                     std::shared_ptr<FilterProgress> progress = {});
}
#endif
