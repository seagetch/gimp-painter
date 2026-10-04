/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-wire.hpp"
#include <glib.h>
#include <functional>
#include <cstring>
#include <utility>
#include <iostream>
#include <thread>
using namespace GimpPainter;
using Disposition = FilterProcedureDisposition;
static void rejects (const std::function<void ()>& call) {
  bool rejected = false; try { call (); } catch (const std::exception&) { rejected = true; } g_assert (rejected);
}
static void put32 (FilterWire::Frame& frame, std::size_t at, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) frame.payload[at + i] = std::uint8_t (value >> (8 * i));
}
static void progress_cases () {
  using Event = FilterProgress::Event;
  using Type = FilterWire::Type;
  FilterProgress channel; FilterProgress::Snapshot s;
  g_assert (channel.try_snapshot (s) && !s.revision && !s.active);
  std::array<char,512> unterminated; unterminated.fill ('x');
  channel.start (true, unterminated.data ());
  g_assert (channel.try_snapshot (s) && s.active && s.cancellable && s.text[511] == 'x' && !s.text[512]);
  std::array<char,513> utf; utf.fill ('x'); utf[511] = char (0xe2); utf[512] = 0;
  channel.set_text (utf.data ());
  g_assert (channel.try_snapshot (s) && !s.text[511]);
  channel.set_text ("ok\xc0\xaf"); channel.set_value (2);
  g_assert (channel.try_snapshot (s) && s.value == 1 && !std::strcmp (s.text.data (), "ok??"));
  const auto revision = s.revision;
  channel.set_value (std::numeric_limits<double>::quiet_NaN ());
  channel.set_value (std::numeric_limits<double>::infinity ());
  g_assert (channel.try_snapshot (s) && s.revision == revision && s.value == 1);
  channel.set_value (-1); channel.pulse (7); channel.message (4, "domain", "message"); channel.end ();
  g_assert (channel.try_snapshot (s) && !s.active && !s.cancellable && s.value == 0 && s.pulses == 7 &&
    s.message_revision == 1 && s.message_severity == 4 && !std::strcmp (s.message_domain.data (), "domain"));
  channel.reset (); g_assert (channel.try_snapshot (s) && !s.revision && !s.message_revision && !s.pulses);
  std::atomic<bool> running {true};
  std::thread worker ([&] { for (unsigned i = 0; i < 10000; ++i) channel.set_value (i / 10000.0); running = false; });
  while (running) if (channel.try_snapshot (s)) g_assert (std::isfinite (s.value) && s.value >= 0 && s.value <= 1);
  worker.join ();

  std::vector<FilterWire::Frame> frames;
  FilterWire::ProgressWriter writer ([&] (const FilterWire::Frame& f) { frames.push_back (f); });
  const auto now = FilterWire::ProgressWriter::Clock::now ();
  s = {}; s.cancellable = true; FilterProgress::copy_text (s.text, "native start");
  writer.update (Event::start, s, now);
  for (unsigned i = 1; i <= 10000; ++i) {
    s.value = double (i) / 10000; s.pulses = i;
    writer.update (Event::value, s, now); writer.update (Event::pulse, s, now);
  }
  FilterProgress::copy_text (s.text, "native text"); writer.update (Event::text, s, now);
  s.message_severity = 2; FilterProgress::copy_text (s.message_domain, "plug-in");
  FilterProgress::copy_text (s.message_text, "native message"); writer.update (Event::message, s, now);
  g_assert (frames.size () == 1); // bounded coalescing, with final updates retained
  writer.update (Event::end, s, now); writer.finish ();
  g_assert (frames.size () == 6);
  channel.reset (); FilterWire::Result result (4);
  for (const auto& f : frames) {
    auto bytes = FilterWire::encode (f); const auto decoded = FilterWire::decode (bytes.data (), bytes.size ());
    result.accept (decoded); FilterWire::publish_progress (decoded, channel);
  }
  g_assert (channel.try_snapshot (s) && !s.active && s.value == 1 && s.pulses == 10000 &&
    s.message_revision == 1 && !std::strcmp (s.text.data (), "native text"));
  result.accept ({Type::output,0,{1,2,3,4}}); result.accept (FilterWire::success (4,Disposition::merged)); result.finish ();
  rejects ([&] { result.accept (frames.front ()); });
  {
    FilterWire::Result r (4);
    rejects ([&] { r.accept (frames[1]); }); // text without a start
    r.accept (frames.front ());
    auto duplicate = frames.front (); duplicate.offset = 2;
    rejects ([&] { r.accept (duplicate); });
    auto gap = frames[1]; gap.offset = 3; rejects ([&] { r.accept (gap); });
    r.accept ({Type::output,0,{1,2,3,4}});
    rejects ([&] { r.accept (FilterWire::success (4,Disposition::merged)); }); // active at terminal
  }
  for (const auto& malformed : std::vector<FilterWire::Frame> {
      {Type::progress_start,1,{2}}, {Type::progress_start,1,{}}, {Type::progress_end,1,{0}},
      {Type::progress_text,1,{0}}, {Type::progress_text,1,{0xc0,0xaf}},
      {Type::progress_text,1,{0xed,0xa0,0x80}}, {Type::progress_text,1,{0xf4,0x90,0x80,0x80}},
      {Type::progress_text,1,std::vector<std::uint8_t> (513,'x')},
      {Type::progress_pulse,1,std::vector<std::uint8_t> (8)},
      {Type::progress_message,1,{5,0,0,0,0,0}}, {Type::progress_message,1,{0,0,0,0,129,0}},
      {Type::progress_message,1,{0,0,0,0,1,0}}, {Type::progress_message,1,{0,0,0,0,0,0,0xff}} })
    rejects ([&] { FilterWire::encode (malformed); });
  for (double value : {-0.1,1.1,std::numeric_limits<double>::infinity (),std::numeric_limits<double>::quiet_NaN ()}) {
    auto f = frames[2]; std::uint64_t bits; std::memcpy (&bits, &value, 8);
    for (unsigned i = 0; i < 8; ++i) f.payload[i] = std::uint8_t (bits >> (8*i));
    rejects ([&] { FilterWire::encode (f); });
  }
  { // Decoder checks content even if an untrusted sender bypasses encode().
    auto raw = FilterWire::encode (frames.front ()); raw[FilterWire::header_size] = 3;
    rejects ([&] { FilterWire::decode (raw.data (), raw.size ()); });
  }
  { FilterWire::Result r (4); r.accept (frames.front ());
    FilterWire::Frame pulses {Type::progress_pulse,2,std::vector<std::uint8_t> (8,255)}; r.accept (pulses);
    pulses.offset = 3; rejects ([&] { r.accept (pulses); }); }
  { FilterWire::ProgressBudget budget (now);
    for (unsigned i = 0; i < FilterWire::ProgressBudget::burst; ++i) g_assert (budget.admit (now));
    g_assert (!budget.admit (now));
    for (unsigned day = 1; day <= 10000; ++day) g_assert (budget.admit (now + std::chrono::hours (24*day)));
  }
  { // Valid accumulated reports must survive a delayed consumer draining fast.
    FilterWire::Result r (4); r.accept (frames.front ());
    auto value = frames[2];
    for (unsigned i = 2; i <= 4096; ++i) { value.offset = i; r.accept (value); }
    r.accept ({Type::progress_end,4097,{}});
    r.accept ({Type::output,0,{1,2,3,4}}); r.accept (FilterWire::success (4,Disposition::merged)); r.finish ();
  }
}
int main () {
  progress_cases ();
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
  auto bad = req; bad.procedure = FilterProcedure (5); rejects ([&] { FilterWire::request (bad); });
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
  for (std::uint32_t tag : {0u,5u,0xffffffffu}) {
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
  for (unsigned version : {1u,2u,3u,4u,5u}) { auto old = bytes; old[3] = '0' + version; old[4] = version; rejects ([&] { FilterWire::decode (old.data (), old.size ()); }); }
  for (unsigned old_size : {36u,60u,64u,88u})
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

  FilterProcedureRequest retinex; retinex.procedure = FilterProcedure::retinex;
  retinex.width = 53; retinex.height = 41;
  for (unsigned storage : {3u,4u}) for (int scale : {16,250,256}) for (int nscales : {0,1,8})
    for (int mode : {0,1,2}) for (double cvar : {0.0,-0.0,0.123456789,1.2,4.0}) {
      retinex.storage_channels = storage; retinex.scale = scale;
      retinex.nscales = nscales; retinex.scales_mode = mode; retinex.cvar = cvar;
      const auto frame = FilterWire::encode (FilterWire::request (retinex));
      const auto decoded = FilterWire::request (FilterWire::decode (frame.data (),frame.size ()));
      g_assert (decoded.procedure == FilterProcedure::retinex && decoded.storage_channels == storage &&
                decoded.scale == scale && decoded.nscales == nscales && decoded.scales_mode == mode &&
                !std::memcmp (&decoded.cvar,&cvar,sizeof cvar));
      g_assert (decoded.scratch_bytes () == 53u*41u*(storage*5u+8u)+8u*(53u+3u));
    }
  for (std::uint32_t storage : {0u,1u,2u,5u,0xffffffffu}) {
    auto frame = FilterWire::request (retinex); put32 (frame,60,storage);
    rejects ([&] { FilterWire::request (frame); });
  }
  for (auto entry : {std::pair<std::size_t,std::uint32_t>{64,15}, {64,257}, {68,9}, {68,0xffffffffu},
                     {72,3}, {72,0xffffffffu}, {76,1}, {56,3}, {12,1}, {16,2}, {20,1}, {24,1}, {28,1}}) {
    auto frame = FilterWire::request (retinex); put32 (frame,entry.first,entry.second);
    rejects ([&] { FilterWire::request (frame); });
  }
  for (double value : {-1.0,4.000000000000001,std::numeric_limits<double>::infinity (),
                        -std::numeric_limits<double>::infinity (),std::numeric_limits<double>::quiet_NaN ()}) {
    auto invalid = retinex; invalid.cvar = value; rejects ([&] { FilterWire::request (invalid); });
    auto frame = FilterWire::request (retinex);
    std::uint64_t bits; std::memcpy (&bits,&value,8);
    for (unsigned i = 0; i < 8; ++i) frame.payload[80+i] = std::uint8_t (bits >> (8*i));
    rejects ([&] { FilterWire::request (frame); });
  }
  for (const FilterSelectionRegion& region : {FilterSelectionRegion {true,true,0,0,15,41},
      FilterSelectionRegion {true,true,0,0,53,15}, FilterSelectionRegion {true,false,0,0,0,0}}) {
    auto invalid = retinex; invalid.start_region = region; rejects ([&] { FilterWire::request (invalid); });
  }
  { auto native_overflow = retinex; native_overflow.width = 32768; native_overflow.height = 16384;
    rejects ([&] { FilterWire::request (native_overflow); });
    native_overflow.storage_channels = 3;
    g_assert (native_overflow.bytes () == 2147483648ull);
    native_overflow.height = 21846;
    rejects ([&] { FilterWire::request (native_overflow); }); }
  for (std::size_t at : {std::size_t(60),std::size_t(64),std::size_t(68),std::size_t(72),std::size_t(76),std::size_t(80)}) {
    auto frame = FilterWire::request (tile); put32 (frame,at,99);
    rejects ([&] { FilterWire::request (frame); });
  }

  FilterProcedureRequest convolution; convolution.procedure = FilterProcedure::convolution;
  convolution.width = 9; convolution.height = 8;
  for (unsigned mode : {0u,1u,2u,3u}) for (unsigned storage : {1u,2u,3u,4u}) {
    auto r = convolution; r.sample_mode = mode; r.storage_channels = storage; r.gray = storage <= 2;
    r.matrix[0] = 0.123456789012345; r.matrix[24] = -0.0000000000123;
    r.channels = {{-17,0,1,2,2147483647}}; r.alpha_alg = -51;
    r.divisor = -0.345678901234567; r.offset = 3.141592653589793;
    const auto frame = FilterWire::encode (FilterWire::request (r));
    const auto decoded = FilterWire::request (FilterWire::decode (frame.data (),frame.size ()));
    g_assert (decoded.matrix == r.matrix && decoded.channels == r.channels && decoded.alpha_alg == r.alpha_alg &&
      decoded.sample_mode == mode && decoded.storage_channels == storage && decoded.divisor == r.divisor && decoded.offset == r.offset);
    g_assert (decoded.bytes () == 9*8*(mode ? 32 : 4));
  }
  for (double value : {std::numeric_limits<double>::quiet_NaN (),std::numeric_limits<double>::infinity ()}) {
    for (unsigned mode : {0u,1u}) {
      auto r = convolution; r.sample_mode = mode; r.matrix[17] = value;
      rejects ([&] { FilterWire::request (r); });
      r = convolution; r.sample_mode = mode; r.divisor = value; rejects ([&] { FilterWire::request (r); });
      r = convolution; r.sample_mode = mode; r.offset = value; rejects ([&] { FilterWire::request (r); });
    }
  }
  for (double divisor : {0.0,-0.0,1e-310,1e100}) {
    auto r = convolution; r.divisor = divisor; rejects ([&] { FilterWire::request (r); });
  }
  for (auto entry : {std::pair<std::size_t,std::uint32_t>{88,4},{96,3},{100,1},{340,1},{60,1}}) {
    auto frame = FilterWire::request (convolution); put32 (frame,entry.first,entry.second);
    rejects ([&] { FilterWire::request (frame); });
  }
  {
    auto r = convolution; r.sample_mode = 1;
    auto frame = FilterWire::request (r); put32 (frame,100,frame.payload[100] == 1 ? 2 : 1);
    rejects ([&] { FilterWire::request (frame); }); // explicitly reject the other host byte order
    FilterWire::Result aligned (32,true,false,32);
    rejects ([&] { aligned.accept ({FilterWire::Type::output,0,std::vector<std::uint8_t> (4)}); });
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
