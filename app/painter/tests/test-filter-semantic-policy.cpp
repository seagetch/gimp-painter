/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-procedure.hpp"
#include "baseline-filter-procedure.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
using GimpPainter::FilterProcedureRequest;
using GimpPainter::FilterProcedure;
std::size_t cases = 0;
struct Result {
  std::string error;
  std::uint64_t bytes = 0, scratch = 0;
  bool operator == (const Result& other) const
  { return error == other.error && bytes == other.bytes && scratch == other.scratch; }
};
template<typename Request> Result result (const Request& request)
{
  Result r;
  try { r.bytes = request.bytes (); r.scratch = request.scratch_bytes (); }
  catch (const std::invalid_argument& error) { r.error = error.what (); }
  return r;
}
template<typename Change> void compare (unsigned route, Change change)
{
  FilterProcedureRequest current;
  BaselineFilterPolicy::FilterProcedureRequest before;
  current.procedure = FilterProcedure (route);
  before.procedure = BaselineFilterPolicy::FilterProcedure (route);
  current.width = before.width = 37; current.height = before.height = 19;
  change (current); change (before);
  const auto a = result (current), b = result (before);
  if (!(a == b)) {
    std::cerr << "Admission/error changed at case " << cases << ": " << a.error << " / " << b.error << '\n';
    std::abort ();
  }
  ++cases;
}
}
int main ()
{
  const std::vector<std::int32_t> integers = {INT32_MIN, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16,
    17, 89, 90, 91, 99, 100, 101, 239, 240, 249, 250, 251, 255, 256, 257, 1024, INT32_MAX};
  const double tiny = std::numeric_limits<double>::denorm_min ();
  const double fmax = std::numeric_limits<float>::max ();
  const std::vector<double> numbers = {-std::numeric_limits<double>::infinity (), -std::numeric_limits<double>::max (),
    -fmax, -4, -1, -std::numeric_limits<float>::denorm_min (), -tiny, -0.0, 0.0, tiny,
    std::numeric_limits<float>::denorm_min (), 1, 1.2, 4, std::nextafter (4.0, 5.0), fmax,
    std::nextafter (fmax, std::numeric_limits<double>::infinity ()), std::numeric_limits<double>::max (),
    std::numeric_limits<double>::infinity (), std::numeric_limits<double>::quiet_NaN ()};
  for (unsigned route = 0; route <= 5; ++route) {
    compare (route, [] (auto&) {});
    for (auto value : integers) {
#define INTEGER_FIELD(field) compare (route, [=] (auto& r) { r.field = value; })
      INTEGER_FIELD (angle); INTEGER_FIELD (segments); INTEGER_FIELD (orientation); INTEGER_FIELD (transparent);
      INTEGER_FIELD (tiles); INTEGER_FIELD (scale); INTEGER_FIELD (nscales); INTEGER_FIELD (scales_mode);
      INTEGER_FIELD (alpha_alg); INTEGER_FIELD (border);
#undef INTEGER_FIELD
      for (unsigned i = 0; i < 5; ++i) compare (route, [=] (auto& r) { r.channels[i] = value; });
    }
    for (double value : numbers) {
      compare (route, [=] (auto& r) { r.cvar = value; });
      compare (route, [=] (auto& r) { r.divisor = value; });
      compare (route, [=] (auto& r) { r.offset = value; });
      for (unsigned i = 0; i < 25; ++i) compare (route, [=] (auto& r) { r.matrix[i] = value; });
    }
    for (std::uint32_t dimension : {0u, 1u, 2u, 3u, 15u, 16u, 17u, 32768u, 524288u, 524289u, UINT32_MAX}) {
      compare (route, [=] (auto& r) { r.width = dimension; });
      compare (route, [=] (auto& r) { r.height = dimension; });
    }
    for (std::uint32_t storage = 0; storage <= 5; ++storage)
      for (std::uint32_t mode = 0; mode <= 4; ++mode) for (bool gray : {false, true})
        compare (route, [=] (auto& r) { r.storage_channels = storage; r.sample_mode = mode; r.gray = gray; });
    for (GimpPainter::FilterSelectionRegion region : {
        GimpPainter::FilterSelectionRegion {true, true, 0, 0, 16, 16}, {true, true, 0, 0, 15, 16},
        {true, false, 0, 0, 0, 0}, {true, true, -1, 0, 16, 16}, {true, true, 0, 0, 38, 19},
        {true, true, 0, 0, 0, 16}, {false, false, -1, -1, -1, -1}})
      compare (route, [=] (auto& r) { r.start_region = region; });
  }
  /* Matrix and offset may narrow to float zero; the divisor may not. Native
   * double admits the same finite nonzero divisor without narrowing. */
  for (unsigned mode = 0; mode <= 3; ++mode) for (double matrix : numbers)
    for (double divisor : numbers) for (double offset : {-tiny, -0.0, tiny})
      compare (4, [=] (auto& r) { r.sample_mode = mode; r.matrix[7] = matrix; r.divisor = divisor; r.offset = offset; });
  for (unsigned storage : {3u, 4u}) for (unsigned height : {16383u, 16384u, 21845u, 21846u})
    for (bool selected : {false, true}) compare (3, [=] (auto& r) {
      r.width = 32768; r.height = height; r.storage_channels = storage;
      if (selected) r.start_region = {true, true, 0, 0, 16, 16};
    });

  namespace policy = GimpPainter::FilterLegacy;
  for (unsigned width : {0u, 15u, 16u, 32768u, 524288u, UINT32_MAX})
    for (unsigned height : {0u, 15u, 16u, 16383u, 16384u, 524288u, UINT32_MAX}) {
      const bool before = width >= 16 && height >= 16 && std::uint64_t (width) * height <= std::uint64_t (INT32_MAX) / 4;
      assert (policy::retinex_owner_geometry (width, height) == before);
    }
  for (auto value : integers) {
    assert (!std::strcmp (policy::blinds_orientation (value), value == 1 ? "vertical" : "horizontal"));
    assert (policy::enabled (value) == (value != 0));
  }
  assert (!std::strcmp (policy::small_tiles.saved, "plug-in-small-tiles"));
  assert (!std::strcmp (policy::small_tiles.execution, "plug-in-painter-small-tiles"));
  assert (!std::strcmp (policy::retinex.saved, "plug-in-retinex"));
  assert (!std::strcmp (policy::retinex.execution, "plug-in-painter-retinex"));
  assert (!std::strcmp (policy::convolution.saved, "plug-in-convmatrix"));
  assert (!std::strcmp (policy::convolution.execution, "plug-in-painter-convmatrix"));
  assert (policy::matrix_count == 25 && policy::channel_count == 5);
  for (unsigned count = 0; count < 14; ++count) assert (policy::convolution_saved_count (count) == (count == 11 || count == 12));
  const FilterProcedureRequest defaults;
  const BaselineFilterPolicy::FilterProcedureRequest prior;
  assert (defaults.angle == prior.angle && defaults.segments == prior.segments && defaults.tiles == prior.tiles &&
    defaults.scale == prior.scale && defaults.nscales == prior.nscales && defaults.scales_mode == prior.scales_mode &&
    defaults.cvar == prior.cvar && defaults.alpha_alg == prior.alpha_alg && defaults.border == prior.border &&
    defaults.divisor == prior.divisor && defaults.offset == prior.offset && defaults.matrix == prior.matrix && defaults.channels == prior.channels);
  std::cout << cases << " pre-refactor/current request domain, reservation and exact-error comparisons passed\n";
}
