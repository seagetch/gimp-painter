/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_GIMP_FILTER_PROCEDURE_HPP
#define GIMP_PAINTER_GIMP_FILTER_PROCEDURE_HPP
#include "painter/filter-procedure.hpp"
#include "painter/filter-raster.hpp"
namespace GimpPainter {
/* Called once on the private helper main thread after private directories and
 * environment are established. Pending is cancellation; validation,
 * execution and storage failures throw. Output is staging, never a live cache. */
FilterProcedureDisposition run_filter_procedure (const FilterProcedureRequest& request,
                           FilterRaster& input, FilterRaster& output,
                           std::atomic<bool>& cancel);
}
#endif
