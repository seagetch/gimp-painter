/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYPAINT_RESOURCE_HPP
#define GIMP_PAINTER_MYPAINT_RESOURCE_HPP
#include "../../painter/gimp-painter-visibility.h"
#include <gio/gio.h>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "mapping.hpp"
#include "mypaintbrush-enum-settings.h"

namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint {
struct Point { double x, y; };
struct Diagnostic {
  enum class Kind { LegacyIgnored, Unsupported };
  Kind kind;
  std::string path;
  std::string message;
};
/* Lossless JSON is authoritative; typed accessors resolve old-reader defaults.
 * Copies own independent trees. Unknown members at every depth survive edits. */
class Resource
{
public:
  Resource ();
  ~Resource ();
  Resource (const Resource&);
  Resource& operator= (const Resource&);
  Resource (Resource&&) noexcept;
  Resource& operator= (Resource&&) noexcept;
  static Resource decode (const std::string& text, const std::string& source_basename = {});
  static Resource load (GInputStream *input, GCancellable *cancel = nullptr, const std::string& source_basename = {});
  std::string encode () const;
  bool save (GOutputStream *output, GCancellable *cancel, GError **error) const noexcept;
  int source_version () const noexcept;
  bool has_member (const std::string& section, const std::string& name) const;
  double base_value (int id) const;
  bool switch_value (int id) const;
  bool text_is_null (int id) const;
  /* Borrowed until this Resource changes or is destroyed. NULL represents a
   * missing/null text. Copy before callbacks or releasing the owning lease. */
  const char *peek_text (int id) const;
  std::string text_value (int id) const;
  std::string parent_brush_name () const;
  std::string group () const;
  std::string preview_png_base64 () const;
  void set_preview_png_base64 (const std::string& value);
  std::vector<Point> curve (int id, int input) const;
  Mapping mapping (int id) const;
  void set_base_value (int id, double value);
  void set_switch (int id, bool value);
  void set_text (int id, const char *value);
  void set_curve (int id, int input, const std::vector<Point>& points);
  void set_parent_brush_name (const std::string& value);
  void set_group (const std::string& value);
  std::vector<Diagnostic> diagnostics () const;
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} }
#endif
