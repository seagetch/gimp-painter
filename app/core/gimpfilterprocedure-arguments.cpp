/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core-types.h"
#include "gimpimage.h"
#include "gimpdrawable.h"
#include "gimpparamspecs.h"
#include "pdb/gimpprocedure.h"
}
#include "gimpfilterprocedure-arguments.hpp"
#include "painter/resources.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace GimpPainter {
namespace {
/* Runtime metadata fingerprints for this bundled GIMP 3 provider. These are
 * independently pinned public/hidden declarations, not legacy admission limits
 * or defaults for missing saved arguments. See the registration code in
 * plug-ins/common/{blinds,tile-small,contrast-retinex,convolution-matrix}.c. */
namespace RuntimeMetadata {
struct Integer { gint minimum, maximum, initial; };
struct Real { gdouble minimum, maximum, initial; };
constexpr Integer blinds_angle {0, 90, 30}, blinds_segments {1, 1024, 3};
constexpr Integer tiles_public {2, 6, 2}, tiles_hidden {0, 6, 2};
constexpr Integer retinex_scale_public {16, 250, 240}, retinex_scale_hidden {16, 256, 240};
constexpr Integer retinex_nscales {0, 8, 3};
constexpr Real retinex_cvar {0, 4, 1.2};
constexpr Integer convolution_alpha {G_MININT, G_MAXINT, 1}, convolution_border {0, 2, 2};
constexpr Real convolution_divisor {-G_MAXDOUBLE, G_MAXDOUBLE, 1}, convolution_offset {-G_MAXDOUBLE, G_MAXDOUBLE, 0};
}
/* Bounded copies cover the largest explicitly supported semantic array. The
 * GParamSpec array subtype itself does not describe its required count. */
constexpr gsize max_elements = FilterLegacy::matrix_count;
constexpr gsize max_string = 32;
[[noreturn]] void invalid ()
{ throw std::invalid_argument ("Bundled Filter parameter value is incompatible"); }
[[noreturn]] void drift ()
{ throw std::runtime_error ("Bundled Filter argument metadata changed"); }

gsize object_count (GObject *const *objects)
{
  if (!objects) return 0;
  for (gsize n = 0; n <= max_elements; ++n)
    if (!objects[n]) return n;
  invalid ();
}
void bounded (const GValue& value)
{
  const auto type = G_VALUE_TYPE (&value);
  if (type == G_TYPE_INT || type == G_TYPE_BOOLEAN || type == GIMP_TYPE_RUN_MODE ||
      type == GIMP_TYPE_IMAGE) return;
  if (type == G_TYPE_DOUBLE)
    { if (!std::isfinite (g_value_get_double (&value))) invalid (); return; }
  if (type == G_TYPE_STRING)
    {
      const char *text = g_value_get_string (&value);
      if (!text) return;
      for (gsize n = 0; n <= max_string; ++n) if (!text[n]) return;
      invalid ();
    }
  if (type == GIMP_TYPE_CORE_OBJECT_ARRAY)
    { object_count (static_cast<GObject **> (g_value_get_boxed (&value))); return; }
  if (type == GIMP_TYPE_DOUBLE_ARRAY || type == GIMP_TYPE_INT32_ARRAY)
    {
      const auto *array = static_cast<const GimpArray *> (g_value_get_boxed (&value));
      if (!array) return;
      const gsize width = type == GIMP_TYPE_DOUBLE_ARRAY ? sizeof (gdouble) : sizeof (gint32);
      if (array->length > max_elements * width || array->length % width ||
          ((array->data == nullptr) != (array->length == 0))) invalid ();
      if (type == GIMP_TYPE_DOUBLE_ARRAY)
        for (gsize at = 0; at < array->length; at += width)
          {
            gdouble number; std::memcpy (&number, array->data + at, width);
            if (!std::isfinite (number)) invalid ();
          }
      return;
    }
  invalid ();
}
void integer (GParamSpec *spec, RuntimeMetadata::Integer expected)
{
  if (!G_IS_PARAM_SPEC_INT (spec) || G_PARAM_SPEC_INT (spec)->minimum != expected.minimum ||
      G_PARAM_SPEC_INT (spec)->maximum != expected.maximum || G_PARAM_SPEC_INT (spec)->default_value != expected.initial) drift ();
}
void real (GParamSpec *spec, RuntimeMetadata::Real expected)
{
  if (!G_IS_PARAM_SPEC_DOUBLE (spec) || G_PARAM_SPEC_DOUBLE (spec)->minimum != expected.minimum ||
      G_PARAM_SPEC_DOUBLE (spec)->maximum != expected.maximum || G_PARAM_SPEC_DOUBLE (spec)->default_value != expected.initial) drift ();
}
void choice (GParamSpec *spec, const char *const *names, const gint *ids, unsigned count, const char *initial)
{
  if (!GIMP_IS_PARAM_SPEC_CHOICE (spec)) drift ();
  auto *choices = gimp_param_spec_choice_get_choice (spec);
  if (!choices || g_list_length (gimp_choice_list_nicks (choices)) != count ||
      g_strcmp0 (gimp_param_spec_choice_get_default (spec), initial)) drift ();
  for (unsigned i = 0; i < count; ++i)
    if (!gimp_choice_is_valid (choices, names[i]) || gimp_choice_get_id (choices, names[i]) != ids[i]) drift ();
}
}

FilterParameterSchema::FilterParameterSchema (GimpProcedure *procedure, FilterProcedure route, bool hidden)
  : procedure_ (procedure), size_ (0)
{
  struct Expected { const char *name; GType type; };
  const Expected context[] = {{"run-mode", GIMP_TYPE_RUN_MODE}, {"image", GIMP_TYPE_IMAGE},
                              {"drawables", GIMP_TYPE_CORE_OBJECT_ARRAY}};
  const Expected blinds[] = {{"angle-displacement", G_TYPE_INT}, {"num-segments", G_TYPE_INT},
                            {"orientation", G_TYPE_STRING}, {"bg-transparent", G_TYPE_BOOLEAN}};
  const Expected tiles[] = {{"num-tiles", G_TYPE_INT}};
  const Expected retinex[] = {{"scale", G_TYPE_INT}, {"nscales", G_TYPE_INT},
                             {"scales-mode", G_TYPE_STRING}, {"cvar", G_TYPE_DOUBLE}};
  const Expected convolution[] = {{"matrix", GIMP_TYPE_DOUBLE_ARRAY}, {"alpha-alg", G_TYPE_INT},
    {"divisor", G_TYPE_DOUBLE}, {"offset", G_TYPE_DOUBLE}, {"channels", GIMP_TYPE_INT32_ARRAY}, {"border-mode", G_TYPE_INT}};
  const Expected *inputs = nullptr;
  switch (route) {
    case FilterProcedure::blinds: inputs = blinds; size_ = 7; break;
    case FilterProcedure::small_tiles: inputs = tiles; size_ = 4; break;
    case FilterProcedure::retinex: inputs = retinex; size_ = 7; break;
    case FilterProcedure::convolution: inputs = convolution; size_ = 9; break;
    default: drift ();
  }
  if (!procedure || !procedure->args || procedure->num_args != static_cast<gint> (size_)) drift ();
  /* Exact cardinality + unique known names rejects missing, extra and duplicate
   * parameters. Context position remains fixed even when inputs reorder. */
  std::array<bool, 9> seen {};
  for (unsigned i = 0; i < size_; ++i)
    {
      auto *parameter = procedure->args[i];
      if (!parameter || (parameter->flags & G_PARAM_READWRITE) != G_PARAM_READWRITE) drift ();
      unsigned key = 0;
      for (; key < size_; ++key)
        if (!g_strcmp0 (g_param_spec_get_name (parameter), (key < 3 ? context[key] : inputs[key-3]).name)) break;
      if (key == size_ || seen[key] || (key < 3 && key != i) ||
          G_PARAM_SPEC_VALUE_TYPE (parameter) != (key < 3 ? context[key] : inputs[key-3]).type) drift ();
      seen[key] = true;
    }
  auto *mode = spec ("run-mode"), *image = spec ("image"), *drawables = spec ("drawables");
  if (!G_IS_PARAM_SPEC_ENUM (mode) || G_PARAM_SPEC_ENUM (mode)->default_value != GIMP_RUN_NONINTERACTIVE ||
      !GIMP_IS_PARAM_SPEC_IMAGE (image) || gimp_param_spec_image_none_allowed (image) ||
      !GIMP_IS_PARAM_SPEC_CORE_OBJECT_ARRAY (drawables) ||
      gimp_param_spec_core_object_array_get_object_type (drawables) != GIMP_TYPE_DRAWABLE) drift ();
  switch (route) {
    case FilterProcedure::blinds: {
      integer (spec ("angle-displacement"), RuntimeMetadata::blinds_angle);
      integer (spec ("num-segments"), RuntimeMetadata::blinds_segments);
      auto *transparent = spec ("bg-transparent");
      if (!G_IS_PARAM_SPEC_BOOLEAN (transparent) || G_PARAM_SPEC_BOOLEAN (transparent)->default_value) drift ();
      const char *names[] = {"horizontal", "vertical"};
      const gint ids[] = {GIMP_ORIENTATION_HORIZONTAL, GIMP_ORIENTATION_VERTICAL};
      choice (spec ("orientation"), names, ids, 2, "horizontal"); break;
    }
    case FilterProcedure::small_tiles:
      integer (spec ("num-tiles"), hidden ? RuntimeMetadata::tiles_hidden : RuntimeMetadata::tiles_public); break;
    case FilterProcedure::retinex: {
      integer (spec ("scale"), hidden ? RuntimeMetadata::retinex_scale_hidden : RuntimeMetadata::retinex_scale_public);
      integer (spec ("nscales"), RuntimeMetadata::retinex_nscales);
      real (spec ("cvar"), RuntimeMetadata::retinex_cvar);
      const char *names[] = {"uniform", "low", "high"}; const gint ids[] = {0, 1, 2};
      choice (spec ("scales-mode"), names, ids, 3, "uniform"); break;
    }
    case FilterProcedure::convolution:
      if (!GIMP_IS_PARAM_SPEC_DOUBLE_ARRAY (spec ("matrix")) ||
          !GIMP_IS_PARAM_SPEC_INT32_ARRAY (spec ("channels"))) drift ();
      integer (spec ("alpha-alg"), RuntimeMetadata::convolution_alpha);
      integer (spec ("border-mode"), RuntimeMetadata::convolution_border);
      real (spec ("divisor"), RuntimeMetadata::convolution_divisor);
      real (spec ("offset"), RuntimeMetadata::convolution_offset); break;
  }
}
unsigned FilterParameterSchema::slot (const char *name) const
{
  if (name)
    for (unsigned i = 0; i < size_; ++i)
      if (!g_strcmp0 (g_param_spec_get_name (procedure_->args[i]), name)) return i;
  throw std::invalid_argument ("Unknown bundled Filter parameter name");
}
GParamSpec *FilterParameterSchema::spec (const char *name) const
{ return procedure_->args[slot (name)]; }

bool filter_parameter_values_equal (const GValue& first, const GValue& second)
{
  if (G_VALUE_TYPE (&first) != G_VALUE_TYPE (&second)) return false;
  bounded (first); bounded (second);
  const auto type = G_VALUE_TYPE (&first);
  if (type == G_TYPE_INT) return g_value_get_int (&first) == g_value_get_int (&second);
  if (type == G_TYPE_BOOLEAN) return g_value_get_boolean (&first) == g_value_get_boolean (&second);
  if (type == GIMP_TYPE_RUN_MODE) return g_value_get_enum (&first) == g_value_get_enum (&second);
  if (type == GIMP_TYPE_IMAGE) return g_value_get_object (&first) == g_value_get_object (&second);
  if (type == G_TYPE_DOUBLE) {
    const auto a = g_value_get_double (&first), b = g_value_get_double (&second);
    return !std::memcmp (&a, &b, sizeof a);
  }
  if (type == G_TYPE_STRING) return !g_strcmp0 (g_value_get_string (&first), g_value_get_string (&second));
  const auto *a = g_value_get_boxed (&first), *b = g_value_get_boxed (&second);
  if (!a || !b) return a == b;
  if (type == GIMP_TYPE_CORE_OBJECT_ARRAY) {
    auto aa = static_cast<GObject *const *> (a), bb = static_cast<GObject *const *> (b);
    const auto count = object_count (aa);
    if (count != object_count (bb)) return false;
    for (gsize i = 0; i < count; ++i) if (aa[i] != bb[i]) return false;
    return true;
  }
  const auto *aa = static_cast<const GimpArray *> (a), *bb = static_cast<const GimpArray *> (b);
  return aa->length == bb->length && (!aa->length || !std::memcmp (aa->data, bb->data, aa->length));
}
void filter_parameter_validate_copy (GParamSpec *spec, const GValue& value)
{
  if (!spec || G_VALUE_TYPE (&value) != G_PARAM_SPEC_VALUE_TYPE (spec)) invalid ();
  bounded (value);
  Value copy (G_VALUE_TYPE (&value)); g_value_copy (&value, copy.get ());
  /* The CoreObjectArray class callback can return FALSE after changing data;
   * GLib's public wrapper also checks the outer GValue. Exact contents still
   * need comparison for in-place boxed changes with an unchanged outer value. */
  const bool changed = g_param_value_validate (spec, copy.get ());
  if (changed || !filter_parameter_values_equal (value, *copy.get ())) invalid ();
}
void FilterParameterBinder::ValuesDelete::operator() (GimpValueArray *value) const
{ if (value) gimp_value_array_unref (value); }
FilterParameterBinder::FilterParameterBinder (const FilterParameterSchema& schema)
  : schema_ (schema), values_ (gimp_procedure_get_arguments (schema.procedure ()))
{
  if (!values_ || gimp_value_array_length (values_.get ()) != static_cast<gint> (schema_.size ())) drift ();
}
void FilterParameterBinder::assign (const char *name, const GValue& value)
{
  const auto slot = schema_.slot (name);
  if (assigned_[slot]) throw std::invalid_argument ("Repeated bundled Filter parameter assignment");
  auto *spec = schema_.spec (name);
  auto *target = gimp_value_array_index (values_.get (), slot);
  if (G_VALUE_TYPE (target) != G_PARAM_SPEC_VALUE_TYPE (spec)) drift ();
  filter_parameter_validate_copy (spec, value);
  if (G_VALUE_TYPE (&value) == GIMP_TYPE_CORE_OBJECT_ARRAY &&
      object_count (static_cast<GObject **> (g_value_get_boxed (&value))) != 1) invalid ();
  if (G_VALUE_TYPE (&value) == GIMP_TYPE_DOUBLE_ARRAY || G_VALUE_TYPE (&value) == GIMP_TYPE_INT32_ARRAY) {
    const auto *array = static_cast<const GimpArray *> (g_value_get_boxed (&value));
    const auto bytes = G_VALUE_TYPE (&value) == GIMP_TYPE_DOUBLE_ARRAY ? FilterLegacy::matrix_count * sizeof (gdouble) : FilterLegacy::channel_count * sizeof (gint32);
    if (!array || array->length != bytes) invalid ();
  }
  g_value_copy (&value, target); assigned_[slot] = true;
}
GimpValueArray *FilterParameterBinder::finish () const
{
  for (unsigned i = 0; i < schema_.size (); ++i)
    if (!assigned_[i]) throw std::invalid_argument ("Missing bundled Filter parameter assignment");
  return values_.get ();
}
void FilterParameterBinder::set_enum (const char *name, GType type, gint number)
{ if (type != GIMP_TYPE_RUN_MODE) invalid (); Value v (type); g_value_set_enum (v.get (), number); assign (name, *v.get ()); }
void FilterParameterBinder::set_int (const char *name, gint number)
{ Value v (G_TYPE_INT); g_value_set_int (v.get (), number); assign (name, *v.get ()); }
void FilterParameterBinder::set_double (const char *name, gdouble number)
{ Value v (G_TYPE_DOUBLE); g_value_set_double (v.get (), number); assign (name, *v.get ()); }
void FilterParameterBinder::set_boolean (const char *name, bool flag)
{ Value v (G_TYPE_BOOLEAN); g_value_set_boolean (v.get (), flag); assign (name, *v.get ()); }
void FilterParameterBinder::set_string (const char *name, const char *text)
{ Value v (G_TYPE_STRING); g_value_set_static_string (v.get (), text); assign (name, *v.get ()); }
void FilterParameterBinder::set_image (const char *name, GimpImage *image)
{ Value v (GIMP_TYPE_IMAGE); g_value_set_object (v.get (), image); assign (name, *v.get ()); }
void FilterParameterBinder::set_drawables (const char *name, GObject *const *objects, gsize count)
{
  if (!objects || count != 1 || !objects[0] || objects[count]) invalid ();
  Value v (GIMP_TYPE_CORE_OBJECT_ARRAY); g_value_set_static_boxed (v.get (), objects); assign (name, *v.get ());
}
void FilterParameterBinder::set_double_array (const char *name, const gdouble *numbers, gsize count)
{
  if (!numbers || count != FilterLegacy::matrix_count) invalid ();
  Value v (GIMP_TYPE_DOUBLE_ARRAY); gimp_value_set_static_double_array (v.get (), numbers, count); assign (name, *v.get ());
}
void FilterParameterBinder::set_int32_array (const char *name, const gint32 *numbers, gsize count)
{
  if (!numbers || count != FilterLegacy::channel_count) invalid ();
  Value v (GIMP_TYPE_INT32_ARRAY); gimp_value_set_static_int32_array (v.get (), numbers, count); assign (name, *v.get ());
}
}
