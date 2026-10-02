/* GIMP - The GNU Image Manipulation Program
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Genuine fixtures are checked separately from explicitly synthetic wire data.
 */
#include "../painter-xcf-compat.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

using namespace gimp::painter::xcf;
using Buffer = std::vector<std::uint8_t>;
static unsigned checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { std::cerr << __LINE__ << ": " #expr "\n"; std::exit (1); } } while (false)

static Bytes bytes (const Buffer &b) { return { b.data (), b.size () }; }
static Buffer read (const std::string &path)
{
  std::ifstream f (path, std::ios::binary);
  CHECK (f.good ());
  return Buffer (std::istreambuf_iterator<char> (f), std::istreambuf_iterator<char> ());
}
static void u32 (Buffer &b, std::uint32_t n)
{
  b.push_back (n >> 24); b.push_back (n >> 16); b.push_back (n >> 8); b.push_back (n);
}
static void set32 (Buffer &b, std::size_t at, std::uint32_t n)
{
  b.at (at) = n >> 24; b.at (at + 1) = n >> 16;
  b.at (at + 2) = n >> 8; b.at (at + 3) = n;
}
static void str (Buffer &b, const std::string &s)
{
  u32 (b, s.size () + 1); b.insert (b.end (), s.begin (), s.end ()); b.push_back (0);
}
static void end (Buffer &b) { u32 (b, 0); u32 (b, 0); }
static void property (Buffer &b, unsigned tag, std::uint32_t value)
{
  u32 (b, tag); u32 (b, 4); u32 (b, value);
}
static Buffer minimal (unsigned version, bool standard)
{
  Buffer b { 'g','i','m','p',' ','x','c','f',' ','v',
             std::uint8_t ('0' + version / 100),
             std::uint8_t ('0' + version / 10 % 10),
             std::uint8_t ('0' + version % 10), 0 };
  u32 (b, 16); u32 (b, 16); u32 (b, 0);
  if (version >= 4 && standard) u32 (b, version == 4 ? 0 : 150);
  end (b); end (b);
  if (version >= 11 && standard) end (b);
  return b;
}
static Buffer layer (unsigned version, bool standard, const Buffer &props)
{
  auto b = minimal (version, standard);
  CHECK (version < 11);
  const auto table = b.size () - 8;
  b.insert (b.begin () + table, 4, 0);
  set32 (b, table, b.size ());
  u32 (b, 16); u32 (b, 16); u32 (b, 1); str (b, "layer");
  b.insert (b.end (), props.begin (), props.end ());
  end (b); end (b); // properties END, hierarchy=0, mask=0
  return b;
}
static const Property &find (const std::vector<Property> &props, unsigned tag)
{
  for (const auto &p : props) if (p.tag == tag) return p;
  CHECK (false); std::abort ();
}
static std::string text (const Buffer &b, const WireString &s)
{
  if (! s.bytes.size) return {};
  return std::string (reinterpret_cast<const char *> (b.data () + s.bytes.offset),
                      s.bytes.size - (s.has_terminal_nul ? 1 : 0));
}
static void range_ok (Range r, std::size_t size)
{
  CHECK (r.offset <= size); CHECK (r.size <= size - r.offset);
}
static void candidate_ranges (const Candidate &c, std::size_t size)
{
  for (const auto &p : c.image_properties) { range_ok (p.encoded, size); range_ok (p.payload, size); }
  for (const auto *objects : { &c.layers, &c.channels })
    for (const auto &o : *objects)
      {
        range_ok (o.metadata, size); range_ok (o.name.encoded, size);
        for (const auto &p : o.properties) { range_ok (p.encoded, size); range_ok (p.payload, size); }
      }
}

static void genuine_fixtures (const std::string &dir, const std::string &baseline)
{
  auto ordinary = read (dir + "/ordinary-layers.xcf");
  auto p = probe (bytes (ordinary));
  CHECK (ordinary.size () == 3411);
  CHECK (p.selection == Selection::shared_layout);
  CHECK (p.legacy.header.version == 3);
  CHECK (p.legacy.header.properties_offset == 26);
  CHECK (p.legacy.layers.size () == 3);
  CHECK (p.legacy.channels.size () == 2);
  CHECK (p.legacy.image_properties[0].encoded.offset == 26);
  CHECK (p.legacy.image_properties[0].tag == 17);
  CHECK (p.legacy.layer_offset_table.offset == 375);
  CHECK (p.legacy.layers[0].metadata.offset == 403);
  CHECK (p.legacy.layers[1].metadata.offset == 661);
  CHECK (p.legacy.layers[2].metadata.offset == 2625);
  CHECK (p.legacy.layers[1].mask_offset == 2434);
  CHECK (find (p.legacy.layers[1].properties, 30).payload.size == 8);
  CHECK (find (p.legacy.layers[1].properties, 6).payload.offset == 716);
  CHECK (ordinary[719] == 191); // exact saved opacity, NOT 75%
  CHECK (text (ordinary, p.legacy.layers[1].name) == "painted stroke");
  candidate_ranges (p.legacy, ordinary.size ());

  for (const auto &name : { "clone-normal-in-group.xcf", "clone-group.xcf" })
    {
      auto b = read (dir + "/" + name);
      p = probe (bytes (b));
      CHECK (p.selection == Selection::legacy_candidate);
      CHECK (p.standard.diagnostic.status == Status::malformed);
      CHECK (p.standard.diagnostic.offset == 26); // compression tag17 != precision0..4
      CHECK (p.legacy.header.version == 4 && ! p.legacy.header.has_precision);
      CHECK (p.legacy.layers.size () == 3);
      CHECK (p.legacy.layers[0].metadata.offset == 395);
      CHECK (p.legacy.layers[0].hierarchy_offset == 610);
      const auto &prop = find (p.legacy.layers[0].properties, 33);
      CHECK (prop.encoded.offset == 561 && prop.declared_size == 25);
      CHECK (prop.payload.offset == 569 && prop.payload.size == 25);
      const auto clone = decode_clone (bytes (b), prop.payload);
      CHECK (clone.diagnostic.status == Status::ok);
      CHECK (clone.name.bytes.offset == 573 && clone.name.bytes.size == 13);
      CHECK (text (b, clone.name) == (std::string (name) == "clone-group.xcf" ? "source group" : "source child"));
      CHECK (clone.arguments.size () == 1 && clone.arguments[0].tag == 0);
      CHECK (clone.arguments[0].encoded.offset == 586);
      CHECK (clone.tail.size == 0);
      candidate_ranges (p.legacy, b.size ());
      // Every truncation prefix: no out-of-bounds access or partial unsafe ranges.
      for (std::size_t n = 0; n < b.size (); ++n)
        {
          auto q = probe ({ b.data (), n });
          candidate_ranges (q.legacy, n); candidate_ranges (q.standard, n);
        }
    }

  // Genuine old writer output. Old reader CRASHES: this is safe record recovery,
  // not evidence of a successful old-reader roundtrip or of a working port.
  auto filter = read (dir + "/filter-edge.xcf");
  p = probe (bytes (filter));
  CHECK (p.selection == Selection::legacy_candidate);
  const auto &prop = find (p.legacy.layers[0].properties, 32);
  CHECK (prop.encoded.offset == 563 && prop.payload.offset == 571 && prop.payload.size == 89);
  const auto f = decode_filter (bytes (filter), prop.payload);
  CHECK (f.diagnostic.status == Status::ok);
  CHECK (text (filter, f.name) == "plug-in-edge");
  CHECK (f.arguments.size () == 7);
  const unsigned tags[] = { 2, 1, 10, 5, 2, 2, 0 };
  const unsigned offsets[] = { 588, 600, 608, 616, 628, 640, 652 };
  for (unsigned i = 0; i != 7; ++i)
    { CHECK (f.arguments[i].tag == tags[i]); CHECK (f.arguments[i].encoded.offset == offsets[i]); }
  CHECK (f.arguments[1].kind == ArgumentKind::unsupported);
  CHECK (f.arguments[2].kind == ArgumentKind::omitted_drawable);
  CHECK (f.arguments[3].scalar_bits == 0x40000000); // original IEEE32 2.0 bits
  CHECK (f.tail.size == 0);
  for (std::size_t n = 0; n < prop.payload.size; ++n)
    CHECK (decode_filter (bytes (filter), { prop.payload.offset, n }).diagnostic.status != Status::ok);

  auto modern = read (baseline);
  p = probe (bytes (modern));
  CHECK (p.selection == Selection::standard_candidate);
  CHECK (p.standard.header.version == 11 && p.standard.header.precision == 150);
  CHECK (p.standard.header.properties_offset == 30 && p.standard.header.offset_bytes == 8);
  CHECK (! p.standard.layers.empty ());
  candidate_ranges (p.standard, modern.size ());
  std::cout << "genuine: ordinary/shared, two clone/painter, negative filter raw recovery, standard v011 passed\n";
}

static void synthetic_cases ()
{
  auto b = minimal (4, false);
  CHECK (probe (bytes (b)).selection == Selection::legacy_candidate); // standard table truncated
  u32 (b, 0);
  CHECK (probe (bytes (b)).selection == Selection::ambiguous); // both layouts consume legal zero words
  CHECK (probe ({ nullptr, 1 }).selection == Selection::invalid);
  CHECK (probe ({ nullptr, 0 }).selection == Selection::invalid);
  b = minimal (999, true);
  CHECK (probe (bytes (b)).selection == Selection::unsupported);
  b = minimal (0, false);
  b[9] = 'f'; b[10] = 'i'; b[11] = 'l'; b[12] = 'e';
  CHECK (probe (bytes (b)).selection == Selection::shared_layout);
  b[13] = 1; CHECK (probe (bytes (b)).selection == Selection::invalid);

  Buffer props; property (props, 7, 23);
  b = layer (3, false, props);
  CHECK (probe (bytes (b)).selection == Selection::ambiguous);
  for (unsigned i = 0; i < 7; ++i)
    {
      const LegacyMode expected[] = { LegacyMode::erase, LegacyMode::replace,
        LegacyMode::anti_erase, LegacyMode::src_in, LegacyMode::dst_in,
        LegacyMode::src_out, LegacyMode::dst_out };
      CHECK (decode_legacy_mode (23 + i) == expected[i]);
    }
  CHECK (decode_legacy_mode (22) == LegacyMode::common_0_to_22);
  CHECK (decode_legacy_mode (30) == LegacyMode::unknown);

  props.clear (); property (props, 32, 1); property (props, 33, 0x3f000000);
  b = layer (4, true, props);
  auto p = probe (bytes (b));
  CHECK (p.standard.diagnostic.status == Status::ok);
  CHECK (find (p.standard.layers[0].properties, 33).payload.size == 4);
  // Duplicate and unknown ordered raw records; never collapse into a map.
  props.clear (); property (props, 7, 1); property (props, 0xfedcba98, 0xdeadbeef); property (props, 7, 3);
  b = layer (3, false, props);
  p = probe (bytes (b));
  CHECK (p.selection == Selection::shared_layout);
  CHECK (p.legacy.layers[0].properties.size () == 4);
  CHECK (p.legacy.layers[0].properties[1].tag == 0xfedcba98);
  CHECK (p.legacy.layers[0].properties[1].payload.size == 4);
  auto original = b;
  probe (bytes (b)); CHECK (original == b); // immutable input

  Limits l;
  l.max_records = 0;
  CHECK (probe (bytes (b), l).selection == Selection::inconclusive);
  l = Limits (); l.max_objects = 0;
  CHECK (probe (bytes (b), l).selection == Selection::inconclusive);
  l = Limits (); l.max_scanned_bytes = 25;
  CHECK (probe (bytes (b), l).selection == Selection::inconclusive);
  l = Limits (); l.max_string_bytes = 2;
  CHECK (probe (bytes (b), l).selection == Selection::inconclusive);

  // Oversized unknown property: bounded rejection, not allocation or wraparound.
  b = minimal (3, false); set32 (b, 26, 0xfeed); set32 (b, 30, 0xffffffff);
  CHECK (probe (bytes (b)).selection == Selection::inconclusive);
  l = Limits (); l.max_property_bytes = std::numeric_limits<std::size_t>::max ();
  CHECK (decode_candidate (bytes (b), Dialect::legacy_painter, l).diagnostic.status == Status::truncated);
  b = minimal (11, true); set32 (b, 38, 0xffffffff); set32 (b, 42, 0xffffffff);
  CHECK (decode_candidate (bytes (b), Dialect::standard).diagnostic.status == Status::malformed);

  // Colormap framing differs in v0: count bytes rather than count*3 bytes.
  for (unsigned version : { 0u, 3u })
    {
      b = minimal (version, false); b.resize (26); u32 (b, 1);
      const unsigned count = 2, color_bytes = version == 0 ? count : count * 3;
      u32 (b, 4 + color_bytes); u32 (b, count);
      b.insert (b.end (), color_bytes, 0x80); end (b); end (b);
      p = probe (bytes (b));
      CHECK (p.selection == Selection::shared_layout);
      CHECK (p.legacy.image_properties[0].payload.size == 4 + color_bytes);
      set32 (b, 34, 257);
      CHECK (decode_candidate (bytes (b), Dialect::legacy_painter).diagnostic.status == Status::malformed);
    }

  // The old USER_UNIT writer computes the wrong outer size (?: precedence).
  b = minimal (3, false); b.resize (26); u32 (b, 24); u32 (b, 6);
  u32 (b, 0x3f800000); u32 (b, 2);
  for (unsigned i = 0; i != 5; ++i) str (b, "u");
  end (b); end (b);
  p = probe (bytes (b));
  CHECK (p.selection == Selection::shared_layout);
  CHECK (p.legacy.image_properties[0].length_mismatch);
  CHECK (p.legacy.image_properties[0].payload.size == 38);

  // Synthetic wire quirks below. They are NOT genuine writer/reader fixtures.
  Buffer wire; str (wire, "test-proc");
  u32 (wire, 3); u32 (wire, 2); u32 (wire, 0x0000ffff); // declared2 / written4
  u32 (wire, 4); u32 (wire, 1); wire.push_back (0x80);
  u32 (wire, 5); u32 (wire, 4); u32 (wire, 0x7fc00123); // preserve NaN bits
  u32 (wire, 8); str (wire, "reader string");
  end (wire);
  auto e = decode_filter (bytes (wire), { 0, wire.size () });
  CHECK (e.diagnostic.status == Status::ok && e.arguments.size () == 5);
  CHECK (e.arguments[0].kind == ArgumentKind::int16_writer_32_bits);
  CHECK (e.arguments[0].payload.size == 4 && e.arguments[0].declared_size == 2);
  CHECK (e.arguments[0].length_mismatch && e.arguments[0].scalar_bits == 65535);
  CHECK (e.arguments[1].scalar_bits == 128 && e.arguments[2].scalar_bits == 0x7fc00123);
  CHECK (text (wire, e.arguments[3].string_value) == "reader string");
  for (std::size_t n = 0; n < wire.size (); ++n)
    CHECK (decode_filter (bytes (wire), { 0, n }).diagnostic.status != Status::ok);

  wire.clear (); str (wire, "p"); u32 (wire, 7); str (wire, "abc"); end (wire);
  const auto exact = decode_filter (bytes (wire), { 0, wire.size () });
  CHECK (exact.diagnostic.status == Status::truncated);
  CHECK (exact.arguments[0].kind == ArgumentKind::omitted_array);
  CHECK (exact.arguments[0].payload.size == 0);
  const auto recovery = decode_filter (bytes (wire), { 0, wire.size () }, FilterPolicy::writer_string_recovery);
  CHECK (recovery.diagnostic.status == Status::ok && recovery.used_writer_string_recovery);
  CHECK (recovery.arguments[0].kind == ArgumentKind::recovered_writer_string);
  CHECK (text (wire, recovery.arguments[0].string_value) == "abc");
  wire.clear (); str (wire, "p"); u32 (wire, 7); u32 (wire, 0); end (wire);
  e = decode_filter (bytes (wire), { 0, wire.size () });
  CHECK (e.diagnostic.status == Status::ok && e.has_ambiguous_null_string);

  wire.clear (); str (wire, "p");
  for (unsigned tag : { 1u, 6u, 9u, 10u, 99u }) { u32 (wire, tag); u32 (wire, 0); }
  end (wire);
  e = decode_filter (bytes (wire), { 0, wire.size () });
  CHECK (e.diagnostic.status == Status::ok && e.arguments.size () == 6);
  CHECK (e.arguments[4].kind == ArgumentKind::unknown);
  wire.push_back (17);
  e = decode_filter (bytes (wire), { 0, wire.size () });
  CHECK (e.diagnostic.status == Status::needs_recovery && e.tail.size == 1);
  CHECK (e.encoded.size == wire.size ());
  CHECK (decode_filter (bytes (wire), { 1, std::numeric_limits<std::size_t>::max () }).diagnostic.status == Status::truncated);
  l = Limits (); l.max_records = 1;
  CHECK (decode_filter (bytes (wire), { 0, wire.size () }, FilterPolicy::legacy_reader, l).diagnostic.status == Status::limit);

  // Missing NUL is preserved, not repaired as the old xcf_read_string does.
  wire.clear (); str (wire, "name"); wire.back () = 'x'; end (wire);
  e = decode_clone (bytes (wire), { 0, wire.size () });
  CHECK (e.diagnostic.status == Status::ok && ! e.name.has_terminal_nul);
  CHECK (text (wire, e.name) == "namex");

  // Deterministic mutation smoke test, bounded by deliberately small budgets.
  b = layer (4, false, props); l = Limits ();
  l.max_records = 128; l.max_objects = 16; l.max_string_bytes = 256;
  l.max_property_bytes = 256; l.max_scanned_bytes = 4096;
  for (unsigned seed = 0; seed != 2000; ++seed)
    {
      auto mutated = b;
      const auto at = (seed * 17u) % mutated.size ();
      mutated[at] ^= std::uint8_t ((seed / mutated.size ()) + 1);
      const auto q = probe (bytes (mutated), l);
      candidate_ranges (q.legacy, mutated.size ()); candidate_ranges (q.standard, mutated.size ());
    }
  std::cout << "synthetic: ambiguity, limits, truncation, offsets, unknown/duplicate records, wire quirks, mutations passed\n";
}

int main (int argc, char **argv)
{
  if (argc != 3) { std::cerr << "usage: test-painter-xcf-compat LEGACY_FIXTURE_DIR STANDARD_XCF\n"; return 2; }
  synthetic_cases ();
  genuine_fixtures (argv[1], argv[2]);
  std::cout << checks << " checks passed; metadata decoder only, app open/roundtrip NOT tested\n";
}
