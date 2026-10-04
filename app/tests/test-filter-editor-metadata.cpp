/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Registered-only metadata, budget, provenance and lifetime boundaries. */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpdrawable.h"
#include "core/gimpparamspecs.h"
#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "plug-in/plug-in-types.h"
#include "plug-in/gimpplugindef.h"
#include "plug-in/gimppluginmanager.h"
#include "plug-in/gimppluginprocedure.h"
#include "core/gimpfilterparametereditor.h"
void gimp_test_filter_editor_metadata (Gimp *application);
void gimp_test_filter_editor_manager_lifetime (Gimp *application);
}
#include "core/gimpfilterparametereditor.hpp"
#include "core/gimpfilterpaths.hpp"
#include "painter/binding-store.hpp"
#include <algorithm>
#include <cstring>

using namespace GimpPainter;
namespace {
using Object = ObjectRef<GObject>;
/* Budget fixtures deliberately pass dynamically allocated nick/blurb text. */
constexpr auto flags = static_cast<GParamFlags> (G_PARAM_READWRITE);
constexpr auto blinds = FilterProcedure::blinds;

/* Only registry pointers are temporarily substituted. No main-loop dispatch,
 * query, execute, or mutation of the real manager's attestation occurs. */
struct Registry {
  explicit Registry (Gimp *application)
    : app (application), original_manager (app->plug_in_manager), original_pdb (app->pdb),
      manager (Object::adopt (G_OBJECT (gimp_plug_in_manager_new (app)))),
      pdb (Object::adopt (G_OBJECT (gimp_pdb_new (app))))
    { app->plug_in_manager = GIMP_PLUG_IN_MANAGER (manager.get ()); app->pdb = GIMP_PDB (pdb.get ()); }
  ~Registry () { app->plug_in_manager = original_manager; app->pdb = original_pdb; }
  Gimp *app;
  GimpPlugInManager *original_manager;
  GimpPDB *original_pdb;
  Object manager, pdb;
  void attach (GimpProcedure *procedure, bool needs_query = false, bool has_init = false)
  {
    auto *definition = gimp_plug_in_def_new (GIMP_PLUG_IN_PROCEDURE (procedure)->file);
    gimp_plug_in_def_set_needs_query (definition, needs_query);
    gimp_plug_in_def_set_has_init (definition, has_init);
    gimp_plug_in_def_add_procedure (definition, GIMP_PLUG_IN_PROCEDURE (procedure));
    app->plug_in_manager->plug_in_defs = g_slist_prepend (nullptr, definition);
    gimp_plug_in_manager_add_procedure (app->plug_in_manager, GIMP_PLUG_IN_PROCEDURE (procedure));
    gimp_filter_parameter_editor_capture (app->plug_in_manager);
    g_slist_free_full (app->plug_in_manager->plug_in_defs, g_object_unref);
    app->plug_in_manager->plug_in_defs = nullptr;
    gimp_pdb_register_procedure (app->pdb, procedure);
  }
  void remove (GimpProcedure *procedure)
  {
    gimp_pdb_unregister_procedure (app->pdb, procedure);
    app->plug_in_manager->plug_in_procedures = g_slist_remove (app->plug_in_manager->plug_in_procedures, procedure);
    g_object_unref (procedure);
  }
};

Object procedure (const char *path = nullptr, std::size_t text_size = 0, unsigned extra_choices = 0)
{
  const auto trusted = filter_registered_plugin_path (blinds);
  auto file = Object::adopt (G_OBJECT (g_file_new_for_path (path ? path : trusted.c_str ())));
  auto result = Object::adopt (G_OBJECT (gimp_plug_in_procedure_new (GIMP_PDB_PROC_TYPE_PLUGIN, G_FILE (file.get ()) )));
  auto *p = GIMP_PROCEDURE (result.get ());
  gimp_object_set_static_name (GIMP_OBJECT (p), FilterLegacy::blinds.execution);
  std::string text (text_size, 'x');
  const auto label = [&] (const char *name) { return text_size ? text.c_str () : name; };
  gimp_procedure_add_argument (p, g_param_spec_enum ("run-mode", label ("run-mode"), label ("run-mode"),
    GIMP_TYPE_RUN_MODE, GIMP_RUN_NONINTERACTIVE, flags));
  gimp_procedure_add_argument (p, gimp_param_spec_image ("image", label ("image"), label ("image"), FALSE, flags));
  gimp_procedure_add_argument (p, gimp_param_spec_core_object_array ("drawables", label ("drawables"), label ("drawables"),
    GIMP_TYPE_DRAWABLE, flags));
  gimp_procedure_add_argument (p, g_param_spec_int ("angle-displacement", label ("angle"), label ("angle"), 0, 90, 30, flags));
  gimp_procedure_add_argument (p, g_param_spec_int ("num-segments", label ("segments"), label ("segments"), 1, 1024, 3, flags));
  auto *choices = gimp_choice_new ();
  gimp_choice_add (choices, "horizontal", GIMP_ORIENTATION_HORIZONTAL, label ("Horizontal"), label ("Horizontal"));
  gimp_choice_add (choices, "vertical", GIMP_ORIENTATION_VERTICAL, label ("Vertical"), label ("Vertical"));
  for (unsigned i = 0; i < extra_choices; ++i)
    {
      const auto nick = "excess-" + std::to_string (i);
      gimp_choice_add (choices, nick.c_str (), i + 100, nick.c_str (), nullptr);
    }
  gimp_procedure_add_argument (p, gimp_param_spec_choice ("orientation", label ("orientation"), label ("orientation"),
    choices, "horizontal", flags));
  gimp_procedure_add_argument (p, g_param_spec_boolean ("bg-transparent", label ("transparent"), label ("transparent"), FALSE, flags));
  return result;
}

void rejected (Gimp *app, FilterEditorSchemaStatus expected, const char *reason = nullptr)
{
  bool caught = false;
  try { FilterEditorSchema schema (app, blinds); }
  catch (const FilterEditorSchemaError& error) {
    caught = true;
    g_assert_true (error.status () == expected);
    if (reason) g_assert_nonnull (std::strstr (error.what (), reason));
  }
  g_assert_true (caught);
}
void finalized (gpointer data, GObject *) { *static_cast<bool *> (data) = true; }
struct Reentry { Gimp *app; GimpPlugInManager *manager; bool entered = false; };
void close_again (gpointer data, GObject *)
{
  auto& state = *static_cast<Reentry *> (data);
  state.entered = true;
  gimp_filter_parameter_editor_close (state.manager);
  rejected (state.app, FilterEditorSchemaStatus::unavailable);
}
}

void gimp_test_filter_editor_metadata (Gimp *application)
{
  std::size_t maximum = 0;
  for (unsigned i = 1; i <= 4; ++i)
    {
      const auto route = static_cast<FilterProcedure> (i);
      FilterEditorSchema schema (application, route);
      g_assert_true (schema.current (application));
      g_assert_cmpuint (schema.snapshot_bytes (), <=, FilterEditorSchema::max_bytes);
      maximum = std::max (maximum, schema.snapshot_bytes ());
      const auto& context = schema.field ("drawables");
      g_assert_cmpuint (context.runtime_slot, ==, 2);
      g_assert_cmpuint (context.legacy.saved_slot, ==, 2);
      g_assert_cmpuint (context.legacy.count, ==, 1);
      g_assert_true (context.default_is_null);
      g_test_message ("Registered route %u descriptor bytes: %zu", i, schema.snapshot_bytes ());
    }
  g_test_message ("Largest admitted metadata descriptor: %zu bytes (budget %zu)", maximum, FilterEditorSchema::max_bytes);
  g_assert_cmpuint (filter_editor_field (FilterProcedure::convolution, "matrix").saved_slot, ==, 4);
  g_assert_cmpuint (filter_editor_field (FilterProcedure::convolution, "matrix").count, ==, 25);
  g_assert_cmpuint (filter_editor_field (FilterProcedure::convolution, "channels").saved_slot, ==, 9);
  g_assert_cmpuint (filter_editor_field (FilterProcedure::convolution, "channels").count, ==, 5);
  g_assert_cmpint (filter_editor_field (blinds, "num-segments").integer_max, ==, 100);
  {
    Registry fixture (application);
    rejected (application, FilterEditorSchemaStatus::unavailable);
    auto owner = procedure ();
    auto *p = GIMP_PROCEDURE (owner.get ());
    fixture.attach (p, true); // completed restore's historical needs_query
    FilterEditorSchema schema (application, blinds);
    g_assert_true (schema.current (application));
    auto *angle = G_PARAM_SPEC_INT (p->args[3]);
    ++angle->maximum;
    g_assert_false (schema.current (application));
    --angle->maximum;
    g_assert_true (schema.current (application));
    auto *orientation = G_PARAM_SPEC_STRING (p->args[5]);
    gchar *old_default = orientation->default_value;
    orientation->default_value = g_strdup ("vertical");
    g_assert_cmpstr (gimp_param_spec_choice_get_default (p->args[5]), ==, "horizontal");
    g_assert_false (schema.current (application));
    g_free (orientation->default_value);
    orientation->default_value = old_default;
    g_assert_true (schema.current (application));
    std::swap (p->args[3], p->args[4]);
    g_assert_false (schema.current (application));
    FilterEditorSchema reordered (application, blinds);
    g_assert_cmpuint (reordered.field ("angle-displacement").runtime_slot, ==, 4);
    g_assert_cmpuint (reordered.field ("angle-displacement").legacy.saved_slot, ==, 3);
    std::swap (p->args[3], p->args[4]);
    auto duplicate = procedure ();
    gimp_pdb_register_procedure (application->pdb, GIMP_PROCEDURE (duplicate.get ()));
    rejected (application, FilterEditorSchemaStatus::incompatible);
    gimp_pdb_unregister_procedure (application->pdb, GIMP_PROCEDURE (duplicate.get ()));
    g_assert_true (schema.current (application));
    auto wrong_path = Object::adopt (G_OBJECT (g_file_new_for_path ("/tmp/untrusted-filter-provider")));
    auto *plugin = GIMP_PLUG_IN_PROCEDURE (p);
    GFile *old_file = plugin->file;
    plugin->file = G_FILE (wrong_path.get ());
    g_assert_false (schema.current (application));
    plugin->file = old_file;
    g_assert_true (schema.current (application));
  }
  for (unsigned scenario = 0; scenario < 6; ++scenario)
    {
      Registry fixture (application);
      auto owner = procedure (scenario == 0 ? "/tmp/untrusted-filter-provider" : nullptr,
                               scenario == 2 ? 4097 : scenario == 3 ? 4096 : 0,
                               scenario == 4 ? 513 : 0);
      if (scenario == 5)
        {
          auto *spec = G_PARAM_SPEC_STRING (GIMP_PROCEDURE (owner.get ())->args[5]);
          g_free (spec->default_value);
          spec->default_value = g_strnfill (4097, 'x');
        }
      fixture.attach (GIMP_PROCEDURE (owner.get ()), false, scenario == 1);
      rejected (application, scenario ? FilterEditorSchemaStatus::incompatible : FilterEditorSchemaStatus::unavailable,
                scenario >= 2 ? "limit" : nullptr);
    }
}

void gimp_test_filter_editor_manager_lifetime (Gimp *application)
{
  {
    Registry fixture (application);
    auto *manager = application->plug_in_manager;
    auto *store = BindingStore::find (G_OBJECT (manager));
    g_assert_nonnull (store);
    g_assert_true (store->state () == BindingStore::State::active);
    auto owner = procedure ();
    auto *p = GIMP_PROCEDURE (owner.get ());
    bool procedure_finalized = false, manager_finalized = false;
    g_object_weak_ref (G_OBJECT (p), finalized, &procedure_finalized);
    g_object_weak_ref (G_OBJECT (manager), finalized, &manager_finalized);
    fixture.attach (p);
    std::unique_ptr<FilterEditorSchema> schema (new FilterEditorSchema (application, blinds));
    fixture.remove (p);
    owner.reset ();
    g_assert_false (procedure_finalized);
    g_object_run_dispose (G_OBJECT (manager));
    gimp_filter_parameter_editor_close (manager);
    g_assert_true (store->state () == BindingStore::State::closed);
    g_assert_false (schema->current (application));
    g_assert_false (procedure_finalized);
    schema.reset ();
    g_assert_true (procedure_finalized);
    application->plug_in_manager = fixture.original_manager;
    fixture.manager.reset ();
    g_assert_true (manager_finalized);
  }
  {
    Registry fixture (application);
    auto owner = procedure ();
    auto *p = GIMP_PROCEDURE (owner.get ());
    fixture.attach (p);
    Reentry reentry {application, application->plug_in_manager};
    g_object_weak_ref (G_OBJECT (p), close_again, &reentry);
    fixture.remove (p);
    owner.reset ();
    gimp_filter_parameter_editor_close (application->plug_in_manager);
    g_assert_true (reentry.entered);
    gimp_filter_parameter_editor_close (application->plug_in_manager);
  }
}
