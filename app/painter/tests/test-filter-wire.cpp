/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-wire.hpp"
#include <glib.h>
#include <functional>
#include <iostream>
using namespace GimpPainter;
static void rejects (const std::function<void ()>& call) {
  bool rejected = false; try { call (); } catch (const std::exception&) { rejected = true; } g_assert (rejected);
}
int main () {
  FilterProcedureRequest req; req.width = 13; req.height = 7; req.angle = 90; req.segments = 100;
  req.orientation = -17; req.transparent = -1; req.gray = true; req.background = {{17,17,17,255}};
  const auto bytes = FilterWire::encode (FilterWire::request (req));
  const auto restored = FilterWire::request (FilterWire::decode (bytes.data (), bytes.size ()));
  g_assert (restored.bytes () == 364 && restored.orientation == -17 && restored.transparent == -1 && restored.gray && restored.background == req.background);
  for (unsigned pos : {0,4,5,6,7,8,9,10,11,20,21,22,23}) {
    auto bad = bytes; bad[pos] ^= 128; rejects ([&] { FilterWire::decode (bad.data (), bad.size ()); });
  }
  for (std::size_t n = 0; n < bytes.size (); ++n) rejects ([&] { FilterWire::decode (bytes.data (), n); });
  auto trailing = bytes; trailing.push_back (0); rejects ([&] { FilterWire::decode (trailing.data (), trailing.size ()); });
  for (int angle : {-1,91}) { auto bad = req; bad.angle = angle; rejects ([&] { FilterWire::request (bad); }); }
  for (int segments : {0,101}) { auto bad = req; bad.segments = segments; rejects ([&] { FilterWire::request (bad); }); }
  auto bad = req; bad.procedure = FilterProcedure (2); rejects ([&] { FilterWire::request (bad); });
  bad = req; bad.width = 0; rejects ([&] { FilterWire::request (bad); });
  FilterWire::Result result (8);
  rejects ([&] { result.finish (); });
  rejects ([&] { result.accept ({FilterWire::Type::output, 4, {1,2,3,4}}); });
  result.accept ({FilterWire::Type::output, 0, {1,2,3,4}});
  rejects ([&] { result.accept ({FilterWire::Type::success, 8, {}}); });
  rejects ([&] { result.accept ({FilterWire::Type::output, 4, std::vector<std::uint8_t> (8)}); });
  result.accept ({FilterWire::Type::output, 4, {5,6,7,8}});
  result.accept ({FilterWire::Type::success, 8, {}}); result.finish ();
  rejects ([&] { result.accept ({FilterWire::Type::success, 8, {}}); });
  rejects ([&] { result.accept ({FilterWire::Type::output, 8, {5,6,7,8}}); });
  rejects ([&] { FilterWire::encode ({FilterWire::Type::failure, 0, std::vector<std::uint8_t> (FilterWire::metadata_limit+1)}); });
  rejects ([&] { FilterWire::encode ({FilterWire::Type::input, 0, std::vector<std::uint8_t> (FilterWire::pixel_limit+4)}); });
  std::cout << "Filter wire validation passed\n";
}
