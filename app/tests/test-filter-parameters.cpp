/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Core metadata/binding boundaries. Synthetic native procedures receive the
 * bound values through the real core execution API; no provider test switch
 * or change to the immutable historical/wire fixtures is needed. */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpparamspecs.h"
#include "pdb/gimpprocedure.h"
void gimp_test_filter_parameter_reordering (Gimp *application);
void gimp_test_filter_parameter_metadata (Gimp *application);
void gimp_test_filter_parameter_assignment (Gimp *application);
void gimp_test_filter_parameter_lifetime (Gimp *application);
}
#include "core/gimpfilterprocedure-arguments.hpp"
#include "painter/object-ref.hpp"
#include "painter/resources.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace GimpPainter;
namespace {
using Object = ObjectRef<GObject>;
constexpr auto parameter_flags = static_cast<GParamFlags> (GIMP_PARAM_READWRITE);
const FilterProcedure routes[] = {FilterProcedure::blinds, FilterProcedure::small_tiles,
                                  FilterProcedure::retinex, FilterProcedure::convolution};

struct SpecDelete { void operator() (GParamSpec *spec) const { g_param_spec_unref (spec); } };
using Spec = std::unique_ptr<GParamSpec, SpecDelete>;
Spec keep_spec (GParamSpec *spec) { return Spec (g_param_spec_ref_sink (spec)); }

template<class Action> void rejects (Action action)
{
  bool caught = false;
  try { action (); }
  catch (const std::exception&) { caught = true; }
  g_assert_true (caught);
}

GParamSpec *integer (const char *name, gint minimum, gint maximum, gint initial)
{ return g_param_spec_int (name, name, name, minimum, maximum, initial, parameter_flags); }
GParamSpec *real (const char *name, gdouble minimum, gdouble maximum, gdouble initial)
{ return g_param_spec_double (name, name, name, minimum, maximum, initial, parameter_flags); }
GParamSpec *choice (bool orientation)
{
  auto *values = gimp_choice_new ();
  if (orientation)
    {
      gimp_choice_add (values, "horizontal", GIMP_ORIENTATION_HORIZONTAL, "Horizontal", nullptr);
      gimp_choice_add (values, "vertical", GIMP_ORIENTATION_VERTICAL, "Vertical", nullptr);
    }
  else
    {
      gimp_choice_add (values, "uniform", 0, "Uniform", nullptr);
      gimp_choice_add (values, "low", 1, "Low", nullptr);
      gimp_choice_add (values, "high", 2, "High", nullptr);
    }
  const char *name = orientation ? "orientation" : "scales-mode";
  /* The pspec takes the choice reference. */
  return gimp_param_spec_choice (name, name, name, values,
                                 orientation ? "horizontal" : "uniform", parameter_flags);
}

struct Scene
{
  explicit Scene (Gimp *application)
    : image (Object::adopt (G_OBJECT (gimp_image_new (application, 5, 5,
                                                    GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR))))
  {
    gimp_image_undo_disable (GIMP_IMAGE (image.get ()));
    layer = Object::sink (G_OBJECT (gimp_layer_new (GIMP_IMAGE (image.get ()), 5, 5,
      babl_format ("R'G'B'A u8"), "Parameter fixture", 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY)));
    g_assert_true (gimp_image_add_layer (GIMP_IMAGE (image.get ()), GIMP_LAYER (layer.get ()),
                                        nullptr, 0, FALSE));
  }
  Object image, layer;
};

struct Received
{
  FilterProcedure route;
  bool hidden;
  GimpImage *image;
  GObject *drawable;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  unsigned calls = 0;
};
Received *receiving = nullptr;

const GValue *named (GimpProcedure *procedure, const GimpValueArray *arguments, const char *name)
{
  for (gint i = 0; i < procedure->num_args; ++i)
    if (!std::strcmp (g_param_spec_get_name (procedure->args[i]), name))
      return gimp_value_array_index (arguments, i);
  g_error ("Missing receiving-marshal parameter: %s", name);
}
std::array<gdouble, 25> coefficients ()
{
  std::array<gdouble, 25> values;
  for (unsigned i = 0; i < values.size (); ++i) values[i] = (gint (i) - 12) / 16.0;
  values[1] = std::numeric_limits<gdouble>::denorm_min ();
  values[24] = -0.0;
  return values;
}
const std::array<gint32, 5> channel_flags {{-7, 0, 2, 1, -1}};

GimpValueArray *receive (GimpProcedure *procedure, Gimp *, GimpContext *,
                        GimpProgress *, const GimpValueArray *arguments, GError **)
{
  g_assert_nonnull (receiving);
  auto& state = *receiving;
  ++state.calls;
  g_assert_cmpint (gimp_value_array_length (arguments), ==, procedure->num_args);
  g_assert_cmpint (g_value_get_enum (named (procedure, arguments, "run-mode")), ==, GIMP_RUN_NONINTERACTIVE);
  g_assert_true (g_value_get_object (named (procedure, arguments, "image")) == state.image);
  auto **objects = static_cast<GObject **> (g_value_get_boxed (named (procedure, arguments, "drawables")));
  g_assert_nonnull (objects);
  g_assert_true (objects[0] == state.drawable);
  g_assert_null (objects[1]);
  g_assert_true (GIMP_IS_DRAWABLE (objects[0]));
  switch (state.route)
    {
    case FilterProcedure::blinds:
      g_assert_cmpint (g_value_get_int (named (procedure, arguments, "angle-displacement")), ==, 63);
      g_assert_cmpint (g_value_get_int (named (procedure, arguments, "num-segments")), ==, 7);
      g_assert_cmpstr (g_value_get_string (named (procedure, arguments, "orientation")), ==, "vertical");
      g_assert_true (g_value_get_boolean (named (procedure, arguments, "bg-transparent")));
      break;
    case FilterProcedure::small_tiles:
      g_assert_cmpint (g_value_get_int (named (procedure, arguments, "num-tiles")), ==, state.hidden ? 0 : 2);
      break;
    case FilterProcedure::retinex:
      g_assert_cmpint (g_value_get_int (named (procedure, arguments, "scale")), ==, state.hidden ? 256 : 250);
      g_assert_cmpint (g_value_get_int (named (procedure, arguments, "nscales")), ==, 0);
      g_assert_cmpstr (g_value_get_string (named (procedure, arguments, "scales-mode")), ==, "high");
      g_assert_cmpfloat (g_value_get_double (named (procedure, arguments, "cvar")), ==, std::nextafter (1.2, 2.0));
      break;
    case FilterProcedure::convolution:
      {
        const auto expected = coefficients ();
        gsize length = 0;
        const auto *matrix = gimp_value_get_double_array (named (procedure, arguments, "matrix"), &length);
        g_assert_cmpuint (length, ==, expected.size ());
        g_assert_cmpmem (matrix, length * sizeof (gdouble), expected.data (), sizeof expected);
        const auto *channels = gimp_value_get_int32_array (named (procedure, arguments, "channels"), &length);
        g_assert_cmpuint (length, ==, channel_flags.size ());
        g_assert_cmpmem (channels, length * sizeof (gint32), channel_flags.data (), sizeof channel_flags);
        g_assert_cmpint (g_value_get_int (named (procedure, arguments, "alpha-alg")), ==, -7);
        g_assert_cmpint (g_value_get_int (named (procedure, arguments, "border-mode")), ==, 1);
        g_assert_cmpfloat (g_value_get_double (named (procedure, arguments, "divisor")), ==, std::nextafter (1.0, 2.0));
        const gdouble offset = g_value_get_double (named (procedure, arguments, "offset")), expected_offset = -0.0;
        g_assert_cmpmem (&offset, sizeof offset, &expected_offset, sizeof expected_offset);
      }
      break;
    }
  return gimp_value_array_new_from_types (nullptr, GIMP_TYPE_PDB_STATUS_TYPE, state.status, G_TYPE_NONE);
}

Object procedure_new (FilterProcedure route, bool hidden = true, unsigned permutation = 0)
{
  auto owner = Object::adopt (G_OBJECT (gimp_procedure_new (receive, TRUE)));
  auto *procedure = GIMP_PROCEDURE (owner.get ());
  gimp_object_set_static_name (GIMP_OBJECT (procedure), "painter-parameter-boundary-fixture");
  gimp_procedure_add_argument (procedure, g_param_spec_enum ("run-mode", "run-mode", "run-mode",
    GIMP_TYPE_RUN_MODE, GIMP_RUN_NONINTERACTIVE, parameter_flags));
  gimp_procedure_add_argument (procedure, gimp_param_spec_image ("image", "image", "image", FALSE, parameter_flags));
  gimp_procedure_add_argument (procedure, gimp_param_spec_core_object_array ("drawables", "drawables", "drawables",
                                                                          GIMP_TYPE_DRAWABLE, parameter_flags));
  std::vector<GParamSpec *> suffix;
  switch (route)
    {
    case FilterProcedure::blinds:
      suffix = {integer ("angle-displacement", 0, 90, 30), integer ("num-segments", 1, 1024, 3), choice (true),
                g_param_spec_boolean ("bg-transparent", "bg-transparent", "bg-transparent", FALSE, parameter_flags)};
      break;
    case FilterProcedure::small_tiles:
      suffix = {integer ("num-tiles", hidden ? 0 : 2, 6, 2)};
      break;
    case FilterProcedure::retinex:
      suffix = {integer ("scale", 16, hidden ? 256 : 250, 240), integer ("nscales", 0, 8, 3),
                choice (false), real ("cvar", 0, 4, 1.2)};
      break;
    case FilterProcedure::convolution:
      suffix = {gimp_param_spec_double_array ("matrix", "matrix", "matrix", parameter_flags),
                integer ("alpha-alg", G_MININT, G_MAXINT, 1), real ("divisor", -G_MAXDOUBLE, G_MAXDOUBLE, 1),
                real ("offset", -G_MAXDOUBLE, G_MAXDOUBLE, 0),
                gimp_param_spec_int32_array ("channels", "channels", "channels", parameter_flags),
                integer ("border-mode", 0, 2, 2)};
      break;
    }
  if (permutation == 1) std::reverse (suffix.begin (), suffix.end ());
  if (permutation == 2) std::rotate (suffix.begin (), suffix.begin () + 1, suffix.end ());
  for (auto *spec : suffix) gimp_procedure_add_argument (procedure, spec);
  return owner;
}

void inputs (FilterParameterBinder& binder, FilterProcedure route, bool hidden)
{
  switch (route)
    {
    case FilterProcedure::blinds:
      binder.set_boolean ("bg-transparent", true);
      binder.set_int ("num-segments", 7);
      binder.set_string ("orientation", "vertical");
      binder.set_int ("angle-displacement", 63);
      break;
    case FilterProcedure::small_tiles: binder.set_int ("num-tiles", hidden ? 0 : 2); break;
    case FilterProcedure::retinex:
      binder.set_double ("cvar", std::nextafter (1.2, 2.0));
      binder.set_int ("nscales", 0);
      binder.set_string ("scales-mode", "high");
      binder.set_int ("scale", hidden ? 256 : 250);
      break;
    case FilterProcedure::convolution:
      {
        const auto matrix = coefficients ();
        binder.set_int32_array ("channels", channel_flags.data (), channel_flags.size ());
        binder.set_double ("offset", -0.0);
        binder.set_int ("border-mode", 1);
        binder.set_double_array ("matrix", matrix.data (), matrix.size ());
        binder.set_int ("alpha-alg", -7);
        binder.set_double ("divisor", std::nextafter (1.0, 2.0));
      }
      break;
    }
}
void context (FilterParameterBinder& binder, Scene& scene)
{
  GObject *objects[] = {scene.layer.get (), nullptr};
  binder.set_drawables ("drawables", objects, 1);
  binder.set_image ("image", GIMP_IMAGE (scene.image.get ()));
  binder.set_enum ("run-mode", GIMP_TYPE_RUN_MODE, GIMP_RUN_NONINTERACTIVE);
}
void execute (Gimp *application, GimpProcedure *procedure, FilterParameterBinder& binder, Received& state)
{
  GError *error = nullptr;
  receiving = &state;
  auto *result = gimp_procedure_execute (procedure, application, gimp_get_user_context (application),
                                       nullptr, binder.finish (), &error);
  receiving = nullptr;
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_cmpint (g_value_get_enum (gimp_value_array_index (result, 0)), ==, state.status);
  g_assert_cmpuint (state.calls, ==, 1);
  gimp_value_array_unref (result);
}

void replace (GimpProcedure *procedure, unsigned slot, GParamSpec *spec)
{
  g_param_spec_unref (procedure->args[slot]);
  procedure->args[slot] = g_param_spec_ref_sink (spec);
}
template<class Change> void rejects_metadata (FilterProcedure route, Change change, bool hidden = true)
{
  auto owner = procedure_new (route, hidden);
  auto *procedure = GIMP_PROCEDURE (owner.get ());
  change (procedure);
  rejects ([&] { FilterParameterSchema schema (procedure, route, hidden); });
}
void finalized (gpointer data, GObject *) { ++*static_cast<unsigned *> (data); }
} // namespace

extern "C" void gimp_test_filter_parameter_reordering (Gimp *application)
{
  Scene scene (application);
  unsigned executed = 0;
  for (auto route : routes) for (bool hidden : {false, true})
    for (unsigned permutation = 0; permutation < 3; ++permutation)
      {
        auto owner = procedure_new (route, hidden, permutation);
        auto *procedure = GIMP_PROCEDURE (owner.get ());
        FilterParameterSchema schema (procedure, route, hidden);
        g_assert_cmpuint (schema.slot ("run-mode"), ==, 0);
        g_assert_cmpuint (schema.slot ("image"), ==, 1);
        g_assert_cmpuint (schema.slot ("drawables"), ==, 2);
        FilterParameterBinder binder (schema);
        inputs (binder, route, hidden);
        context (binder, scene);
        Received state {route, hidden, GIMP_IMAGE (scene.image.get ()), scene.layer.get ()};
        execute (application, procedure, binder, state);
        ++executed;
      }
  g_assert_cmpuint (executed, ==, 24);
  g_test_message ("24 typed core calls: four routes, public/hidden metadata, three suffix orders (Small Tiles has one input)");
}

extern "C" void gimp_test_filter_parameter_metadata (Gimp *)
{
  for (auto route : routes)
    {
      rejects_metadata (route, [] (GimpProcedure *p) { std::swap (p->args[0], p->args[1]); });
      rejects_metadata (route, [] (GimpProcedure *p) { std::swap (p->args[1], p->args[2]); });
      rejects_metadata (route, [] (GimpProcedure *p) { g_param_spec_unref (p->args[--p->num_args]); });
      rejects_metadata (route, [] (GimpProcedure *p) { gimp_procedure_add_argument (p, integer ("unexpected", 0, 9, 0)); });
      rejects_metadata (route, [] (GimpProcedure *p) { replace (p, 3, integer ("renamed", 0, 9, 0)); });
      rejects_metadata (route, [] (GimpProcedure *p) {
        replace (p, 3, g_param_spec_enum ("run-mode", "run-mode", "run-mode", GIMP_TYPE_RUN_MODE,
                                         GIMP_RUN_NONINTERACTIVE, parameter_flags));
      });
      rejects_metadata (route, [] (GimpProcedure *p) {
        replace (p, 3, g_param_spec_string (g_param_spec_get_name (p->args[3]), "wrong type", "wrong type", nullptr,
                                          parameter_flags));
      });
      rejects_metadata (route, [] (GimpProcedure *p) { p->args[3]->flags = static_cast<GParamFlags> (p->args[3]->flags & ~G_PARAM_WRITABLE); });
      rejects_metadata (route, [] (GimpProcedure *p) { G_PARAM_SPEC_ENUM (p->args[0])->default_value = GIMP_RUN_INTERACTIVE; });
      rejects_metadata (route, [] (GimpProcedure *p) {
        replace (p, 1, gimp_param_spec_image ("image", "image", "image", TRUE, parameter_flags));
      });
      rejects_metadata (route, [] (GimpProcedure *p) {
        replace (p, 2, gimp_param_spec_core_object_array ("drawables", "drawables", "drawables", G_TYPE_OBJECT,
                                                        parameter_flags));
      });
      /* Every numeric field is checked by name, including reordered schemas. */
      auto original = procedure_new (route);
      auto *p = GIMP_PROCEDURE (original.get ());
      for (gint slot = 3; slot < p->num_args; ++slot)
        for (unsigned drift = 0; drift < 2; ++drift)
          if (G_IS_PARAM_SPEC_INT (p->args[slot]) || G_IS_PARAM_SPEC_DOUBLE (p->args[slot]))
            rejects_metadata (route, [=] (GimpProcedure *changed) {
              auto *spec = changed->args[slot];
              if (G_IS_PARAM_SPEC_INT (spec))
                {
                  if (drift) --G_PARAM_SPEC_INT (spec)->maximum;
                  else ++G_PARAM_SPEC_INT (spec)->default_value;
                }
              else
                {
                  if (drift) G_PARAM_SPEC_DOUBLE (spec)->maximum = std::nextafter (G_PARAM_SPEC_DOUBLE (spec)->maximum, 0.0);
                  else G_PARAM_SPEC_DOUBLE (spec)->default_value = std::nextafter (G_PARAM_SPEC_DOUBLE (spec)->default_value,
                                                                                  std::numeric_limits<gdouble>::infinity ());
                }
            });
    }
  rejects_metadata (FilterProcedure::blinds, [] (GimpProcedure *p) { G_PARAM_SPEC_BOOLEAN (p->args[6])->default_value = TRUE; });
  for (auto route : {FilterProcedure::blinds, FilterProcedure::retinex})
    for (unsigned drift = 0; drift < 3; ++drift)
      rejects_metadata (route, [=] (GimpProcedure *p) {
        auto *spec = p->args[5];
        auto *values = gimp_param_spec_choice_get_choice (spec);
        if (drift == 0) gimp_choice_add (values, "extra", 17, "Extra", nullptr);
        else if (drift == 1)
          {
            auto *replacement = gimp_choice_new ();
            const bool blinds = route == FilterProcedure::blinds;
            gimp_choice_add (replacement, blinds ? "horizontal" : "uniform", blinds ? GIMP_ORIENTATION_HORIZONTAL : 0,
                             "First", nullptr);
            gimp_choice_add (replacement, blinds ? "vertical" : "low", blinds ? 19 : 1, "Second", nullptr);
            if (!blinds) gimp_choice_add (replacement, "high", 19, "Third", nullptr);
            const char *name = blinds ? "orientation" : "scales-mode";
            replace (p, 5, gimp_param_spec_choice (name, name, name, replacement,
                                                  blinds ? "horizontal" : "uniform", parameter_flags));
          }
        else
          {
            g_free (G_PARAM_SPEC_STRING (spec)->default_value);
            G_PARAM_SPEC_STRING (spec)->default_value = g_strdup (route == FilterProcedure::blinds ? "vertical" : "high");
          }
      });
  for (auto route : {FilterProcedure::small_tiles, FilterProcedure::retinex})
    for (bool hidden : {false, true})
      {
        auto owner = procedure_new (route, hidden);
        rejects ([&] { FilterParameterSchema schema (GIMP_PROCEDURE (owner.get ()), route, !hidden); });
      }
}

extern "C" void gimp_test_filter_parameter_assignment (Gimp *application)
{
  Scene scene (application);
  auto owner = procedure_new (FilterProcedure::blinds);
  FilterParameterSchema schema (GIMP_PROCEDURE (owner.get ()), FilterProcedure::blinds);
  FilterParameterBinder binder (schema);
  rejects ([&] { binder.finish (); });
  rejects ([&] { binder.set_int ("missing", 1); });
  rejects ([&] { binder.set_double ("angle-displacement", 30.0); });
  rejects ([&] { binder.set_int ("angle-displacement", 91); });
  rejects ([&] { binder.set_string ("orientation", "diagonal"); });
  rejects ([&] { binder.set_enum ("run-mode", G_TYPE_INT, GIMP_RUN_NONINTERACTIVE); });
  inputs (binder, FilterProcedure::blinds, true);
  rejects ([&] { binder.finish (); });
  rejects ([&] { binder.set_int ("angle-displacement", 12); });
  context (binder, scene);
  Received state {FilterProcedure::blinds, true, GIMP_IMAGE (scene.image.get ()), scene.layer.get ()};
  execute (application, GIMP_PROCEDURE (owner.get ()), binder, state);

  /* The real validator changes non-NULL empty to NULL while returning FALSE. */
  auto objects_spec = keep_spec (gimp_param_spec_core_object_array ("objects", "objects", "objects", G_TYPE_OBJECT,
                                                                   parameter_flags));
  GObject *empty[] = {nullptr};
  Value original (GIMP_TYPE_CORE_OBJECT_ARRAY), copy (GIMP_TYPE_CORE_OBJECT_ARRAY), null_objects (GIMP_TYPE_CORE_OBJECT_ARRAY);
  g_value_set_static_boxed (original.get (), empty);
  g_value_copy (original.get (), copy.get ());
  g_assert_nonnull (g_value_get_boxed (copy.get ()));
  g_assert_false (G_PARAM_SPEC_GET_CLASS (objects_spec.get ())->value_validate (objects_spec.get (), copy.get ()));
  g_assert_null (g_value_get_boxed (copy.get ()));
  g_value_copy (original.get (), copy.get ());
  const gboolean wrapper_changed = g_param_value_validate (objects_spec.get (), copy.get ());
  g_assert_null (g_value_get_boxed (copy.get ()));
  g_test_message ("Empty CoreObjectArray: GIMP class callback returns FALSE; installed GLib wrapper returns %s",
                  wrapper_changed ? "TRUE" : "FALSE");
  rejects ([&] { filter_parameter_validate_copy (objects_spec.get (), *original.get ()); });
  g_assert_true (g_value_get_boxed (original.get ()) == empty);
  g_assert_false (filter_parameter_values_equal (*original.get (), *null_objects.get ()));

  auto first = Object::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  auto second = Object::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  auto third = Object::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  GObject *ab[] = {first.get (), second.get (), nullptr}, *ac[] = {first.get (), third.get (), nullptr};
  Value va (GIMP_TYPE_CORE_OBJECT_ARRAY), vb (GIMP_TYPE_CORE_OBJECT_ARRAY);
  g_value_set_static_boxed (va.get (), ab); g_value_set_static_boxed (vb.get (), ac);
  g_assert_cmpint (g_param_values_cmp (objects_spec.get (), va.get (), vb.get ()), ==, 0);
  g_assert_false (filter_parameter_values_equal (*va.get (), *vb.get ()));
  std::swap (ac[0], ac[1]);
  g_assert_false (filter_parameter_values_equal (*va.get (), *vb.get ()));
  ac[0] = first.get (); ac[1] = second.get ();
  g_assert_true (filter_parameter_values_equal (*va.get (), *vb.get ()));

  for (GType type : {GIMP_TYPE_DOUBLE_ARRAY, GIMP_TYPE_INT32_ARRAY})
    {
      auto spec = keep_spec (type == GIMP_TYPE_DOUBLE_ARRAY
        ? gimp_param_spec_double_array ("array", "array", "array", parameter_flags)
        : gimp_param_spec_int32_array ("array", "array", "array", parameter_flags));
      const gsize width = type == GIMP_TYPE_DOUBLE_ARRAY ? sizeof (gdouble) : sizeof (gint32);
      gdouble doubles_a[] = {1.0, -0.0, 3.0}, doubles_b[] = {1.0, 0.0, 3.0};
      gint32 ints_a[] = {1, 2, 3}, ints_b[] = {1, 2, 4};
      Value a (type), b (type), nil (type), empty_value (type);
      if (type == GIMP_TYPE_DOUBLE_ARRAY)
        {
          gimp_value_set_double_array (a.get (), doubles_a, 3);
          gimp_value_set_double_array (b.get (), doubles_b, 3);
        }
      else
        {
          gimp_value_set_int32_array (a.get (), ints_a, 3);
          gimp_value_set_int32_array (b.get (), ints_b, 3);
        }
      g_assert_cmpint (g_param_values_cmp (spec.get (), a.get (), b.get ()), ==, 0);
      g_assert_false (filter_parameter_values_equal (*a.get (), *b.get ()));
      g_value_copy (a.get (), b.get ());
      g_assert_true (filter_parameter_values_equal (*a.get (), *b.get ()));
      GimpArray empty_array {nullptr, 0, TRUE};
      g_value_set_static_boxed (empty_value.get (), &empty_array);
      g_assert_false (filter_parameter_values_equal (*nil.get (), *empty_value.get ()));

      /* Deliberately tiny backing storage proves shape/size rejection precedes
       * copy or element reads. The GValue borrows this descriptor throughout. */
      guint8 byte = 0;
      const std::array<GimpArray, 5> malformed {{{&byte, width - 1, TRUE}, {nullptr, width, TRUE},
        {&byte, 0, TRUE}, {&byte, 26 * width, TRUE}, {&byte, G_MAXSIZE, TRUE}}};
      for (const auto& array : malformed)
        {
          Value bad (type);
          g_value_set_static_boxed (bad.get (), &array);
          rejects ([&] { filter_parameter_validate_copy (spec.get (), *bad.get ()); });
          g_assert_true (g_value_get_boxed (bad.get ()) == &array);
          g_assert_cmpuint (byte, ==, 0);
        }
    }

  /* Exercise the public FALSE-return branch even on GLib versions which
   * detect a changed outer boxed pointer themselves. This test-only override
   * changes a later numeric element in place, retaining the outer GValue.
   * No event loop or procedure is run while this class callback is replaced. */
  {
    auto spec = keep_spec (gimp_param_spec_double_array ("array", "array", "array", parameter_flags));
    struct ValidateOverride
    {
      GParamSpecClass *klass;
      decltype (GParamSpecClass::value_validate) previous;
      explicit ValidateOverride (GParamSpec *spec) : klass (G_PARAM_SPEC_GET_CLASS (spec)), previous (klass->value_validate)
      {
        klass->value_validate = [] (GParamSpec *, GValue *value) -> gboolean {
          auto *array = static_cast<GimpArray *> (g_value_get_boxed (value));
          g_assert_cmpuint (array->length, ==, 25 * sizeof (gdouble));
          const gdouble replacement = 4.0;
          std::memcpy (array->data + 24 * sizeof (gdouble), &replacement, sizeof replacement);
          return FALSE;
        };
      }
      ~ValidateOverride () { klass->value_validate = previous; }
    } validation (spec.get ());
    auto elements = coefficients ();
    elements[24] = 3.0;
    Value source (GIMP_TYPE_DOUBLE_ARRAY), changed (GIMP_TYPE_DOUBLE_ARRAY);
    gimp_value_set_double_array (source.get (), elements.data (), elements.size ());
    g_value_copy (source.get (), changed.get ());
    g_assert_false (g_param_value_validate (spec.get (), changed.get ()));
    g_assert_false (filter_parameter_values_equal (*source.get (), *changed.get ()));
    rejects ([&] { filter_parameter_validate_copy (spec.get (), *source.get ()); });
    gsize count = 0;
    const auto *retained = gimp_value_get_double_array (source.get (), &count);
    g_assert_cmpmem (retained, count * sizeof (gdouble), elements.data (), sizeof elements);
  }

  auto double_spec = keep_spec (real ("number", -G_MAXDOUBLE, G_MAXDOUBLE, 0));
  Value number (G_TYPE_DOUBLE), other (G_TYPE_DOUBLE), text (G_TYPE_STRING), empty_text (G_TYPE_STRING);
  g_value_set_double (number.get (), -0.0); g_value_set_double (other.get (), 0.0);
  g_assert_false (filter_parameter_values_equal (*number.get (), *other.get ()));
  g_value_set_double (number.get (), 1.0); g_value_set_double (other.get (), std::nextafter (1.0, 2.0));
  g_assert_false (filter_parameter_values_equal (*number.get (), *other.get ()));
  const gdouble tiny = std::numeric_limits<gdouble>::denorm_min ();
  g_value_set_double (number.get (), tiny);
  filter_parameter_validate_copy (double_spec.get (), *number.get ());
  const gdouble retained = g_value_get_double (number.get ());
  g_assert_cmpmem (&tiny, sizeof tiny, &retained, sizeof retained);
  g_assert_cmpfloat (static_cast<float> (tiny), ==, 0.0f);
  auto retinex = procedure_new (FilterProcedure::retinex);
  FilterParameterSchema retinex_schema (GIMP_PROCEDURE (retinex.get ()), FilterProcedure::retinex);
  FilterParameterBinder tiny_binding (retinex_schema);
  tiny_binding.set_double ("cvar", tiny);
  tiny_binding.set_int ("scale", 256);
  tiny_binding.set_int ("nscales", 0);
  tiny_binding.set_string ("scales-mode", "high");
  context (tiny_binding, scene);
  const gdouble bound_tiny = g_value_get_double (gimp_value_array_index (tiny_binding.finish (), retinex_schema.slot ("cvar")));
  g_assert_cmpmem (&tiny, sizeof tiny, &bound_tiny, sizeof bound_tiny);
  for (gdouble invalid : {std::numeric_limits<gdouble>::infinity (), std::numeric_limits<gdouble>::quiet_NaN ()})
    {
      g_value_set_double (number.get (), invalid);
      rejects ([&] { filter_parameter_validate_copy (double_spec.get (), *number.get ()); });
    }
  g_value_set_static_string (empty_text.get (), "");
  g_assert_false (filter_parameter_values_equal (*text.get (), *empty_text.get ()));
  g_assert_false (filter_parameter_values_equal (*text.get (), *number.get ()));

  auto convolution = procedure_new (FilterProcedure::convolution);
  FilterParameterBinder arrays (FilterParameterSchema (GIMP_PROCEDURE (convolution.get ()), FilterProcedure::convolution));
  const auto matrix = coefficients ();
  rejects ([&] { arrays.set_double_array ("matrix", matrix.data (), 24); });
  rejects ([&] { arrays.set_int32_array ("channels", channel_flags.data (), 4); });
  GObject *two[] = {scene.layer.get (), scene.layer.get (), nullptr};
  rejects ([&] { arrays.set_drawables ("drawables", two, 2); });
  Value too_short (GIMP_TYPE_DOUBLE_ARRAY);
  gimp_value_set_double_array (too_short.get (), matrix.data (), 24);
  rejects ([&] { arrays.assign ("matrix", *too_short.get ()); });
}

extern "C" void gimp_test_filter_parameter_lifetime (Gimp *application)
{
  for (auto status : {GIMP_PDB_SUCCESS, GIMP_PDB_EXECUTION_ERROR, GIMP_PDB_CANCEL})
    {
      unsigned image_finalized = 0, layer_finalized = 0;
      Object drawable_lease;
      {
        Scene scene (application);
        g_object_weak_ref (scene.image.get (), finalized, &image_finalized);
        g_object_weak_ref (scene.layer.get (), finalized, &layer_finalized);
        drawable_lease = Object::retain (scene.layer.get ());
        auto owner = procedure_new (FilterProcedure::small_tiles);
        auto *procedure = GIMP_PROCEDURE (owner.get ());
        FilterParameterBinder binder (FilterParameterSchema (procedure, FilterProcedure::small_tiles));
        auto *original = g_new0 (GObject *, 2);
        original[0] = scene.layer.get ();
        binder.set_drawables ("drawables", original, 1);
        g_free (original);
        binder.set_image ("image", GIMP_IMAGE (scene.image.get ()));
        binder.set_enum ("run-mode", GIMP_TYPE_RUN_MODE, GIMP_RUN_NONINTERACTIVE);
        binder.set_int ("num-tiles", 0);
        Received state {FilterProcedure::small_tiles, true, GIMP_IMAGE (scene.image.get ()), scene.layer.get (), status};
        /* Release the original references. The image GValue and independent
         * drawable ObjectRef are the execution leases, not the shallow box. */
        scene.layer.reset ();
        scene.image.reset ();
        g_assert_cmpuint (image_finalized, ==, 0);
        g_assert_cmpuint (layer_finalized, ==, 0);
        execute (application, procedure, binder, state);
        g_assert_cmpuint (layer_finalized, ==, 0);
      }
      /* Binder cleanup releases its image, which drops the image's layer
       * owner. The separate lease still protects recovery after any status. */
      g_assert_cmpuint (image_finalized, ==, 1);
      g_assert_cmpuint (layer_finalized, ==, 0);
      g_assert_true (GIMP_IS_LAYER (drawable_lease.get ()));
      drawable_lease.reset ();
      g_assert_cmpuint (layer_finalized, ==, 1);
    }

  /* Multiple borrowed elements survive validation and exception unwinding
   * after both original container and all outside element owners disappear. */
  unsigned gone[2] = {0, 0};
  std::array<Object, 2> leases;
  {
    auto spec = keep_spec (gimp_param_spec_core_object_array ("objects", "objects", "objects", G_TYPE_OBJECT,
                                                            parameter_flags));
    Value original (GIMP_TYPE_CORE_OBJECT_ARRAY), copied (GIMP_TYPE_CORE_OBJECT_ARRAY);
    GObject **objects = g_new0 (GObject *, 3);
    for (unsigned i = 0; i < 2; ++i)
      {
        objects[i] = G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr));
        g_object_weak_ref (objects[i], finalized, &gone[i]);
        leases[i] = Object::retain (objects[i]);
      }
    g_value_take_boxed (original.get (), objects);
    g_value_copy (original.get (), copied.get ());
    for (unsigned i = 0; i < 2; ++i) g_object_unref (objects[i]);
    g_value_set_boxed (original.get (), nullptr);
    filter_parameter_validate_copy (spec.get (), *copied.get ());
    auto **received = static_cast<GObject **> (g_value_get_boxed (copied.get ()));
    for (unsigned i = 0; i < 2; ++i)
      {
        g_assert_cmpuint (gone[i], ==, 0);
        g_assert_true (received[i] == leases[i].get ());
      }
    rejects ([&] {
      Value rejected (GIMP_TYPE_CORE_OBJECT_ARRAY);
      g_value_copy (copied.get (), rejected.get ());
      auto wrong = keep_spec (gimp_param_spec_core_object_array ("objects", "objects", "objects", GIMP_TYPE_DRAWABLE,
                                                               parameter_flags));
      filter_parameter_validate_copy (wrong.get (), *rejected.get ());
    });
  }
  for (unsigned i = 0; i < 2; ++i)
    {
      g_assert_cmpuint (gone[i], ==, 0);
      leases[i].reset ();
      g_assert_cmpuint (gone[i], ==, 1);
    }
}
