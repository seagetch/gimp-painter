/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-wire.hpp"
#include <glib.h>
#include <functional>
#include <iostream>
using namespace GimpPainter;
using Disposition = FilterProcedureDisposition;
static void rejects (const std::function<void ()>& call) {
  bool rejected = false; try { call (); } catch (const std::exception&) { rejected = true; } g_assert (rejected);
}
static void put32 (FilterWire::Frame& frame, std::size_t at, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) frame.payload[at + i] = std::uint8_t (value >> (8 * i));
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
  auto bad = req; bad.procedure = FilterProcedure (3); rejects ([&] { FilterWire::request (bad); });
  bad = req; bad.width = 0; rejects ([&] { FilterWire::request (bad); });
  g_assert (!restored.raw_shadow && !restored.start_region.selected && restored.start_region.intersects &&
            restored.start_region.x1 == 0 && restored.start_region.y1 == 0 &&
            restored.start_region.x2 == 13 && restored.start_region.y2 == 7);
  for (const FilterSelectionRegion& region : {
        FilterSelectionRegion {true,true,2,1,12,6}, FilterSelectionRegion {true,false,13,1,13,6},
        FilterSelectionRegion {true,false,0,0,0,0}, FilterSelectionRegion {true,false,2,7,12,7}}) {
    auto selected = req; selected.raw_shadow = true; selected.start_region = region;
    const auto decoded = FilterWire::request (FilterWire::request (selected));
    g_assert (decoded.raw_shadow && decoded.start_region.selected &&
              decoded.start_region.intersects == region.intersects && decoded.start_region.x1 == region.x1 &&
              decoded.start_region.y1 == region.y1 && decoded.start_region.x2 == region.x2 && decoded.start_region.y2 == region.y2);
  }
  for (const FilterSelectionRegion& region : {
        FilterSelectionRegion {true,true,-1,0,12,6}, FilterSelectionRegion {true,true,0,-1,12,6},
        FilterSelectionRegion {true,true,2,1,14,6}, FilterSelectionRegion {true,true,2,1,12,8},
        FilterSelectionRegion {true,true,2,1,1,6}, FilterSelectionRegion {true,true,2,2,12,1},
        FilterSelectionRegion {true,true,2,1,2,6}, FilterSelectionRegion {true,false,2,1,12,6}}) {
    bad = req; bad.start_region = region; rejects ([&] { FilterWire::request (bad); });
  }
  for (unsigned dimension : {0u,524289u,0xffffffffu}) {
    bad = req; bad.width = dimension; rejects ([&] { FilterWire::request (bad); });
    bad = req; bad.height = dimension; rejects ([&] { FilterWire::request (bad); });
  }
  for (std::uint32_t tag : {0u,3u,0xffffffffu}) {
    auto f = FilterWire::request (req); put32 (f,0,tag); rejects ([&] { FilterWire::request (f); });
  }
  for (std::uint32_t flags : {0u,1u,8u,0xffffffffu}) {
    auto f = FilterWire::request (req); put32 (f,36,flags); rejects ([&] { FilterWire::request (f); });
  }
  for (std::size_t at : {std::size_t (40),std::size_t (44),std::size_t (48),std::size_t (52),std::size_t (56),std::size_t (60)}) {
    auto f = FilterWire::request (req); put32 (f,at,1); rejects ([&] { FilterWire::request (f); });
  }
  { auto f = FilterWire::request (req); put32 (f,28,2); rejects ([&] { FilterWire::request (f); }); }
  { auto f = FilterWire::request (req); put32 (f,36,7); put32 (f,40,0xffffffffu);
    rejects ([&] { FilterWire::request (f); }); }
  for (unsigned version : {1u,2u}) { auto old = bytes; old[3] = '0' + version; old[4] = version; rejects ([&] { FilterWire::decode (old.data (), old.size ()); }); }
  for (unsigned old_size : {36u,60u})
    rejects ([&] { FilterWire::encode ({FilterWire::Type::request, 0, std::vector<std::uint8_t> (old_size)}); });
  FilterProcedureRequest tile; tile.procedure = FilterProcedure::small_tiles; tile.width = 67; tile.height = 66;
  for (int factor = 0; factor <= 6; ++factor) for (bool gray : {false,true}) for (bool raw : {false,true}) {
    tile.tiles = factor; tile.gray = gray; tile.raw_shadow = raw;
    tile.start_region = {true,true,1,2,64,63};
    auto frame = FilterWire::request (tile);
    const auto encoded = FilterWire::encode (frame);
    const auto decoded = FilterWire::request (FilterWire::decode (encoded.data (),encoded.size ()));
    g_assert (decoded.procedure == tile.procedure && decoded.tiles == factor && decoded.gray == gray &&
              decoded.raw_shadow == raw && decoded.start_region.x1 == 1 && decoded.start_region.y2 == 63);
  }
  {
    auto huge = tile; huge.width = 524288; huge.height = 524288; huge.start_region = {};
    rejects ([&] { FilterWire::request (huge); });
    huge.start_region = {true,true,0,0,1,524288};
    g_assert (FilterWire::request (FilterWire::request (huge)).bytes () == 1099511627776ull);
    auto frame = FilterWire::request (huge); put32 (frame,48,524288);
    rejects ([&] { FilterWire::request (frame); });
  }
  for (std::uint32_t factor : {7u,0xffffffffu,0x80000000u}) {
    auto frame = FilterWire::request (tile); put32 (frame,56,factor);
    rejects ([&] { FilterWire::request (frame); });
  }
  for (std::size_t at : {std::size_t(12),std::size_t(16),std::size_t(20),std::size_t(24),std::size_t(60)}) {
    auto frame = FilterWire::request (tile); put32 (frame,at,99);
    rejects ([&] { FilterWire::request (frame); });
  }
  rejects ([&] { FilterWire::encode ({FilterWire::Type::success, 8, {}}); });
  rejects ([&] { FilterWire::success (8,Disposition::pending); });
  rejects ([&] { FilterWire::success (8,Disposition (4)); });
  FilterWire::Result result (8);
  rejects ([&] { result.finish (); });
  rejects ([&] { result.accept ({FilterWire::Type::output, 4, {1,2,3,4}}); });
  result.accept ({FilterWire::Type::output, 0, {1,2,3,4}});
  rejects ([&] { result.accept (FilterWire::success (8,Disposition::merged)); });
  rejects ([&] { result.accept ({FilterWire::Type::output, 4, std::vector<std::uint8_t> (8)}); });
  result.accept ({FilterWire::Type::output, 4, {5,6,7,8}});
  result.accept (FilterWire::success (8,Disposition::merged)); result.finish ();
  rejects ([&] { result.accept (FilterWire::success (8,Disposition::merged)); });
  rejects ([&] { result.accept ({FilterWire::Type::output, 8, {5,6,7,8}}); });
  rejects ([&] { FilterWire::encode ({FilterWire::Type::failure, 0, std::vector<std::uint8_t> (FilterWire::metadata_limit+1)}); });
  rejects ([&] { FilterWire::encode ({FilterWire::Type::input, 0, std::vector<std::uint8_t> (FilterWire::pixel_limit+4)}); });
  for (bool raw : {false,true}) for (bool empty : {false,true}) {
    FilterWire::Result typed (4,raw,empty);
    const auto expected = empty ? Disposition::no_merge : raw ? Disposition::shadow : Disposition::merged;
    g_assert (typed.disposition () == Disposition::pending);
    typed.accept ({FilterWire::Type::output,0,{1,2,3,4}});
    for (std::uint32_t value : {0u,1u,2u,3u,4u,0xffffffffu}) {
      if (value == std::uint32_t (expected)) continue;
      auto f = FilterWire::success (4,expected); put32 (f,0,value);
      rejects ([&] { typed.accept (f); });
      g_assert (!typed.terminal () && typed.disposition () == Disposition::pending);
    }
    typed.accept (FilterWire::success (4,expected)); typed.finish ();
    g_assert (typed.disposition () == expected);
    rejects ([&] { typed.accept (FilterWire::success (4,expected)); });
  }
  std::cout << "Filter wire validation passed\n";
}
