/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core-types.h"
#include "gimp.h"
#include "gimpimage.h"
#include "gimpdrawable.h"
#include "gimpparamspecs.h"
#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "plug-in/plug-in-types.h"
#include "plug-in/gimpplugindef.h"
#include "plug-in/gimppluginmanager.h"
#include "plug-in/gimppluginprocedure.h"
#include "gimpfilterparametereditor.h"
}
#include "gimpfilterparametereditor.hpp"
#include "gimpfilterprocedure-arguments.hpp"
#include "gimpfilterpaths.hpp"
#include "painter/binding-store.hpp"
#include <array>
#include <cstring>
#include <limits>

namespace GimpPainter {
namespace {
constexpr unsigned route_count = 4, max_registry_nodes = 4096, max_text = 4096;
[[noreturn]] void unavailable (const char *reason)
{ throw FilterEditorSchemaError (FilterEditorSchemaStatus::unavailable, reason); }
[[noreturn]] void incompatible (const char *reason)
{ throw FilterEditorSchemaError (FilterEditorSchemaStatus::incompatible, reason); }
unsigned route_index (FilterProcedure route)
{
  const auto value = static_cast<unsigned> (route);
  if (!value || value > route_count) incompatible ("This filter has no admitted parameter schema");
  return value - 1;
}
std::size_t text_length (const char *value)
{
  if (!value) return 0;
  for (std::size_t size = 0; size <= max_text; ++size)
    if (!value[size]) return size;
  incompatible ("Bundled filter metadata text exceeds the editor limit");
}
bool named (GimpProcedure *procedure, const char *name)
{
  if (!procedure || !name) return false;
  const auto *actual = gimp_object_get_name (procedure);
  return actual && text_length (actual) == std::strlen (name) && !std::strcmp (actual, name);
}
struct Budget {
  std::size_t bytes = 0, specs = 0;
  void add (std::size_t count)
  {
    if (count > FilterEditorSchema::max_bytes - bytes)
      incompatible ("Bundled filter metadata exceeds the 64 KiB editor limit");
    bytes += count;
  }
  std::string text (const char *value)
  {
    const auto size = text_length (value);
    if (value && !g_utf8_validate (value, size, nullptr))
      incompatible ("Bundled filter metadata contains invalid UTF-8 text");
    add (size + 1);
    return value ? std::string (value, size) : std::string ();
  }
};

struct Provider {
  std::array<ObjectRef<GObject>, 2> procedures;
  std::string path;
  unsigned count = 0;
  gint64 definition_mtime = 0;
  bool queried_during_restore = false;
  FilterEditorSchemaStatus failure_status = FilterEditorSchemaStatus::unavailable;
  std::string failure_reason;
};
/* Capturing only provenance is necessary because normal restore disposes the
 * temporary defs before the first dialog can open. No metadata snapshot or
 * new query occurs here. Only the admitted procedure objects are retained;
 * potentially large unrelated definition/help metadata is never retained. */
struct Providers {
  std::array<Provider, route_count> routes;
  bool captured = false;
  void close () noexcept {
    std::array<Provider, route_count> retired;
    captured = false;
    routes.swap (retired);
    /* Releasing a provider can call back into the manager. Nothing below this
     * point accesses the implementation; all owning state is already empty. */
  }
};
struct ProvidersSlot : SlotSpec<GObject, Providers> {};

Provider capture_provider (GimpPlugInManager *manager, FilterProcedure route)
{
  Provider result;
  result.path = filter_registered_plugin_path (route);
  auto expected = ObjectRef<GObject>::adopt (G_OBJECT (g_file_new_for_path (result.path.c_str ())));
  GimpPlugInDef *definition = nullptr;
  unsigned visited = 0;
  for (auto *node = manager->plug_in_defs; node; node = node->next)
    {
      if (++visited > max_registry_nodes)
        unavailable ("The plug-in registry exceeds the bounded editor lookup limit");
      auto *candidate = static_cast<GimpPlugInDef *> (node->data);
      if (!candidate || !GIMP_IS_PLUG_IN_DEF (candidate) || !candidate->file) continue;
      if (g_file_equal (candidate->file, G_FILE (expected.get ())))
        {
          if (definition) incompatible ("The bundled filter provider is registered more than once");
          definition = candidate;
        }
    }
  if (!definition) unavailable ("The trusted bundled filter is not registered in this session");
  if (definition->has_init)
    incompatible ("The bundled filter provider requires initialization");
  const auto& names = filter_editor_route_names (route);
  const unsigned count = names.public_name && std::strcmp (names.execution, names.public_name) ? 2 : 1;
  unsigned seen = 0;
  for (auto *node = definition->procedures; node; node = node->next)
    {
      if (++seen > count) incompatible ("The bundled filter has unexpected procedure registrations");
      auto *procedure = static_cast<GimpProcedure *> (node->data);
      if (!procedure || !GIMP_IS_PLUG_IN_PROCEDURE (procedure))
        incompatible ("The bundled filter registration is not a plug-in procedure");
      const unsigned slot = named (procedure, names.execution) ? 0 :
        count == 2 && named (procedure, names.public_name) ? 1 : 2;
      if (slot >= count || result.procedures[slot])
        incompatible ("The bundled filter has an unknown or duplicate procedure registration");
      auto *plugin = GIMP_PLUG_IN_PROCEDURE (procedure);
      if (!plugin->file || !g_file_equal (plugin->file, G_FILE (expected.get ())) ||
          plugin->file_proc || plugin->batch_interpreter || plugin->installed_during_init ||
          procedure->proc_type != GIMP_PDB_PROC_TYPE_PLUGIN)
        incompatible ("The bundled filter executable registration is incompatible");
      result.procedures[slot] = ObjectRef<GObject>::retain (G_OBJECT (procedure));
    }
  if (seen != count) incompatible ("The bundled filter is missing a required procedure registration");
  result.definition_mtime = definition->mtime;
  /* Normal restore never clears needs_query after its synchronous query loop.
   * At this hook it records query history, not pending work. The caller is the
   * completed restore stage; ordinary dialog lookup cannot create attestations. */
  result.queried_during_restore = definition->needs_query;
  result.count = count;
  return result;
}

Provider registered_provider (Gimp *gimp, FilterProcedure route)
{
  if (!gimp || !gimp->pdb || !gimp->plug_in_manager)
    unavailable ("The filter procedure registry is unavailable");
  auto *manager = gimp->plug_in_manager;
  auto *store = BindingStore::find (G_OBJECT (manager));
  if (!store || store->state () != BindingStore::State::active)
    unavailable ("The filter procedure registry is closed or unavailable");
  Provider provider = store->read<ProvidersSlot> ([&] (const Providers& providers) {
    if (!providers.captured) unavailable ("The trusted filter provider restore has not completed");
    return providers.routes[route_index (route)];
  });
  if (!provider.count)
    {
      if (!provider.failure_reason.empty ())
        throw FilterEditorSchemaError (provider.failure_status, provider.failure_reason.c_str ());
      unavailable ("The trusted bundled filter is not registered in this session");
    }
  auto expected = ObjectRef<GObject>::adopt (G_OBJECT (g_file_new_for_path (provider.path.c_str ())));
  const auto& names = filter_editor_route_names (route);
  unsigned seen = 0, visited = 0;
  std::array<bool, 2> found {};
  for (auto *node = manager->plug_in_procedures; node; node = node->next)
    {
      if (++visited > max_registry_nodes)
        unavailable ("The plug-in registry exceeds the bounded editor lookup limit");
      auto *procedure = static_cast<GimpPlugInProcedure *> (node->data);
      if (!procedure || !GIMP_IS_PLUG_IN_PROCEDURE (procedure) || !procedure->file ||
          !g_file_equal (procedure->file, G_FILE (expected.get ()))) continue;
      const unsigned slot = node->data == provider.procedures[0].get () ? 0 :
        provider.count == 2 && node->data == provider.procedures[1].get () ? 1 : 2;
      if (++seen > provider.count || slot >= provider.count || found[slot])
        incompatible ("The bundled filter provider procedure identity changed");
      found[slot] = true;
    }
  if (seen != provider.count) incompatible ("The bundled filter provider registrations changed");
  for (unsigned i = 0; i < provider.count; ++i)
    {
      auto *procedure = GIMP_PROCEDURE (provider.procedures[i].get ());
      const auto *name = i ? names.public_name : names.execution;
      /* A shadowing registration is not the original bundled provider, even
       * when the first procedure currently returned by lookup happens to match. */
      auto *registrations = static_cast<GList *> (g_hash_table_lookup (gimp->pdb->procedures, name));
      if (!registrations) unavailable ("A required bundled filter procedure is not registered");
      if (registrations->next || registrations->data != procedure ||
          gimp_pdb_lookup_procedure (gimp->pdb, name) != procedure)
        incompatible ("A bundled filter procedure was replaced or registered more than once");
    }
  return provider;
}

void check_procedure (GimpProcedure *procedure, FilterProcedure route,
                      bool hidden, const std::string& path)
{
  const auto& names = filter_editor_route_names (route);
  if (!procedure || !GIMP_IS_PLUG_IN_PROCEDURE (procedure) ||
      procedure->proc_type != GIMP_PDB_PROC_TYPE_PLUGIN || procedure->num_values ||
      !named (procedure, hidden ? names.execution : names.public_name) ||
      !procedure->args || procedure->num_args < 3 || procedure->num_args > 9)
    incompatible ("The bundled filter has an incompatible procedure signature");
  auto *plugin = GIMP_PLUG_IN_PROCEDURE (procedure);
  auto file = ObjectRef<GObject>::adopt (G_OBJECT (g_file_new_for_path (path.c_str ())));
  if (!plugin->file || !g_file_equal (plugin->file, G_FILE (file.get ())) ||
      plugin->file_proc || plugin->batch_interpreter || plugin->installed_during_init)
    incompatible ("The bundled filter executable binding changed");
  if (route != FilterProcedure::blinds)
    {
      text_length (plugin->image_types);
      if (g_strcmp0 (plugin->image_types, route == FilterProcedure::retinex ? "RGB*" : "RGB*, GRAY*") ||
          plugin->sensitivity_mask != GIMP_PROCEDURE_SENSITIVE_DRAWABLE ||
          (hidden && (plugin->menu_paths || plugin->menu_label)))
        incompatible ("The bundled filter image or visibility constraints changed");
    }
}

FilterEditorParameter snapshot_parameter (GParamSpec *spec, FilterProcedure route,
                                          unsigned slot, Budget& budget)
{
  if (!spec || ++budget.specs > FilterEditorSchema::max_specs)
    incompatible ("The bundled filter metadata exceeds the parameter limit");
  /* The owner must not invoke a default callback supplied by an arbitrary
   * subclass. These are the exact built-in spec types restored by the four
   * admitted bundled providers; the helper's broader type checks stay intact. */
  const auto kind = G_PARAM_SPEC_TYPE (spec);
  if (kind != G_TYPE_PARAM_INT && kind != G_TYPE_PARAM_DOUBLE && kind != G_TYPE_PARAM_BOOLEAN &&
      kind != G_TYPE_PARAM_ENUM && kind != GIMP_TYPE_PARAM_CHOICE && kind != GIMP_TYPE_PARAM_IMAGE &&
      kind != GIMP_TYPE_PARAM_CORE_OBJECT_ARRAY && kind != GIMP_TYPE_PARAM_DOUBLE_ARRAY &&
      kind != GIMP_TYPE_PARAM_INT32_ARRAY)
    incompatible ("The bundled filter parameter spec class is not admitted by this editor");
  budget.add (sizeof (FilterEditorParameter));
  FilterEditorParameter result;
  result.runtime_slot = slot;
  result.name = budget.text (g_param_spec_get_name (spec));
  if (result.name.empty ()) incompatible ("A bundled filter parameter has no canonical name");
  result.legacy = filter_editor_field (route, result.name.c_str ());
  result.value_type = G_PARAM_SPEC_VALUE_TYPE (spec);
  result.spec_type = G_PARAM_SPEC_TYPE (spec);
  result.type_name = budget.text (g_type_name (result.value_type));
  result.spec_type_name = budget.text (g_type_name (result.spec_type));
  result.flags = spec->flags;
  result.nick = budget.text (g_param_spec_get_nick (spec));
  result.blurb = budget.text (g_param_spec_get_blurb (spec));
  if (G_IS_PARAM_SPEC_INT (spec))
    {
      result.integer_min = G_PARAM_SPEC_INT (spec)->minimum;
      result.integer_max = G_PARAM_SPEC_INT (spec)->maximum;
      result.integer_default = G_PARAM_SPEC_INT (spec)->default_value;
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (spec))
    {
      result.real_min = G_PARAM_SPEC_DOUBLE (spec)->minimum;
      result.real_max = G_PARAM_SPEC_DOUBLE (spec)->maximum;
      result.real_default = G_PARAM_SPEC_DOUBLE (spec)->default_value;
    }
  else if (G_IS_PARAM_SPEC_BOOLEAN (spec))
    result.boolean_default = G_PARAM_SPEC_BOOLEAN (spec)->default_value;
  else if (GIMP_IS_PARAM_SPEC_CHOICE (spec))
    {
      auto *choice = gimp_param_spec_choice_get_choice (spec);
      if (!choice) incompatible ("A bundled filter choice has no metadata");
      /* The public choice getter uses GLib's lazily cached default GValue.
       * Validation reads the raw string pspec default instead. Bound and
       * snapshot that authoritative string before any getter can allocate a
       * cached copy; below we also require the cache to agree exactly. */
      const auto *initial = G_PARAM_SPEC_STRING (spec)->default_value;
      result.default_is_null = initial == nullptr;
      result.string_default = budget.text (initial);
      unsigned count = 0;
      for (auto *node = gimp_choice_list_nicks (choice); node; node = node->next)
        if (++count > FilterEditorSchema::max_specs)
          incompatible ("A bundled filter choice exceeds the editor limit");
      budget.add (count * sizeof (FilterEditorChoice));
      result.choices.reserve (count);
      for (auto *node = gimp_choice_list_nicks (choice); node; node = node->next)
        {
          const auto *nick = static_cast<const gchar *> (node->data);
          FilterEditorChoice item;
          item.nick = budget.text (nick);
          if (item.nick.empty ()) incompatible ("A bundled filter choice has no canonical nick");
          item.value = gimp_choice_get_id (choice, nick);
          item.label = budget.text (gimp_choice_get_label (choice, nick));
          item.help = budget.text (gimp_choice_get_help (choice, nick));
          result.choices.emplace_back (std::move (item));
        }
    }
  else if (G_IS_PARAM_SPEC_ENUM (spec))
    {
      auto *values = G_PARAM_SPEC_ENUM (spec)->enum_class;
      if (!values || values->n_values > FilterEditorSchema::max_specs)
        incompatible ("A bundled filter enum exceeds the editor limit");
      result.integer_default = G_PARAM_SPEC_ENUM (spec)->default_value;
      budget.add (values->n_values * sizeof (FilterEditorChoice));
      result.choices.reserve (values->n_values);
      for (unsigned i = 0; i < values->n_values; ++i)
        {
          FilterEditorChoice item;
          item.value = values->values[i].value;
          item.nick = budget.text (values->values[i].value_nick);
          item.label = budget.text (values->values[i].value_name);
          result.choices.emplace_back (std::move (item));
        }
    }
  else if (GIMP_IS_PARAM_SPEC_IMAGE (spec))
    {
      result.nullable = gimp_param_spec_image_none_allowed (spec);
      result.default_is_null = true;
    }
  else if (GIMP_IS_PARAM_SPEC_CORE_OBJECT_ARRAY (spec))
    {
      result.object_type_name = budget.text (g_type_name (gimp_param_spec_core_object_array_get_object_type (spec)));
      result.default_is_null = true;
      result.array_element_bytes = sizeof (GObject *);
      result.array_element_alignment = alignof (GObject *);
    }
  else if (GIMP_IS_PARAM_SPEC_DOUBLE_ARRAY (spec) || GIMP_IS_PARAM_SPEC_INT32_ARRAY (spec))
    {
      const bool real = GIMP_IS_PARAM_SPEC_DOUBLE_ARRAY (spec);
      result.default_is_null = true;
      result.array_element_bytes = real ? sizeof (gdouble) : sizeof (gint32);
      result.array_element_alignment = real ? alignof (gdouble) : alignof (gint32);
    }
  else incompatible ("The bundled filter parameter type is not supported by this editor");
  /* Inspect only the admitted scalar types and NULL object/array defaults.
   * Never copy arbitrary boxed/object defaults or adopt defaults as user data. */
  const auto *initial = g_param_spec_get_default_value (spec);
  if (!initial || G_VALUE_TYPE (initial) != result.value_type)
    incompatible ("A bundled filter default has an incompatible type");
  if (result.value_type == G_TYPE_INT)
    { if (g_value_get_int (initial) != result.integer_default) incompatible ("A bundled filter integer default changed"); }
  else if (result.value_type == G_TYPE_DOUBLE)
    {
      const auto value = g_value_get_double (initial);
      if (std::memcmp (&value, &result.real_default, sizeof value)) incompatible ("A bundled filter real default changed");
    }
  else if (result.value_type == G_TYPE_BOOLEAN)
    { if (bool (g_value_get_boolean (initial)) != result.boolean_default) incompatible ("A bundled filter boolean default changed"); }
  else if (result.value_type == GIMP_TYPE_RUN_MODE)
    { if (g_value_get_enum (initial) != result.integer_default) incompatible ("A bundled filter context default changed"); }
  else if (result.value_type == G_TYPE_STRING)
    {
      const auto *value = g_value_get_string (initial);
      text_length (value);
      if (!value || result.string_default != value) incompatible ("A bundled filter choice default changed");
    }
  else if (result.value_type == GIMP_TYPE_IMAGE)
    {
      if (g_value_get_object (initial))
        incompatible ("A bundled filter context default references an object");
    }
  else if (result.value_type == GIMP_TYPE_CORE_OBJECT_ARRAY ||
           result.value_type == GIMP_TYPE_DOUBLE_ARRAY || result.value_type == GIMP_TYPE_INT32_ARRAY)
    { if (g_value_get_boxed (initial)) incompatible ("A bundled filter array default is not NULL"); }
  else incompatible ("The bundled filter default type is not supported by this editor");
  return result;
}

bool real_equal (double a, double b) { return !std::memcmp (&a, &b, sizeof a); }
bool same (const FilterEditorParameter& a, const FilterEditorParameter& b)
{
  if (a.runtime_slot != b.runtime_slot || a.name != b.name || a.type_name != b.type_name ||
      a.spec_type_name != b.spec_type_name || a.nick != b.nick || a.blurb != b.blurb ||
      a.value_type != b.value_type || a.spec_type != b.spec_type || a.flags != b.flags ||
      a.integer_min != b.integer_min || a.integer_max != b.integer_max || a.integer_default != b.integer_default ||
      !real_equal (a.real_min, b.real_min) || !real_equal (a.real_max, b.real_max) || !real_equal (a.real_default, b.real_default) ||
      a.default_is_null != b.default_is_null || a.boolean_default != b.boolean_default || a.nullable != b.nullable ||
      a.array_element_bytes != b.array_element_bytes || a.array_element_alignment != b.array_element_alignment ||
      a.string_default != b.string_default || a.object_type_name != b.object_type_name || a.choices.size () != b.choices.size ()) return false;
  for (std::size_t i = 0; i < a.choices.size (); ++i)
    if (a.choices[i].value != b.choices[i].value || a.choices[i].nick != b.choices[i].nick ||
        a.choices[i].label != b.choices[i].label || a.choices[i].help != b.choices[i].help) return false;
  return true;
}
}

const FilterLegacy::RouteNames& filter_editor_route_names (FilterProcedure route)
{
  switch (route) {
    case FilterProcedure::blinds: return FilterLegacy::blinds;
    case FilterProcedure::small_tiles: return FilterLegacy::small_tiles;
    case FilterProcedure::retinex: return FilterLegacy::retinex;
    case FilterProcedure::convolution: return FilterLegacy::convolution;
  }
  incompatible ("This filter has no admitted parameter schema");
}
FilterEditorField filter_editor_field (FilterProcedure route, const char *key)
{
  route_index (route);
  if (!key) incompatible ("The filter editor field has no canonical key");
  FilterEditorField field;
  const auto match = [&] (const char *name, unsigned slot, FilterEditorValueType type) {
    if (std::strcmp (key, name)) return false;
    field.key = name; field.saved_slot = slot; field.type = type; return true;
  };
  using Type = FilterEditorValueType;
  if (match ("run-mode", 0, Type::context) || match ("image", 1, Type::context) || match ("drawables", 2, Type::context))
    { if (field.saved_slot == 2) field.count = 1; return field; }
  const auto integer = [&] (FilterLegacy::Scalar<std::int32_t> range) {
    field.integer_min = range.minimum; field.integer_max = range.maximum; field.integer_initial = range.initial;
  };
  switch (route) {
    case FilterProcedure::blinds:
      if (match ("angle-displacement", 3, Type::integer))
        { integer (FilterLegacy::angle); field.integer_initial = 30; return field; }
      if (match ("num-segments", 4, Type::integer))
        { integer (FilterLegacy::segments); field.integer_initial = 3; return field; }
      if (match ("orientation", 5, Type::choice))
        { field.legacy_flag = true; return field; }
      if (match ("bg-transparent", 6, Type::boolean))
        { field.legacy_flag = true; return field; }
      break;
    case FilterProcedure::small_tiles:
      if (match ("num-tiles", 3, Type::integer)) { integer (FilterLegacy::tiles); return field; }
      break;
    case FilterProcedure::retinex:
      if (match ("scale", 3, Type::integer)) { integer (FilterLegacy::scale); return field; }
      if (match ("nscales", 4, Type::integer)) { integer (FilterLegacy::nscales); return field; }
      if (match ("scales-mode", 5, Type::choice)) { integer (FilterLegacy::scales_mode); return field; }
      if (match ("cvar", 6, Type::real))
        { field.real_min = FilterLegacy::cvar.minimum; field.real_max = FilterLegacy::cvar.maximum;
          field.real_initial = FilterLegacy::cvar.initial; return field; }
      break;
    case FilterProcedure::convolution:
      if (match ("matrix", 4, Type::double_array))
        { field.count = FilterLegacy::matrix_count; field.count_slot = 3; return field; }
      if (match ("alpha-alg", 5, Type::boolean))
        { field.legacy_flag = true; field.integer_initial = FilterLegacy::alpha_alg_default; return field; }
      if (match ("divisor", 6, Type::real)) { field.real_initial = FilterLegacy::divisor_default; return field; }
      if (match ("offset", 7, Type::real)) { field.real_initial = FilterLegacy::offset_default; return field; }
      if (match ("channels", 9, Type::int32_array))
        { field.count = FilterLegacy::channel_count; field.count_slot = 8; field.legacy_flag = true; return field; }
      if (match ("border-mode", 10, Type::choice)) { integer (FilterLegacy::border); return field; }
      break;
  }
  incompatible ("The filter editor field is not part of the admitted legacy mapping");
}
bool filter_editor_procedure_matches (FilterProcedure route, GimpProcedure *procedure) noexcept
{
  try {
    const auto& names = filter_editor_route_names (route);
    return named (procedure, names.execution) || named (procedure, names.public_name);
  } catch (...) { return false; }
}

struct FilterEditorSchema::State {
  FilterProcedure route;
  ObjectRef<GObject> manager, pdb;
  Provider provider;
  std::array<std::vector<FilterEditorParameter>, 2> parameters;
  std::array<gint64, 2> mtimes {};
  gint64 definition_mtime = 0;
  std::size_t bytes = 0;
};
FilterEditorSchema::FilterEditorSchema (Gimp *gimp, FilterProcedure route)
  : state_ (new State {})
{
  try {
    route_index (route);
    state_->route = route;
    state_->provider = registered_provider (gimp, route);
    state_->manager = ObjectRef<GObject>::retain (G_OBJECT (gimp->plug_in_manager));
    state_->pdb = ObjectRef<GObject>::retain (G_OBJECT (gimp->pdb));
    Budget budget;
    budget.add (sizeof (State) + state_->provider.path.size () + 1);
    state_->definition_mtime = state_->provider.definition_mtime;
    for (unsigned i = 0; i < state_->provider.count; ++i)
      {
        auto *procedure = GIMP_PROCEDURE (state_->provider.procedures[i].get ());
        check_procedure (procedure, route, !i, state_->provider.path);
        auto& parameters = state_->parameters[i];
        parameters.reserve (procedure->num_args);
        /* Bound every string/choice before the helper validator walks its
         * borrowed metadata. The validator independently pins names, exact
         * types/defaults/ranges, and the context ABI at slots 0/1/2. */
        for (gint slot = 0; slot < procedure->num_args; ++slot)
          parameters.emplace_back (snapshot_parameter (procedure->args[slot], route, slot, budget));
        FilterParameterSchema signature (procedure, route, !i);
        state_->mtimes[i] = GIMP_PLUG_IN_PROCEDURE (procedure)->mtime;
      }
    state_->bytes = budget.bytes;
  } catch (const FilterEditorSchemaError&) { throw; }
    catch (const std::bad_alloc&) { throw; }
    catch (const std::exception&) { incompatible ("The bundled filter argument metadata is incompatible"); }
}
FilterEditorSchema::~FilterEditorSchema () = default;
const FilterEditorParameter& FilterEditorSchema::field (const char *key) const
{
  if (key) for (const auto& parameter : state_->parameters[0])
    if (parameter.name == key) return parameter;
  incompatible ("The filter editor field is not present in the registered schema");
}
bool FilterEditorSchema::current (Gimp *gimp) const noexcept
{
  try {
    FilterEditorSchema now (gimp, state_->route);
    if (state_->manager.get () != now.state_->manager.get () || state_->pdb.get () != now.state_->pdb.get () ||
        state_->provider.path != now.state_->provider.path || state_->provider.count != now.state_->provider.count ||
        state_->definition_mtime != now.state_->definition_mtime || state_->mtimes != now.state_->mtimes) return false;
    for (unsigned i = 0; i < state_->provider.count; ++i)
      {
        if (state_->provider.procedures[i].get () != now.state_->provider.procedures[i].get () ||
            state_->parameters[i].size () != now.state_->parameters[i].size ()) return false;
        for (std::size_t slot = 0; slot < state_->parameters[i].size (); ++slot)
          if (!same (state_->parameters[i][slot], now.state_->parameters[i][slot])) return false;
      }
    return true;
  } catch (...) { return false; }
}
bool FilterEditorSchema::matches (GimpProcedure *procedure) const noexcept
{ return filter_editor_procedure_matches (state_->route, procedure); }
std::size_t FilterEditorSchema::snapshot_bytes () const noexcept { return state_->bytes; }
}

void gimp_filter_parameter_editor_initialize (GimpPlugInManager *manager)
{
  using namespace GimpPainter;
  try {
    if (!manager || BindingStore::find (G_OBJECT (manager))) return;
    auto& store = BindingStore::ensure (G_OBJECT (manager));
    store.emplace<ProvidersSlot> ();
  } catch (...) { /* C boundary: absence of the slot disables schema editing. */ }
}
void gimp_filter_parameter_editor_activate (GimpPlugInManager *manager)
{
  try { GimpPainter::BindingStore::require (G_OBJECT (manager)).activate (); }
  catch (...) { /* C construction boundary: an inactive store is unavailable. */ }
}
void gimp_filter_parameter_editor_close (GimpPlugInManager *manager)
{
  try {
    if (auto *store = GimpPainter::BindingStore::find (G_OBJECT (manager))) store->close ();
  } catch (...) { /* C disposal boundary; finalization also closes the store. */ }
}
void gimp_filter_parameter_editor_capture (GimpPlugInManager *manager)
{
  using namespace GimpPainter;
  try {
    if (!manager || !GIMP_IS_PLUG_IN_MANAGER (manager)) return;
    BindingStore::require (G_OBJECT (manager)).with<ProvidersSlot> ([&] (Providers& providers) {
      if (providers.captured) return;
      providers.captured = true;
      for (unsigned i = 0; i < route_count; ++i)
        try { providers.routes[i] = capture_provider (manager, static_cast<FilterProcedure> (i + 1)); }
        catch (const FilterEditorSchemaError& error) {
          providers.routes[i].failure_status = error.status ();
          providers.routes[i].failure_reason = error.what ();
        }
        catch (...) { /* This route remains unavailable; startup is unaffected. */ }
    });
  } catch (...) { /* C boundary: missing provenance is a preserve-only editor. */ }
}
