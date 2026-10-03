/* SPDX-License-Identifier: GPL-3.0-or-later
 * Only bundled Blinds is admitted. Every native object belongs to this child
 * main thread; no parent GObject is retained or used by the helper.
 */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <fontconfig/fontconfig.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "core-types.h"
#include "gimp.h"
#include "gimp-contexts.h"
#include "gimp-data-factories.h"
#include "gimpcontext.h"
#include "gimpchannel-select.h"
#include "gimpdrawable.h"
#include "gimpdrawable-shadow.h"
#include "gimpimage.h"
#include "gimpimage-color-profile.h"
#include "gimpimage-undo.h"
#include "gimplayer.h"
#include "gimplayer-new.h"
#include "gimpparamspecs.h"
#include "gimpprogress.h"
#include "config/gimprc.h"
#include "gegl/gimp-gegl.h"
#include "pdb/gimppdb.h"
#include "pdb/gimppdbcontext.h"
#include "pdb/gimpprocedure.h"
#include "pdb/internal-procs.h"
#include "plug-in/plug-in-types.h"
#include "plug-in/gimpplugindef.h"
#include "plug-in/gimpplugin.h"
#include "plug-in/gimppluginmanager.h"
#define __YES_I_NEED_GIMP_PLUG_IN_MANAGER_CALL__
#include "plug-in/gimppluginmanager-call.h"
#undef __YES_I_NEED_GIMP_PLUG_IN_MANAGER_CALL__
#include "plug-in/gimppluginprocedure.h"
}
#include "gimpfilterprocedure.hpp"
#include "gimpfilterpaths.hpp"
#include "gimp-painter-type-traits.hpp"
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/gio-type-traits.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>

/* No embedded C++ implementation or second qdata ownership mechanism. */
struct GimpPainterProcedureProgress { GObject parent_instance; };
struct GimpPainterProcedureProgressClass { GObjectClass parent_class; };
static GType gimp_painter_procedure_progress_get_type ();
static void procedure_progress_iface_init (GimpProgressInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GimpPainterProcedureProgress,
                        gimp_painter_procedure_progress, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE (GIMP_TYPE_PROGRESS,
                                               procedure_progress_iface_init))

namespace GimpPainter {
template<> struct TypeTraits<Gimp>
{ static GType type () noexcept { return GIMP_TYPE_GIMP; } };
template<> struct TypeTraits<GimpContext>
{ static GType type () noexcept { return GIMP_TYPE_CONTEXT; } };
template<> struct TypeTraits<GimpImage>
{ static GType type () noexcept { return GIMP_TYPE_IMAGE; } };
template<> struct TypeTraits<GimpRc>
{ static GType type () noexcept { return GIMP_TYPE_RC; } };
template<> struct TypeTraits<GimpPlugInDef>
{ static GType type () noexcept { return GIMP_TYPE_PLUG_IN_DEF; } };
template<> struct TypeTraits<GimpPlugIn>
{ static GType type () noexcept { return GIMP_TYPE_PLUG_IN; } };
template<> struct TypeTraits<GimpProcedure>
{ static GType type () noexcept { return GIMP_TYPE_PROCEDURE; } };
template<> struct TypeTraits<GeglBuffer>
{ static GType type () noexcept { return GEGL_TYPE_BUFFER; } };
template<> struct TypeTraits<GeglColor>
{ static GType type () noexcept { return GEGL_TYPE_COLOR; } };
template<> struct TypeTraits<GimpPainterProcedureProgress>
{ static GType type () noexcept { return gimp_painter_procedure_progress_get_type (); } };
}

namespace {
using namespace GimpPainter;
constexpr std::size_t transfer_bytes = 128 * 1024;
constexpr const char *blinds_name = "plug-in-blinds";

struct ProgressState
{
  void close () noexcept { active = false; }
  bool active = false;
  double value = 0;
};
struct ProgressSlot : SlotSpec<GimpPainterProcedureProgress, ProgressState> {};

template<class Result, class Function>
Result progress_call (GimpProgress *progress, Result fallback, Function function) noexcept
{
  return boundary (nullptr, fallback, [&] {
    return BindingStore::require (G_OBJECT (progress)).with<ProgressSlot> (function);
  });
}
template<class Function>
void progress_call (GimpProgress *progress, Function function) noexcept
{
  boundary_void (nullptr, [&] {
    BindingStore::require (G_OBJECT (progress)).with<ProgressSlot> (function);
  });
}
GimpProgress *progress_start (GimpProgress *progress, gboolean, const gchar *) noexcept
{
  return progress_call<GimpProgress *> (progress, nullptr, [=] (ProgressState& state) {
    state.active = true;
    state.value = 0;
    return progress;
  });
}
void progress_end (GimpProgress *progress) noexcept
{ progress_call (progress, [] (ProgressState& state) { state.active = false; }); }
gboolean progress_active (GimpProgress *progress) noexcept
{ return progress_call<gboolean> (progress, FALSE, [] (ProgressState& state) { return state.active; }); }
void progress_text (GimpProgress *, const gchar *) noexcept {}
void progress_value (GimpProgress *progress, gdouble value) noexcept
{
  progress_call (progress, [=] (ProgressState& state) {
    if (std::isfinite (value)) state.value = std::max (0.0, std::min (1.0, value));
  });
}
gdouble progress_get_value (GimpProgress *progress) noexcept
{ return progress_call<double> (progress, 0, [] (ProgressState& state) { return state.value; }); }
void progress_pulse (GimpProgress *) noexcept {}
gboolean progress_message (GimpProgress *, Gimp *, GimpMessageSeverity,
                           const gchar *, const gchar *) noexcept
{ return FALSE; }

class ProcedureProgress
{
public:
  ProcedureProgress ()
    : object_ (ObjectRef<GimpPainterProcedureProgress>::adopt (
        static_cast<GimpPainterProcedureProgress *> (g_object_new (
          gimp_painter_procedure_progress_get_type (), nullptr))))
  {
    auto& store = BindingStore::ensure (G_OBJECT (object_.get ()));
    store.emplace<ProgressSlot> ();
    store.activate ();
  }
  GimpProgress *get () const noexcept { return GIMP_PROGRESS (object_.get ()); }
private:
  ObjectRef<GimpPainterProcedureProgress> object_;
};

struct ValuesFree
{ void operator() (GimpValueArray *value) const noexcept { if (value) gimp_value_array_unref (value); } };
using ValuesRef = std::unique_ptr<GimpValueArray, ValuesFree>;
struct ErrorFree
{ void operator() (GError *error) const noexcept { if (error) g_error_free (error); } };
using ErrorRef = std::unique_ptr<GError, ErrorFree>;
struct StringFree
{ void operator() (gchar *text) const noexcept { g_free (text); } };
using StringRef = std::unique_ptr<gchar, StringFree>;

class PluginLifetimes
{
public:
  void observe (GimpPlugInManager *manager)
  {
    connection_ = Connection::connect (
      ObjectRef<GObject>::retain (G_OBJECT (manager)), "plug-in-opened",
      G_CALLBACK (opened), this, nullptr);
  }
  void wait () noexcept
  {
    /* GIMP closes the wire before its low-priority child watch has reaped the
     * native process. The watch holds the final plug-in ref and its context.
     * The helper must service it before declaring success or destroying Gimp.
     * A stuck native child is bounded by the parent's process-group watchdog. */
    for (;;)
      {
        bool pending = false;
        for (auto& plugin : plugins_) if (plugin.lock ()) pending = true;
        if (!pending) return;
        g_main_context_iteration (nullptr, TRUE);
      }
  }
  void check () const
  {
    if (failed_) throw std::runtime_error ("Cannot track private Blinds child lifetime");
  }
private:
  static void opened (GimpPlugInManager *, GimpPlugIn *plugin, gpointer data) noexcept
  {
    auto& self = *static_cast<PluginLifetimes *> (data);
    try
      {
        if (self.count_ == self.plugins_.size ())
          { self.failed_ = true; return; }
        self.plugins_[self.count_++] = WeakRef<GimpPlugIn> (
          ObjectRef<GimpPlugIn>::retain (plugin));
      }
    catch (...) { self.failed_ = true; }
  }
  std::array<WeakRef<GimpPlugIn>, 2> plugins_;
  Connection connection_;
  std::size_t count_ = 0;
  bool failed_ = false;
};

gchar *private_display_name (Gimp *, gint, GObject **monitor, gint *number) noexcept
{
  /* The normal no-GUI fallback leaves monitor_number unset, although the
   * native GPConfig serializes it. Supply deterministic headless values. */
  *monitor = nullptr;
  *number = 0;
  return nullptr;
}

/* No rc deserialization, general plug-in search, interpreter definitions,
 * environment files, fonts, modules, saved contexts, or resource-file loaders.
 * The helper main redirects GIMP3 directories before this code, so gimp_new's
 * extension manager sees only the empty private profile and data directory. */
class PrivateRuntime
{
public:
  ~PrivateRuntime () noexcept
  {
    if (gimp_)
      {
        gimp_plug_in_manager_exit (gimp_.get ()->plug_in_manager);
        plugins_.wait ();
        if (gegl_ready_) gimp_gegl_exit (gimp_.get ());
        gimp_contexts_exit (gimp_.get ());
        gimp_.reset ();
      }
    if (gegl_started_) gegl_exit ();
  }
  void initialize ()
  {
    if (!g_get_prgname ()) g_set_prgname ("gimp-painter-filter-worker");
    /* Even without loading fonts, GimpFontFactory's destructor obtains the
     * current Fontconfig configuration. Install an empty private one before
     * any library initialization, so this cannot scan user/system font files
     * or configuration merely while tearing down a filter process. */
    FcConfig *fonts = FcConfigCreate ();
    if (!fonts) throw std::runtime_error ("Cannot create private empty font configuration");
    const bool fonts_installed = FcConfigSetCurrent (fonts);
    FcConfigDestroy (fonts);
    if (!fonts_installed) throw std::runtime_error ("Cannot install private empty font configuration");
    gegl_init (nullptr, nullptr);
    gegl_started_ = true;
    gimp_ = ObjectRef<Gimp>::adopt (gimp_new (
      "GIMP Painter private filter", nullptr, nullptr,
      FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE,
      GIMP_STACK_TRACE_NEVER, GIMP_PDB_COMPAT_OFF));
    if (!gimp_) throw std::runtime_error ("Cannot create private GIMP runtime");
    gimp_.get ()->gui.get_display_name = private_display_name;
    auto config = ObjectRef<GimpRc>::adopt (static_cast<GimpRc *> (
      g_object_new (GIMP_TYPE_RC, "gimp", gimp_.get (), nullptr)));
    auto edit_config = ObjectRef<GimpRc>::adopt (static_cast<GimpRc *> (
      g_object_new (GIMP_TYPE_RC, "gimp", gimp_.get (), nullptr)));
    const char *directory = gimp_directory ();
    if (!directory || !g_path_is_absolute (directory))
      throw std::runtime_error ("Private Blinds requires an absolute private profile");
    StringRef config_directory (g_filename_to_utf8 (directory, -1, nullptr, nullptr, nullptr));
    if (!config_directory)
      throw std::runtime_error ("Cannot encode private Blinds temporary directory");
    g_object_set (config.get (), "use-opencl", FALSE, "num-processors", 1,
                  "tile-cache-size", guint64 (32 * 1024 * 1024),
                  "temp-path", config_directory.get (),
                  "swap-path", config_directory.get (), nullptr);
    g_object_set (edit_config.get (), "temp-path", config_directory.get (),
                  "swap-path", config_directory.get (), nullptr);
    const auto *settings = GIMP_GEGL_CONFIG (config.get ());
    auto expected_directory = ObjectRef<GFile>::adopt (g_file_new_for_path (directory));
    auto temp_directory = ObjectRef<GFile>::adopt (
      gimp_file_new_for_config_path (settings->temp_path, nullptr));
    auto swap_directory = ObjectRef<GFile>::adopt (
      gimp_file_new_for_config_path (settings->swap_path, nullptr));
    if (!temp_directory || !swap_directory ||
        !g_file_equal (expected_directory.get (), temp_directory.get ()) ||
        !g_file_equal (expected_directory.get (), swap_directory.get ()) ||
        settings->tile_cache_size != 32 * 1024 * 1024 ||
        settings->num_processors != 1 || settings->use_opencl)
      throw std::runtime_error ("Private Blinds resource configuration did not retain its limits");
    gimp_.get ()->config = GIMP_CORE_CONFIG (config.release ());
    gimp_.get ()->edit_config = GIMP_CORE_CONFIG (edit_config.release ());
    gimp_gegl_init (gimp_.get ());
    gegl_ready_ = true;
    gchar *actual_swap = nullptr;
    guint64 actual_cache = 0;
    g_object_get (gegl_config (), "swap", &actual_swap,
                  "tile-cache-size", &actual_cache, nullptr);
    StringRef swap_owner (actual_swap);
    if (!actual_swap || g_strcmp0 (actual_swap, directory) || actual_cache != settings->tile_cache_size)
      throw std::runtime_error ("Private Blinds GEGL resource limits differ from native plug-in configuration");
    gimp_data_factories_add_builtin (gimp_.get ());
    internal_procs_init (gimp_.get ()->pdb);
    plugins_.observe (gimp_.get ()->plug_in_manager);
  }
  Gimp *get () const noexcept { return gimp_.get (); }
  void wait_for_plugins () { plugins_.wait (); plugins_.check (); }
private:
  ObjectRef<Gimp> gimp_;
  PluginLifetimes plugins_;
  bool gegl_started_ = false;
  bool gegl_ready_ = false;
};

void validate_blinds (GimpProcedure *procedure, GFile *file)
{
  if (!procedure || !GIMP_IS_PLUG_IN_PROCEDURE (procedure) ||
      procedure->proc_type != GIMP_PDB_PROC_TYPE_PLUGIN ||
      g_strcmp0 (gimp_object_get_name (procedure), blinds_name) ||
      procedure->num_args != 7 || procedure->num_values != 0 || !procedure->args)
    throw std::runtime_error ("Bundled Blinds has an incompatible procedure signature");
  auto *plugin = GIMP_PLUG_IN_PROCEDURE (procedure);
  if (!plugin->file || !g_file_equal (plugin->file, file) ||
      plugin->file_proc || plugin->batch_interpreter || plugin->installed_during_init)
    throw std::runtime_error ("Bundled Blinds has an incompatible executable binding");
  constexpr const char *names[] = { "run-mode", "image", "drawables",
    "angle-displacement", "num-segments", "orientation", "bg-transparent" };
  const GType types[] = { GIMP_TYPE_RUN_MODE, GIMP_TYPE_IMAGE,
    GIMP_TYPE_CORE_OBJECT_ARRAY, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_BOOLEAN };
  for (unsigned i = 0; i < 7; ++i)
    if (!procedure->args[i] || g_strcmp0 (g_param_spec_get_name (procedure->args[i]), names[i]) ||
        G_PARAM_SPEC_VALUE_TYPE (procedure->args[i]) != types[i] ||
        (procedure->args[i]->flags & G_PARAM_READWRITE) != G_PARAM_READWRITE)
      throw std::runtime_error ("Bundled Blinds argument name or type changed");
  GParamSpec **args = procedure->args;
  if (!G_IS_PARAM_SPEC_ENUM (args[0]) ||
      G_PARAM_SPEC_ENUM (args[0])->default_value != GIMP_RUN_NONINTERACTIVE ||
      !GIMP_IS_PARAM_SPEC_IMAGE (args[1]) || gimp_param_spec_image_none_allowed (args[1]) ||
      !GIMP_IS_PARAM_SPEC_CORE_OBJECT_ARRAY (args[2]) ||
      gimp_param_spec_core_object_array_get_object_type (args[2]) != GIMP_TYPE_DRAWABLE ||
      !G_IS_PARAM_SPEC_INT (args[3]) || !G_IS_PARAM_SPEC_INT (args[4]) ||
      G_PARAM_SPEC_INT (args[3])->minimum != 0 || G_PARAM_SPEC_INT (args[3])->maximum != 90 ||
      G_PARAM_SPEC_INT (args[3])->default_value != 30 ||
      G_PARAM_SPEC_INT (args[4])->minimum != 1 || G_PARAM_SPEC_INT (args[4])->maximum != 1024 ||
      G_PARAM_SPEC_INT (args[4])->default_value != 3 ||
      !GIMP_IS_PARAM_SPEC_CHOICE (args[5]) || !G_IS_PARAM_SPEC_BOOLEAN (args[6]) ||
      G_PARAM_SPEC_BOOLEAN (args[6])->default_value)
    throw std::runtime_error ("Bundled Blinds argument constraints changed");
  GimpChoice *choice = gimp_param_spec_choice_get_choice (args[5]);
  if (!choice || g_list_length (gimp_choice_list_nicks (choice)) != 2 ||
      !gimp_choice_is_valid (choice, "horizontal") || !gimp_choice_is_valid (choice, "vertical") ||
      gimp_choice_get_id (choice, "horizontal") != GIMP_ORIENTATION_HORIZONTAL ||
      gimp_choice_get_id (choice, "vertical") != GIMP_ORIENTATION_VERTICAL ||
      g_strcmp0 (gimp_param_spec_choice_get_default (args[5]), "horizontal"))
    throw std::runtime_error ("Bundled Blinds orientation choices changed");
}

ObjectRef<GimpProcedure> query_blinds (Gimp *gimp, GimpContext *context)
{
  /* Resolve only this bundled route from the private helper's own location.
   * Saved definitions, the environment and plug-in registries cannot select it. */
  const auto path = filter_plugin_path (FilterProcedure::blinds);
  auto file = ObjectRef<GFile>::adopt (g_file_new_for_path (path.c_str ()));
  auto definition = ObjectRef<GimpPlugInDef>::adopt (gimp_plug_in_def_new (file.get ()));
  /* Only this literal executable is queried. No general restore/search/init
   * call is made. The parent process terminates a hung query or execution. */
  gimp_plug_in_manager_call_query (gimp->plug_in_manager, context, definition.get ());
  if (definition.get ()->has_init || !definition.get ()->procedures ||
      definition.get ()->procedures->next)
    throw std::runtime_error ("Bundled Blinds query did not return its single fixed procedure");
  auto procedure = ObjectRef<GimpProcedure>::retain (
    GIMP_PROCEDURE (definition.get ()->procedures->data));
  validate_blinds (procedure.get (), file.get ());
  gimp_plug_in_manager_add_procedure (gimp->plug_in_manager,
                                     GIMP_PLUG_IN_PROCEDURE (procedure.get ()));
  gimp_pdb_register_procedure (gimp->pdb, procedure.get ());
  if (gimp_pdb_lookup_procedure (gimp->pdb, blinds_name) != procedure.get ())
    throw std::runtime_error ("Cannot register bundled Blinds in private PDB");
  return procedure;
}

constexpr const char *merge_shadow_name = "gimp-drawable-merge-shadow";

void validate_merge_shadow (GimpProcedure *procedure)
{
  if (!procedure || G_OBJECT_TYPE (procedure) != GIMP_TYPE_PROCEDURE ||
      procedure->proc_type != GIMP_PDB_PROC_TYPE_INTERNAL || !procedure->marshal_func ||
      g_strcmp0 (gimp_object_get_name (procedure), merge_shadow_name) ||
      procedure->num_args != 2 || procedure->num_values != 0 || !procedure->args)
    throw std::runtime_error ("Private merge-shadow procedure signature changed");
  auto **args = procedure->args;
  if (!args[0] || !args[1] ||
      g_strcmp0 (g_param_spec_get_name (args[0]), "drawable") ||
      g_strcmp0 (g_param_spec_get_name (args[1]), "undo") ||
      G_PARAM_SPEC_VALUE_TYPE (args[0]) != GIMP_TYPE_DRAWABLE ||
      G_PARAM_SPEC_VALUE_TYPE (args[1]) != G_TYPE_BOOLEAN ||
      !GIMP_IS_PARAM_SPEC_DRAWABLE (args[0]) || gimp_param_spec_item_none_allowed (args[0]) ||
      !G_IS_PARAM_SPEC_BOOLEAN (args[1]) || G_PARAM_SPEC_BOOLEAN (args[1])->default_value ||
      (args[0]->flags & G_PARAM_READWRITE) != G_PARAM_READWRITE ||
      (args[1]->flags & G_PARAM_READWRITE) != G_PARAM_READWRITE)
    throw std::runtime_error ("Private merge-shadow argument constraints changed");
}

struct ShadowState
{
  ShadowState (ObjectRef<Gimp> owner, ObjectRef<GimpLayer> drawable)
    : owner (std::move (owner)), drawable (std::move (drawable)) {}
  void close () noexcept { shadow.reset (); drawable.reset (); owner.reset (); }
  ObjectRef<Gimp> owner;
  ObjectRef<GimpLayer> drawable;
  ObjectRef<GeglBuffer> shadow;
  bool called = false;
  bool failed = false;
};
struct ShadowSlot : SlotSpec<GimpProcedure, ShadowState> {};

GimpValueArray *capture_shadow (GimpProcedure *procedure, Gimp *gimp,
                               GimpContext *, GimpProgress *,
                               const GimpValueArray *args, GError **error) noexcept
{
  const auto success = boundary<gboolean> (error, FALSE, [&] {
    return BindingStore::require (G_OBJECT (procedure)).with<ShadowSlot> (
      [&] (ShadowState& state) -> gboolean {
        state.failed = true;
        if (state.called || state.owner.get () != gimp || !args ||
            gimp_value_array_length (args) != 2 ||
            !G_VALUE_HOLDS (gimp_value_array_index (args, 0), GIMP_TYPE_DRAWABLE) ||
            !G_VALUE_HOLDS_BOOLEAN (gimp_value_array_index (args, 1)) ||
            g_value_get_object (gimp_value_array_index (args, 0)) != state.drawable.get ())
          throw std::runtime_error ("Private merge-shadow called outside its owned drawable");
        auto *drawable = GIMP_DRAWABLE (state.drawable.get ());
        auto shadow = ObjectRef<GeglBuffer>::retain (gimp_drawable_get_shadow_buffer (drawable));
        if (!shadow || gegl_buffer_get_format (shadow.get ()) != gimp_drawable_get_format (drawable) ||
            gegl_buffer_get_width (shadow.get ()) != gimp_item_get_width (GIMP_ITEM (drawable)) ||
            gegl_buffer_get_height (shadow.get ()) != gimp_item_get_height (GIMP_ITEM (drawable)))
          throw std::runtime_error ("Private merge-shadow buffer layout changed");
        gegl_buffer_flush (shadow.get ());
        /* Plug-in cleanup frees the drawable's shadow reference. This typed
         * lease preserves the unmerged bytes through that cleanup. */
        state.shadow = std::move (shadow);
        state.called = true;
        state.failed = false;
        return TRUE;
      });
  });
  return gimp_procedure_get_return_values (procedure, success, error ? *error : nullptr);
}

/* A request-local PDB chain entry; never alter the registered native marshal.
 * Every path, including exceptions and cancellation, removes only our entry. */
class ShadowCapture
{
public:
  ~ShadowCapture () noexcept
  {
    uninstall ();
    if (override_)
      boundary_void (nullptr, [&] {
        if (auto *store = BindingStore::find (G_OBJECT (override_.get ()))) store->close ();
      });
  }
  void install (Gimp *gimp, GimpLayer *layer)
  {
    owner_ = ObjectRef<Gimp>::retain (gimp);
    original_ = ObjectRef<GimpProcedure>::retain (gimp_pdb_lookup_procedure (gimp->pdb, merge_shadow_name));
    validate_merge_shadow (original_.get ());
    override_ = ObjectRef<GimpProcedure>::adopt (gimp_procedure_create_override (original_.get (), capture_shadow));
    validate_merge_shadow (override_.get ());
    auto& store = BindingStore::ensure (G_OBJECT (override_.get ()));
    store.emplace<ShadowSlot> (owner_, ObjectRef<GimpLayer>::retain (layer));
    store.activate ();
    /* GEGL's sparse buffer starts at zero. Clearing explicitly also guarantees
     * that any unwritten portion outside the hard execution rectangle is zero. */
    auto *shadow = gimp_drawable_get_shadow_buffer (GIMP_DRAWABLE (layer));
    if (!shadow) throw std::runtime_error ("Cannot create private raw shadow");
    gegl_buffer_clear (shadow, nullptr);
    gimp_pdb_register_procedure (gimp->pdb, override_.get ());
    registered_ = true;
    if (gimp_pdb_lookup_procedure (gimp->pdb, merge_shadow_name) != override_.get ())
      throw std::runtime_error ("Cannot install private merge-shadow interception");
  }
  ObjectRef<GeglBuffer> finish ()
  {
    if (!override_) return {};
    auto result = BindingStore::require (G_OBJECT (override_.get ())).with<ShadowSlot> (
      [] (ShadowState& state) {
        if (state.failed || !state.called || !state.shadow)
          throw std::runtime_error ("Private merge-shadow interception did not finish");
        return state.shadow;
      });
    uninstall ();
    if (gimp_pdb_lookup_procedure (owner_.get ()->pdb, merge_shadow_name) != original_.get ())
      throw std::runtime_error ("Private merge-shadow registration chain was not restored");
    return result;
  }
private:
  void uninstall () noexcept
  {
    if (registered_)
      {
        gimp_pdb_unregister_procedure (owner_.get ()->pdb, override_.get ());
        registered_ = false;
      }
  }
  ObjectRef<Gimp> owner_;
  ObjectRef<GimpProcedure> original_, override_;
  bool registered_ = false;
};

/* A row wider than the transfer bound is split horizontally. The full raster
 * size and the number of rows never change the scratch allocation. */
template<class Function>
bool each_chunk (const FilterProcedureRequest& request, std::atomic<bool>& cancel,
                 Function function)
{
  for (std::uint32_t y = 0; y < request.height; ++y)
    for (std::uint32_t x = 0; x < request.width;)
      {
        if (cancel.load (std::memory_order_relaxed)) return false;
        const auto width = std::min<std::uint32_t> (request.width - x, transfer_bytes / 4);
        const GeglRectangle rect { int (x), int (y), int (width), 1 };
        const std::uint64_t offset = (std::uint64_t (y) * request.width + x) * 4;
        function (rect, offset, std::size_t (width) * 4);
        x += width;
      }
  return !cancel.load (std::memory_order_relaxed);
}
} // namespace

static void
gimp_painter_procedure_progress_dispose (GObject *object) noexcept
{
  GimpPainter::boundary_void (nullptr, [&] {
    if (auto *store = GimpPainter::BindingStore::find (object)) store->close ();
  });
  G_OBJECT_CLASS (gimp_painter_procedure_progress_parent_class)->dispose (object);
}
static void
gimp_painter_procedure_progress_class_init (GimpPainterProcedureProgressClass *klass)
{ G_OBJECT_CLASS (klass)->dispose = gimp_painter_procedure_progress_dispose; }
static void
gimp_painter_procedure_progress_init (GimpPainterProcedureProgress *) {}
static void
procedure_progress_iface_init (GimpProgressInterface *iface)
{
  iface->start = progress_start;
  iface->end = progress_end;
  iface->is_active = progress_active;
  iface->set_text = progress_text;
  iface->set_value = progress_value;
  iface->get_value = progress_get_value;
  iface->pulse = progress_pulse;
  iface->message = progress_message;
}

namespace GimpPainter {
FilterProcedureDisposition
run_filter_procedure (const FilterProcedureRequest& request,
                      FilterRaster& input, FilterRaster& output,
                      std::atomic<bool>& cancel)
{
  using Disposition = FilterProcedureDisposition;
  const auto bytes = request.bytes ();
  const auto region = request.execution_region ();
  if (&input == &output || input.size () != bytes || output.size () != bytes)
    throw std::invalid_argument ("Private Blinds requires separate exact-sized RGBA8 rasters");
  if (cancel.load (std::memory_order_relaxed)) return Disposition::pending;
  std::array<std::uint8_t, transfer_bytes> pixels;
  const auto validate_carrier = [&] (std::size_t count) {
    if (request.gray)
      for (std::size_t p = 0; p < count; p += 4)
        if (pixels[p] != pixels[p + 1] || pixels[p] != pixels[p + 2])
          throw std::invalid_argument ("Private Blinds Gray input is not triplicated");
  };
  if (!region.intersects)
    {
      /* An empty native mask is unrestricted, so it cannot encode a globally
       * nonempty selection that misses this drawable. Blinds performs no merge
       * in that case. Send an exact carrier and an explicit no-merge outcome. */
      if (!each_chunk (request, cancel, [&] (const GeglRectangle&,
                                            std::uint64_t offset, std::size_t count) {
            input.read (offset, count, pixels.data ());
            validate_carrier (count);
            output.write (offset, count, pixels.data ());
          })) return Disposition::pending;
      output.flush ();
      return cancel.load (std::memory_order_relaxed) ? Disposition::pending : Disposition::no_merge;
    }
  PrivateRuntime runtime;
  runtime.initialize ();
  Gimp *gimp = runtime.get ();
  auto context = ObjectRef<GimpContext>::adopt (
    gimp_pdb_context_new (gimp, gimp_get_user_context (gimp), TRUE));
  auto procedure = query_blinds (gimp, context.get ());
  runtime.wait_for_plugins ();
  if (cancel.load (std::memory_order_relaxed)) return Disposition::pending;

  /* Native Blinds always requests RGB pixels. Triplicated Gray samples in an
   * RGB surrogate avoid both luminance conversion and an ICC round trip. */
  auto image = ObjectRef<GimpImage>::adopt (gimp_image_new (
    gimp, int (request.width), int (request.height), GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR));
  if (!image) throw std::runtime_error ("Cannot create private Blinds image");
  gimp_image_undo_disable (image.get ());
  gimp_image_set_use_srgb_profile (image.get (), TRUE);
  const Babl *format = babl_format ("R'G'B'A u8");
  auto layer = ObjectRef<GimpLayer>::sink (gimp_layer_new (
    image.get (), int (request.width), int (request.height), format,
    "Private Blinds input", 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY));
  if (!layer || !gimp_image_add_layer (image.get (), layer.get (), nullptr, 0, FALSE))
    throw std::runtime_error ("Cannot attach private Blinds drawable");
  auto buffer = ObjectRef<GeglBuffer>::retain (
    gimp_drawable_get_buffer (GIMP_DRAWABLE (layer.get ())));
  if (!buffer || gegl_buffer_get_format (buffer.get ()) != format)
    throw std::runtime_error ("Private Blinds drawable is not exact encoded RGBA8 sRGB");
  if (region.selected)
    gimp_channel_select_rectangle (gimp_image_get_mask (image.get ()),
                                   region.x1, region.y1, region.x2 - region.x1, region.y2 - region.y1,
                                   GIMP_CHANNEL_OP_REPLACE, FALSE, 0, 0, FALSE);

  if (!each_chunk (request, cancel, [&] (const GeglRectangle& rect,
                                        std::uint64_t offset, std::size_t count) {
        input.read (offset, count, pixels.data ());
        validate_carrier (count);
        gegl_buffer_set (buffer.get (), &rect, 0, format, pixels.data (), GEGL_AUTO_ROWSTRIDE);
      })) return Disposition::pending;
  gegl_buffer_flush (buffer.get ());

  auto background = ObjectRef<GeglColor>::adopt (gegl_color_new (nullptr));
  auto background_pixel = request.background;
  background_pixel[3] = 255; // legacy GimpContext backgrounds are opaque
  if (request.gray && (background_pixel[0] != background_pixel[1] ||
                       background_pixel[0] != background_pixel[2]))
    throw std::invalid_argument ("Private Blinds Gray background is not triplicated");
  gegl_color_set_pixel (background.get (), format, background_pixel.data ());
  gimp_context_set_background (context.get (), background.get ());

  ValuesRef arguments (gimp_procedure_get_arguments (procedure.get ()));
  if (!arguments || gimp_value_array_length (arguments.get ()) != 7)
    throw std::runtime_error ("Cannot construct private Blinds arguments");
  GObject *drawables[] = { G_OBJECT (layer.get ()), nullptr };
  g_value_set_enum (gimp_value_array_index (arguments.get (), 0), GIMP_RUN_NONINTERACTIVE);
  g_value_set_object (gimp_value_array_index (arguments.get (), 1), image.get ());
  g_value_set_boxed (gimp_value_array_index (arguments.get (), 2), drawables);
  g_value_set_int (gimp_value_array_index (arguments.get (), 3), request.angle);
  g_value_set_int (gimp_value_array_index (arguments.get (), 4), request.segments);
  g_value_set_string (gimp_value_array_index (arguments.get (), 5),
                      request.orientation == 1 ? "vertical" : "horizontal");
  g_value_set_boolean (gimp_value_array_index (arguments.get (), 6), request.transparent != 0);
  ProcedureProgress progress;
  ShadowCapture capture;
  if (request.raw_shadow) capture.install (gimp, layer.get ());
  GError *error = nullptr;
  ValuesRef result (gimp_procedure_execute (procedure.get (), gimp, context.get (),
                                           progress.get (), arguments.get (), &error));
  ErrorRef error_owner (error);
  runtime.wait_for_plugins ();
  if (cancel.load (std::memory_order_relaxed)) return Disposition::pending;
  if (error || !result || gimp_value_array_length (result.get ()) != 1 ||
      !G_VALUE_HOLDS (gimp_value_array_index (result.get (), 0), GIMP_TYPE_PDB_STATUS_TYPE) ||
      g_value_get_enum (gimp_value_array_index (result.get (), 0)) != GIMP_PDB_SUCCESS)
    throw std::runtime_error (error ? error->message : "Bundled Blinds did not finish successfully");

  Disposition disposition = Disposition::merged;
  if (request.raw_shadow)
    {
      buffer = capture.finish ();
      disposition = Disposition::shadow;
    }
  else
    {
      /* Native merge-shadow can replace the buffer. Retain the standalone
       * route's accepted alpha-zero repair only for already-merged output. */
      buffer = ObjectRef<GeglBuffer>::retain (
        gimp_drawable_get_buffer (GIMP_DRAWABLE (layer.get ())));
      if (!buffer) throw std::runtime_error ("Private merged Blinds drawable has no buffer");
    }
  std::array<std::uint8_t, transfer_bytes> original;
  if (!each_chunk (request, cancel, [&] (const GeglRectangle& rect,
                                        std::uint64_t offset, std::size_t count) {
        gegl_buffer_get (buffer.get (), &rect, 1.0, format, pixels.data (),
                         GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
        if (!request.raw_shadow)
          {
            input.read (offset, count, original.data ());
            for (std::size_t p = 0; p < count; p += 4)
              if (pixels[p + 3] == 0) std::memcpy (pixels.data () + p, original.data () + p, 3);
          }
        output.write (offset, count, pixels.data ());
      })) return Disposition::pending;
  output.flush ();
  return cancel.load (std::memory_order_relaxed) ? Disposition::pending : disposition;
}
} // namespace GimpPainter
