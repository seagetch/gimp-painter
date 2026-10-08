/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FILTER_PARAMETER_EDITOR_HPP
#define GIMP_FILTER_PARAMETER_EDITOR_HPP
#include "../painter/gimp-painter-visibility.h"
#include "painter/filter-procedure-policy.hpp"
#include <glib-object.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

typedef struct _Gimp Gimp;
typedef struct _GimpProcedure GimpProcedure;

namespace GimpPainter GIMP_PAINTER_PRIVATE {
enum class FilterEditorValueType { integer, real, boolean, choice, double_array, int32_array, context };
/* Trusted legacy overlay. These values never come from a PDB registration and
 * never fill missing saved arguments. Initial values are for explicit creation
 * or Reset only; unknown and unmodified saved values retain their exact type. */
struct FilterEditorField {
  const char *key = nullptr;
  unsigned saved_slot = 0;
  FilterEditorValueType type = FilterEditorValueType::context;
  gint32 integer_min = G_MININT32, integer_max = G_MAXINT32, integer_initial = 0;
  double real_min = -G_MAXDOUBLE, real_max = G_MAXDOUBLE, real_initial = 0;
  unsigned count = 0, count_slot = 0;
  bool legacy_flag = false;
};
FilterEditorField filter_editor_field (FilterProcedure route, const char *key);
const FilterLegacy::RouteNames& filter_editor_route_names (FilterProcedure route);
bool filter_editor_procedure_matches (FilterProcedure route, GimpProcedure *procedure) noexcept;

struct FilterEditorChoice {
  gint value = 0;
  std::string nick, label, help;
};
/* Plain process-local metadata. No borrowed pspec/GValue/GObject is exposed.
 * Array lengths and saved slots are explicitly supplied by the legacy overlay,
 * not inferred from a boxed type or a public procedure's defaults. */
struct FilterEditorParameter {
  FilterEditorField legacy;
  unsigned runtime_slot = 0;
  std::string name, type_name, spec_type_name, nick, blurb;
  GType value_type = G_TYPE_INVALID, spec_type = G_TYPE_INVALID;
  GParamFlags flags = static_cast<GParamFlags> (0);
  gint integer_min = 0, integer_max = 0, integer_default = 0;
  double real_min = 0, real_max = 0, real_default = 0;
  bool default_is_null = false, boolean_default = false, nullable = false;
  unsigned array_element_bytes = 0, array_element_alignment = 0;
  std::string string_default, object_type_name;
  std::vector<FilterEditorChoice> choices;
};
enum class FilterEditorSchemaStatus { unavailable, incompatible };
class FilterEditorSchemaError : public std::runtime_error {
public:
  FilterEditorSchemaError (FilterEditorSchemaStatus status, const char *reason)
    : std::runtime_error (reason), status_ (status) {}
  FilterEditorSchemaStatus status () const noexcept { return status_; }
private:
  FilterEditorSchemaStatus status_;
};

/* Owner-thread only, synchronous and bounded. This reads already registered
 * metadata; it never queries a plug-in, dispatches a main loop, or waits for a
 * child. Retained providers prevent pointer reuse while a dialog is open. */
class FilterEditorSchema {
public:
  static constexpr std::size_t max_bytes = 64 * 1024, max_specs = 512;
  FilterEditorSchema (Gimp *gimp, FilterProcedure route);
  ~FilterEditorSchema ();
  FilterEditorSchema (const FilterEditorSchema&) = delete;
  FilterEditorSchema& operator= (const FilterEditorSchema&) = delete;
  const FilterEditorParameter& field (const char *key) const;
  bool current (Gimp *gimp) const noexcept;
  bool matches (GimpProcedure *procedure) const noexcept;
  std::size_t snapshot_bytes () const noexcept;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
#endif
