/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "resource.hpp"
#include "mypaintbrush-settings-data.h"
#include "painter/object-ref.hpp"
#include "painter/resources.hpp"
#include <json-glib/json-glib.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace GimpPainter {
template<> struct TypeTraits<JsonParser> { static GType type () noexcept { return JSON_TYPE_PARSER; } };
template<> struct TypeTraits<JsonGenerator> { static GType type () noexcept { return JSON_TYPE_GENERATOR; } };
namespace MyPaint {
namespace {
struct NodeFree { void operator() (JsonNode *n) const noexcept { if (n) json_node_unref (n); } };
using Node = std::unique_ptr<JsonNode, NodeFree>;
Node object_node ()
{
  auto *o = json_object_new (); Node n (json_node_init_object (json_node_alloc (), o));
  json_object_unref (o); return n;
}
JsonObject *object (JsonNode *n, const std::string& where)
{
  if (!n || !JSON_NODE_HOLDS_OBJECT (n)) throw std::invalid_argument (where + " must be an object");
  return json_node_get_object (n);
}
JsonNode *member (JsonObject *o, const char *key) { return json_object_get_member (o, key); }
JsonObject *section (JsonObject *o, const char *key, bool create = false)
{
  auto *n = member (o, key);
  if (!n && create) { auto node = object_node (); json_object_set_member (o, key, node.release ()); n = member (o, key); }
  return n ? object (n, key) : nullptr;
}
std::string json_string (JsonNode *n, const std::string& where)
{
  if (!n || !JSON_NODE_HOLDS_VALUE (n) || json_node_get_value_type (n) != G_TYPE_STRING)
    throw std::invalid_argument (where + " must be a string");
  return json_node_get_string (n);
}
double number (JsonNode *n, const std::string& where)
{
  if (!n || !JSON_NODE_HOLDS_VALUE (n) ||
      (json_node_get_value_type (n) != G_TYPE_INT64 && json_node_get_value_type (n) != G_TYPE_DOUBLE))
    throw std::invalid_argument (where + " must be a number");
  double result = json_node_get_double (n);
  if (!std::isfinite (result)) throw std::invalid_argument (where + " must be finite");
  return result;
}
void finite_value (double value)
{
  if (!std::isfinite (value) || std::abs (value) > std::numeric_limits<float>::max ())
    throw std::invalid_argument ("Brush value does not fit finite legacy float storage");
}
Node clone (JsonNode *n)
{
  if (JSON_NODE_HOLDS_OBJECT (n)) {
    auto out = object_node (); auto *src = json_node_get_object (n); auto *dst = json_node_get_object (out.get ());
    GList *members = json_object_get_members (src);
    for (auto *p = members; p; p = p->next) {
      const auto *key = static_cast<const char *> (p->data);
      json_object_set_member (dst, key, clone (member (src, key)).release ());
    }
    g_list_free (members); return out;
  }
  if (JSON_NODE_HOLDS_ARRAY (n)) {
    auto *src = json_node_get_array (n); auto *dst = json_array_new ();
    Node out (json_node_init_array (json_node_alloc (), dst)); json_array_unref (dst);
    for (guint i = 0; i < json_array_get_length (src); ++i)
      json_array_add_element (dst, clone (json_array_get_element (src, i)).release ());
    return out;
  }
  return Node (json_node_copy (n));
}
std::string encode_node (JsonNode *node)
{
  auto gen = ObjectRef<JsonGenerator>::adopt (json_generator_new ());
  json_generator_set_root (gen.get (), node); json_generator_set_pretty (gen.get (), TRUE);
  String data (json_generator_to_data (gen.get (), nullptr));
  return data.get ();
}
Node parse_json (const std::string& text)
{
  auto parser = ObjectRef<JsonParser>::adopt (json_parser_new ()); GError *error = nullptr;
  if (!json_parser_load_from_data (parser.get (), text.data (), text.size (), &error)) {
    std::string message = GimpPainter::take_error_message (error, "Unknown error"); throw std::invalid_argument (message);
  }
  return clone (json_parser_get_root (parser.get ()));
}
const char *setting_name (int id)
{
  if (id < 0 || id >= BRUSH_MAPPING_COUNT) throw std::out_of_range ("Brush setting index");
  return painter_mypaint_settings[id].internal_name;
}
const char *input_name (int id)
{
  if (id < 0 || id >= INPUT_COUNT) throw std::out_of_range ("Brush input index");
  return painter_mypaint_inputs[id].name;
}
const char *switch_name (int id)
{
  if (id < BRUSH_BOOL_BASE || id >= BRUSH_BOOL_END) throw std::out_of_range ("Brush switch index");
  return painter_mypaint_switches[id-BRUSH_BOOL_BASE].internal_name;
}
const char *text_name (int id)
{
  if (id < BRUSH_TEXT_BASE || id >= BRUSH_TEXT_END) throw std::out_of_range ("Brush text index");
  return painter_mypaint_texts[id-BRUSH_TEXT_BASE].internal_name;
}
int setting_index (const std::string& name)
{
  for (const auto& v : painter_mypaint_settings) if (name == v.internal_name) return v.index;
  return -1;
}
int input_index (const std::string& name)
{
  for (const auto& v : painter_mypaint_inputs) if (name == v.name) return v.index;
  return -1;
}
std::string strip (std::string value)
{
  auto first = value.find_first_not_of (" \t\r\n");
  if (first == std::string::npos) return {};
  return value.substr (first, value.find_last_not_of (" \t\r\n")-first+1);
}
std::vector<std::string> split (const std::string& text, char delim)
{
  std::vector<std::string> result; std::istringstream input (text); std::string item;
  while (std::getline (input, item, delim)) result.push_back (strip (item));
  return result;
}
double ascii_number (const std::string& value)
{
  char *end = nullptr; double n = g_ascii_strtod (value.c_str (), &end);
  if (end == value.c_str () || !strip (end).empty ()) throw std::invalid_argument ("Invalid number: " + value);
  finite_value (n); return n;
}
float migrate (const std::string& key, double value)
{
  const float f = value;
  if (key == "color_hue") return f*64.0/360.0;
  if (key == "color_saturation" || key == "color_value") return f*128.0/256.0;
  return f;
}
std::string migrated_name (const std::string& key)
{
  if (key == "color_hue") return "change_color_h";
  if (key == "color_saturation" || key == "change_color_s") return "change_color_hsv_s";
  if (key == "color_value") return "change_color_v";
  if (key == "speed_slowness") return "speed1_slowness";
  if (key == "stroke_treshold") return "stroke_threshold";
  return key;
}
std::vector<Point> parse_legacy_curve (const std::string& text, int version, const std::string& key)
{
  std::vector<Point> result;
  const char *p = text.c_str ();
  while (*p) {
    while (g_ascii_isspace (*p) || (version > 1 && *p == ',')) ++p;
    if (!*p) break;
    if (version > 1 && *p++ != '(') throw std::invalid_argument ("Expected '(' in legacy curve");
    while (g_ascii_isspace (*p)) ++p;
    char *end = nullptr; double x = g_ascii_strtod (p, &end);
    if (end == p) throw std::invalid_argument ("Missing curve x");
    p = end;
    while (g_ascii_isspace (*p)) ++p;
    double y = g_ascii_strtod (p, &end);
    if (end == p) throw std::invalid_argument ("Missing curve y");
    p = end;
    while (g_ascii_isspace (*p)) ++p;
    if (version > 1 && *p++ != ')') throw std::invalid_argument ("Expected ')' in legacy curve");
    finite_value (x); finite_value (y); result.push_back ({x, migrate (key, y)});
  }
  if (result.size () == 1 || result.size () > 8) throw std::invalid_argument ("Invalid legacy curve point count");
  for (std::size_t i = 1; i < result.size (); ++i)
    if (result[i].x < result[i-1].x) throw std::invalid_argument ("Descending legacy curve");
  return result;
}
}
struct Resource::Impl {
  Node root = object_node ();
  int source_version = 3;
  Impl () { json_object_set_int_member (json_node_get_object (root.get ()), "version", 3); }
  JsonObject *object () const { return json_node_get_object (root.get ()); }
};
Resource::Resource () : impl_ (new Impl) {}
Resource::~Resource () = default;
Resource::Resource (const Resource& other) : impl_ (new Impl)
{ impl_->root = clone (other.impl_->root.get ()); impl_->source_version = other.impl_->source_version; }
Resource& Resource::operator= (const Resource& other)
{ Resource copy (other); impl_.swap (copy.impl_); return *this; }
Resource::Resource (Resource&&) noexcept = default;
Resource& Resource::operator= (Resource&&) noexcept = default;
int Resource::source_version () const noexcept { return impl_->source_version; }
Resource Resource::decode (const std::string& text, const std::string& source_basename)
{
  if (text.size () > 16*1024*1024) throw std::invalid_argument ("Brush exceeds 16 MiB safety limit");
  if (text.find ('\0') != std::string::npos || !g_utf8_validate (text.data (), text.size (), nullptr))
    throw std::invalid_argument ("Brush is not valid UTF-8 text");
  Resource r; const std::string trimmed = strip (text);
  if (trimmed.empty ()) throw std::invalid_argument ("Empty brush file");
  if (trimmed[0] == '{' || trimmed[0] == '[') {
    r.impl_->root = parse_json (text); auto *root = object (r.impl_->root.get (), "Brush root");
    const double version = number (member (root, "version"), "version");
    if (version != std::floor (version) || version < 3 || version > G_MAXINT)
      throw std::invalid_argument ("Expected JSON brush version >= 3");
    r.impl_->source_version = version;
    auto *parent = member (root, "parent_brush_name");
    if (parent && json_string (parent, "parent_brush_name").empty () && !source_basename.empty ())
      r.set_parent_brush_name (source_basename);
    // Known values are validated, never clamped. Unknown values remain opaque.
    for (int i = 0; i < BRUSH_MAPPING_COUNT; ++i) {
      finite_value (r.base_value (i));
      for (int j = 0; j < INPUT_COUNT; ++j) (void) r.curve (i, j);
    }
    for (int i = BRUSH_BOOL_BASE; i < BRUSH_BOOL_END; ++i) (void) r.switch_value (i);
    for (int i = BRUSH_TEXT_BASE; i < BRUSH_TEXT_END; ++i) (void) r.text_value (i);
    (void) r.parent_brush_name (); (void) r.group ();
    return r;
  }
  // Legacy line files: preserve the complete original and all raw keys. The
  // canonical v3 object carries migrated values and survives further editing.
  std::vector<std::pair<std::string, std::string>> fields;
  int version = 1; bool seen_version = false;
  std::istringstream lines (text); std::string line;
  while (std::getline (lines, line)) {
    line = strip (line); if (line.empty () || line[0] == '#') continue;
    auto pos = line.find_first_of (" \t");
    if (pos == std::string::npos) throw std::invalid_argument ("Missing legacy brush value");
    auto key = line.substr (0, pos); auto value = strip (line.substr (pos+1));
    if (key == "version") {
      double v = ascii_number (value);
      if (v < 1 || v > 2 || v != std::floor (v) || seen_version)
        throw std::invalid_argument ("Unsupported or duplicate legacy version");
      version = v; seen_version = true;
    } else fields.emplace_back (key, value);
  }
  r.impl_->source_version = version;
  auto *root = r.impl_->object (); auto *legacy = section (root, "painter_legacy", true);
  json_object_set_int_member (legacy, "version", version);
  json_object_set_string_member (legacy, "source", text.c_str ());
  auto *raw = section (legacy, "fields", true);
  for (const auto& f : fields) {
    const auto& key = f.first; const auto& value = f.second;
    json_object_set_string_member (raw, key.c_str (), value.c_str ());
    if (key == "parent_brush_name") {
      String decoded (g_uri_unescape_string (value.c_str (), nullptr));
      if (!decoded) throw std::invalid_argument ("Invalid percent-escaped brush name");
      r.set_parent_brush_name (decoded.get ()); continue;
    }
    if (key == "group") { r.set_group (value); continue; } // The old reader kept raw group text.
    if (version <= 1 && key == "painting_time") continue;
    if (version <= 1 && key == "color") {
      std::istringstream rgb (value); double red, green, blue;
      if (!(rgb >> red >> green >> blue)) throw std::invalid_argument ("Invalid legacy RGB color");
      red /= 255; green /= 255; blue /= 255;
      double max = std::max ({red, green, blue}), min = std::min ({red, green, blue}), delta = max-min;
      double h = 0, s = 0;
      if (delta > 0.0001) {
        s = delta/max;
        if (red == max) { h = (green-blue)/delta; if (h < 0) h += 6; }
        else if (green == max) h = 2+(blue-red)/delta;
        else h = 4+(red-green)/delta;
        h /= 6;
      }
      r.set_base_value (BRUSH_COLOR_H, h); r.set_base_value (BRUSH_COLOR_S, s); r.set_base_value (BRUSH_COLOR_V, max); continue;
    }
    const std::string name = migrated_name (key);
    int id = setting_index (name);
    if (id < 0) continue; // Raw record retained and diagnostics report unimplemented keys.
    auto parts = split (value, '|');
    if (parts.empty ()) throw std::invalid_argument ("Missing legacy base value");
    r.set_base_value (id, migrate (key, ascii_number (parts[0])));
    for (std::size_t i = 1; i < parts.size (); ++i) {
      auto sep = parts[i].find_first_of (" \t");
      if (sep == std::string::npos) throw std::invalid_argument ("Missing legacy curve data");
      std::string input = parts[i].substr (0, sep);
      int idx = input_index (input);
      if (idx < 0) {
        // Keep unsupported legacy inputs in the canonical settings tree too.
        // Merely retaining raw source is insufficient: diagnostics and Engine
        // must see them rather than executing a silently altered brush.
        auto *setting = section (section (r.impl_->object (), "settings", true), setting_name (id), true);
        json_object_set_string_member (section (setting, "inputs", true), input.c_str (),
                                       strip (parts[i].substr (sep+1)).c_str ());
        continue;
      }
      r.set_curve (id, idx, parse_legacy_curve (parts[i].substr (sep+1), version, key));
    }
  }
  return r;
}
Resource Resource::load (GInputStream *input, GCancellable *cancel, const std::string& source_basename)
{
  if (!G_IS_INPUT_STREAM (input)) throw std::invalid_argument ("Expected GInputStream");
  std::string text; char buffer[8192]; GError *error = nullptr;
  for (;;) {
    const auto count = g_input_stream_read (input, buffer, sizeof buffer, cancel, &error);
    if (count < 0) { std::string why = GimpPainter::take_error_message (error, "Unknown error"); throw std::runtime_error (why); }
    if (!count) break;
    if (text.size () + count > 16*1024*1024) throw std::invalid_argument ("Brush exceeds 16 MiB safety limit");
    text.append (buffer, count);
  }
  return decode (text, source_basename);
}
std::string Resource::encode () const { return encode_node (impl_->root.get ()); }
bool Resource::save (GOutputStream *output, GCancellable *cancel, GError **error) const noexcept
{
  return boundary<bool> (error, false, [&] {
    if (!G_IS_OUTPUT_STREAM (output)) throw std::invalid_argument ("Expected GOutputStream");
    const auto text = encode ();
    return bool (g_output_stream_write_all (output, text.data (), text.size (), nullptr, cancel, error));
  });
}
bool Resource::has_member (const std::string& sec, const std::string& name) const
{
  auto *o = section (impl_->object (), sec.c_str ());
  return o && json_object_has_member (o, name.c_str ());
}
double Resource::base_value (int id) const
{
  const char *key = setting_name (id); auto *s = section (impl_->object (), "settings");
  auto *v = s ? section (s, key) : nullptr;
  if (v) return number (member (v, "base_value"), key);
  return painter_mypaint_settings[id].default_value;
}
bool Resource::switch_value (int id) const
{
  const char *key = switch_name (id); auto *s = section (impl_->object (), "switches");
  auto *n = s ? member (s, key) : nullptr;
  // Historical Resource constructor initialized all switches FALSE. It did not
  // copy metadata defaults (notably use_gimp_brushmark's TRUE default).
  if (!n) return false;
  if (!JSON_NODE_HOLDS_VALUE (n) || json_node_get_value_type (n) != G_TYPE_BOOLEAN)
    throw std::invalid_argument (std::string (key) + " must be boolean");
  return json_node_get_boolean (n);
}
bool Resource::text_is_null (int id) const
{
  auto *s = section (impl_->object (), "texts"); auto *n = s ? member (s, text_name (id)) : nullptr;
  return !n || JSON_NODE_HOLDS_NULL (n);
}
const char *Resource::peek_text (int id) const
{
  const char *key = text_name (id); auto *s = section (impl_->object (), "texts"); auto *n = s ? member (s, key) : nullptr;
  if (!n || JSON_NODE_HOLDS_NULL (n)) return nullptr;
  if (!JSON_NODE_HOLDS_VALUE (n) || json_node_get_value_type (n) != G_TYPE_STRING)
    throw std::invalid_argument (std::string (key) + " must be a string");
  return json_node_get_string (n);
}
std::string Resource::text_value (int id) const
{ const char *text = peek_text (id); return text ? text : ""; }
std::string Resource::parent_brush_name () const
{ auto *n = member (impl_->object (), "parent_brush_name"); return n ? json_string (n, "parent_brush_name") : ""; }
std::string Resource::group () const
{ auto *n = member (impl_->object (), "group"); return n ? json_string (n, "group") : ""; }
std::string Resource::preview_png_base64 () const
{ auto *n = member (impl_->object (), "painter_preview_png"); return n ? json_string (n, "painter_preview_png") : ""; }
void Resource::set_preview_png_base64 (const std::string& value)
{
  if (value.empty ()) json_object_remove_member (impl_->object (), "painter_preview_png");
  else json_object_set_string_member (impl_->object (), "painter_preview_png", value.c_str ());
}
std::vector<Point> Resource::curve (int id, int input) const
{
  const auto *key = setting_name (id); const auto *in = input_name (input);
  auto *s = section (impl_->object (), "settings"); auto *v = s ? section (s, key) : nullptr;
  auto *inputs = v ? section (v, "inputs") : nullptr; auto *n = inputs ? member (inputs, in) : nullptr;
  if (!n) {
    if (id == BRUSH_OPAQUE_MULTIPLY && input == INPUT_PRESSURE) return {{0,0},{1,1}};
    return {};
  }
  if (!JSON_NODE_HOLDS_ARRAY (n)) throw std::invalid_argument ("Curve must be an array");
  auto *a = json_node_get_array (n); std::vector<Point> points;
  for (guint i = 0; i < json_array_get_length (a); ++i) {
    auto *p = json_array_get_element (a, i);
    if (!JSON_NODE_HOLDS_ARRAY (p) || json_array_get_length (json_node_get_array (p)) != 2)
      throw std::invalid_argument ("Curve point must contain exactly x and y");
    auto *pair = json_node_get_array (p);
    double x = number (json_array_get_element (pair, 0), "curve.x"), y = number (json_array_get_element (pair, 1), "curve.y");
    finite_value (x); finite_value (y); points.push_back ({x,y});
  }
  // Arbitrary point counts and order stay in the file, but mapping() rejects
  // those unsupported by the legacy engine. No clipping or silent shortening.
  return points;
}
Mapping Resource::mapping (int id) const
{
  Mapping m (INPUT_COUNT); m.base_value = base_value (id);
  for (int j = 0; j < INPUT_COUNT; ++j) {
    const auto points = curve (id, j); m.set_n (j, points.size ());
    for (std::size_t i = 0; i < points.size (); ++i) m.set_point (j, i, points[i].x, points[i].y);
  }
  return m;
}
void Resource::set_base_value (int id, double value)
{
  finite_value (value); auto *s = section (impl_->object (), "settings", true); auto *v = section (s, setting_name (id), true);
  json_object_set_double_member (v, "base_value", value);
}
void Resource::set_switch (int id, bool value)
{ json_object_set_boolean_member (section (impl_->object (), "switches", true), switch_name (id), value); }
void Resource::set_text (int id, const char *value)
{
  auto *s = section (impl_->object (), "texts", true); const auto *key = text_name (id);
  if (value) json_object_set_string_member (s, key, value); else json_object_set_null_member (s, key);
}
void Resource::set_curve (int id, int input, const std::vector<Point>& points)
{
  for (const auto& p : points) { finite_value (p.x); finite_value (p.y); }
  const double base = base_value (id); // initialize a newly-created setting with its real default
  auto *s = section (impl_->object (), "settings", true); auto *v = section (s, setting_name (id), true);
  if (!member (v, "base_value")) json_object_set_double_member (v, "base_value", base);
  auto *ins = section (v, "inputs", true); auto *array = json_array_new ();
  for (const auto& p : points) {
    auto *pair = json_array_new (); json_array_add_double_element (pair, p.x); json_array_add_double_element (pair, p.y);
    json_array_add_array_element (array, pair);
  }
  json_object_set_array_member (ins, input_name (input), array);
}
void Resource::set_parent_brush_name (const std::string& value)
{ json_object_set_string_member (impl_->object (), "parent_brush_name", value.c_str ()); }
void Resource::set_group (const std::string& value)
{ json_object_set_string_member (impl_->object (), "group", value.c_str ()); }
std::vector<Diagnostic> Resource::diagnostics () const
{
  std::vector<Diagnostic> out;
  if (source_version () > 3) out.push_back ({Diagnostic::Kind::Unsupported, "version", "Future brush version retained; engine semantics are not known"});
  auto scan = [&] (JsonObject *dict, const std::string& path, const std::vector<std::string>& names) {
    if (!dict) return;
    GList *members = json_object_get_members (dict);
    for (auto *p = members; p; p = p->next) {
      const std::string key = static_cast<const char *> (p->data);
      if (std::find (names.begin (), names.end (), key) != names.end ()) continue;
      const bool ignored = path == "settings" && (key == "colorize" || key == "snap_to_pixel" || key == "pressure_gain_log");
      out.push_back ({ignored ? Diagnostic::Kind::LegacyIgnored : Diagnostic::Kind::Unsupported, path+"."+key,
        ignored ? "Retained: pinned legacy engine did not implement this setting" : "Retained: unsupported engine field"});
    }
    g_list_free (members);
  };
  std::vector<std::string> settings, inputs, switches, texts;
  for (auto& s : painter_mypaint_settings) settings.emplace_back (s.internal_name);
  for (auto& s : painter_mypaint_inputs) inputs.emplace_back (s.name);
  for (auto& s : painter_mypaint_switches) switches.emplace_back (s.internal_name);
  for (auto& s : painter_mypaint_texts) texts.emplace_back (s.internal_name);
  auto *s = section (impl_->object (), "settings"); scan (s, "settings", settings);
  scan (section (impl_->object (), "switches"), "switches", switches);
  scan (section (impl_->object (), "texts"), "texts", texts);
  for (int id = 0; id < BRUSH_MAPPING_COUNT; ++id) {
    auto *v = s ? section (s, setting_name (id)) : nullptr;
    scan (v ? section (v, "inputs") : nullptr, std::string ("settings.")+setting_name (id)+".inputs", inputs);
    try { (void) mapping (id); }
    catch (const std::exception& ex) { out.push_back ({Diagnostic::Kind::Unsupported, std::string ("settings.")+setting_name (id), ex.what ()}); }
  }
  if (auto *legacy = section (impl_->object (), "painter_legacy")) {
    auto names = settings;
    for (const auto& n : {"parent_brush_name", "group", "painting_time", "color", "color_hue", "color_saturation", "color_value", "speed_slowness", "change_color_s", "stroke_treshold"}) names.emplace_back (n);
    // Legacy ignored fields are classified consistently with the JSON corpus.
    scan (section (legacy, "fields"), "settings", names);
  }
  return out;
}
} }
