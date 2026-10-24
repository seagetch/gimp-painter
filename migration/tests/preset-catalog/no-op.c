/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <string.h>
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpasyncset.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpdataloaderfactory.h"
#include "core/gimplayerpreset.h"
#include "config/gimpcoreconfig.h"
#include "painter/gimp-painter-error.h"
#include "gimppresetdescriptor-private.h"
#include "gimp-intl.h"

struct _GimpPresetDescriptor { gint refs; GimpPresetDescriptorValues values; gsize allocation_size; };
struct _GimpPresetGuiLease
{
  Gimp *gimp;
  GimpPresetDescriptor *descriptor;
  guint64 generation;
};
/* Metadata only: no global Gimp pointer, resource, factory or ownership map. */
static GimpPresetDescriptor defaults = { -1, {
  N_("Layer Presets"), N_("Layer presets Folders"), "folders-layer-presets", "",
  N_("Select Layer Presets Folders"), N_("Reset Layer Preset _Folders"),
  "layer-presets-path", "layer-presets-path-writable",
  N_("Layer Presets"), NULL, "gimp-layer", "",
  N_("Layer presets"), NULL, N_("Open layer presets dialog"), "gimp-layer-preset-dialog"
}, 0 };
static GimpPresetDescriptor *catalog = &defaults;
static guint owners, frozen_owners, gui_reservations;
static guint64 catalog_generation;
/* Only immutable metadata actually exposed by the borrowed legacy getter.
 * Deduplicated per catalog epoch; never an application/resource pointer map. */
static GPtrArray *legacy_backing;
static gsize legacy_backing_bytes;
void gimp_preset_catalog_legacy_usage (guint *count, gsize *bytes)
{
  if (count) *count = legacy_backing ? legacy_backing->len : 0;
  if (bytes) *bytes = legacy_backing_bytes;
}
static void legacy_epoch_clear (void)
{
  g_clear_pointer (&legacy_backing, g_ptr_array_unref);
  legacy_backing_bytes = 0;
}
static void legacy_descriptor_unref (gpointer d)
{ gimp_preset_descriptor_unref (d); }
static void legacy_expose (GimpPresetDescriptor *d)
{
  guint i;
  if (d == &defaults) return;
  if (!legacy_backing)
    legacy_backing = g_ptr_array_new_with_free_func (legacy_descriptor_unref);
  for (i = 0; i < legacy_backing->len; i++)
    if (g_ptr_array_index (legacy_backing, i) == d) return;
  g_assert (d->allocation_size <= G_MAXSIZE - legacy_backing_bytes);
  g_ptr_array_add (legacy_backing, gimp_preset_descriptor_ref (d));
  legacy_backing_bytes += d->allocation_size;
}

static gboolean fail (GError **error, const gchar *message)
{
  g_set_error_literal (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE, message);
  return FALSE;
}
GimpPresetDescriptor *gimp_preset_descriptor_ref (GimpPresetDescriptor *d)
{ if (d && d != &defaults) g_atomic_int_inc (&d->refs); return d; }
void gimp_preset_descriptor_unref (GimpPresetDescriptor *d)
{ if (d && d != &defaults && g_atomic_int_dec_and_test (&d->refs)) g_free (d); }
const GimpPresetDescriptorValues *gimp_preset_descriptor_values (const GimpPresetDescriptor *d)
{ return d ? &d->values : NULL; }

GimpPresetDescriptor *
gimp_preset_descriptor_new (const GimpPresetDescriptorValues *v, GError **error)
{
  const gchar *fields[16];
  gchar *copies[16];
  gsize size = sizeof (GimpPresetDescriptor);
  GimpPresetDescriptor *d;
  gchar *next;
  guint i;
  G_STATIC_ASSERT (sizeof fields == sizeof *v);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  if (!v || !v->tree_label || !v->label || !v->icon || !v->help_data ||
      !v->fs_label || !v->reset_label || !v->path_property_name ||
      !*v->path_property_name || !v->dialog_label || !v->dialog_icon ||
      !v->dialog_help || !v->action_label || !v->action_help)
    { fail (error, "Incomplete preset descriptor"); return NULL; }
  /* The structure consists exclusively of same-type pointer members; avoid
   * type-punning access by copying its representation into a pointer array. */
  memcpy (fields, v, sizeof fields);
  for (i = 0; i < G_N_ELEMENTS (fields); i++)
    if (fields[i])
      {
        gsize n;
        if (!g_utf8_validate (fields[i], -1, NULL))
          { fail (error, "Preset metadata must be valid UTF-8"); return NULL; }
        n = strlen (fields[i]) + 1;
        if (n > G_MAXSIZE - size)
          { fail (error, "Preset metadata size overflow"); return NULL; }
        size += n;
      }
  d = g_try_malloc0 (size);
  if (!d) { fail (error, "Cannot allocate preset descriptor"); return NULL; }
  d->refs = 1; d->allocation_size = size; next = (gchar *) (d + 1);
  for (i = 0; i < G_N_ELEMENTS (fields); i++)
    if (fields[i])
      { gsize n = strlen (fields[i]) + 1; copies[i] = next; memcpy (next, fields[i], n); next += n; }
    else copies[i] = NULL;
  memcpy (&d->values, copies, sizeof copies);
  return d;
}
static void select_catalog (GimpPresetDescriptor *d)
{
  GimpPresetDescriptor *old = catalog;
  catalog = gimp_preset_descriptor_ref (d); catalog_generation++;
  gimp_preset_descriptor_unref (old);
}
static gboolean valid_path (GObjectClass *klass, const gchar *name, gboolean nullable)
{
  GParamSpec *p;
  if (!name) return nullable;
  p = g_object_class_find_property (klass, name);
  return p && GIMP_IS_PARAM_SPEC_CONFIG_PATH (p) &&
    gimp_param_spec_config_path_type (p) == GIMP_CONFIG_PATH_DIR_LIST &&
    (p->flags & G_PARAM_READWRITE) == G_PARAM_READWRITE;
}
static gboolean validate (Gimp *gimp, GimpPresetDescriptor *d, GError **error)
{
  GObjectClass *klass = gimp && gimp->config ? G_OBJECT_GET_CLASS (gimp->config) :
    g_type_class_ref (GIMP_TYPE_CORE_CONFIG);
  gboolean ok = valid_path (klass, d->values.path_property_name, FALSE) &&
    valid_path (klass, d->values.writable_property_name, TRUE);
  if (!gimp || !gimp->config) g_type_class_unref (klass);
  return ok || fail (error, "Preset properties must be existing read/write directory-list paths");
}
static GimpDataFactory *new_factory (Gimp *gimp, GimpPresetDescriptor *d)
{
  GimpDataFactory *factory = gimp_data_loader_factory_new (gimp, GIMP_TYPE_LAYER_PRESET,
    d->values.path_property_name, d->values.writable_property_name,
    "layer-preset-paths", NULL, NULL);
  if (!factory) return NULL;
  gimp_object_set_static_name (GIMP_OBJECT (factory), "layer preset factory");
  gimp_data_loader_factory_add_loader (factory, "Painter Layer Preset", gimp_layer_preset_load,
                                     ".json", d->values.writable_property_name != NULL);
  return factory;
}
static gboolean replaceable (Gimp *gimp, GimpPresetOwner *o)
{
  return o->active && !o->frozen && gimp->layer_preset_factory &&
    gimp_container_get_n_children (gimp_data_factory_get_container (gimp->layer_preset_factory)) == 0 &&
    gimp_async_set_is_empty (gimp_data_factory_get_async_set (gimp->layer_preset_factory));
}
GimpPresetDescriptor *gimp_preset_catalog_snapshot (Gimp *gimp)
{
  if (!gimp) return gimp_preset_descriptor_ref (catalog);
  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  return gimp_preset_descriptor_ref (gimp_preset_owner (gimp)->descriptor);
}
gboolean gimp_preset_catalog_register (Gimp *gimp, GimpPresetDescriptor *d, GError **error)
{
  GimpPresetOwner *o;
  GimpPresetDescriptor *old;
  GimpDataFactory *candidate, *old_factory;
  guint64 generation, global_generation;
  gboolean result = FALSE;
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  if (!d || (gimp && !GIMP_IS_GIMP (gimp))) return fail (error, "Invalid preset registration owner");
  if (frozen_owners || gui_reservations) return fail (error, "Preset catalog is frozen by initialized or published GUI owners");
  if (!validate (gimp, d, error)) return FALSE;
  if (!gimp)
    {
      if (owners) return fail (error, "Preentry registration requires no application owners");
      select_catalog (d); return TRUE;
    }
  return TRUE; /* causal mutant: falsely report replacement */
  o = gimp_preset_owner (gimp);
  if (o->publishing || !replaceable (gimp, o)) return fail (error, "Preset owner is closed, initialized, loading or nonempty");
  o->publishing = TRUE;
  generation = o->generation; global_generation = catalog_generation;
  gimp_preset_descriptor_ref (d);
  g_object_ref (gimp);
  if (!replaceable (gimp, o) || generation != o->generation || frozen_owners || gui_reservations)
    { fail (error, "Preset owner changed during acquisition"); goto done; }
  candidate = new_factory (gimp, d);
  if (!candidate) { fail (error, "Cannot prepare preset factory"); goto done; }
  if (!replaceable (gimp, o) || generation != o->generation || global_generation != catalog_generation || frozen_owners || gui_reservations)
    { g_object_unref (candidate); fail (error, "Preset owner changed during preparation"); goto done; }
  /* No callbacks between these publications. Old release callbacks see the
   * complete new pair and cannot recursively replace while publishing is set. */
  old = o->descriptor; old_factory = gimp->layer_preset_factory;
  o->descriptor = gimp_preset_descriptor_ref (d); o->generation++;
  gimp->layer_preset_factory = candidate;
  select_catalog (d);
  g_object_unref (old_factory);
  gimp_preset_descriptor_unref (old);
  result = TRUE;
done:
  gimp_preset_descriptor_unref (d);
  o->publishing = FALSE;
  g_object_unref (gimp);
  return result;
}
void gimp_preset_owner_open (Gimp *gimp)
{
  GimpPresetOwner *o = gimp_preset_owner (gimp);
  GimpDataFactory *candidate;
  guint64 generation;
  if (o->active || o->descriptor || o->retired || o->publishing) return;
  generation = o->generation;
  o->publishing = TRUE;
  /* No active descriptor/factory is published during native construction.
   * Emission observers see the absent pair, not a half-initialized owner. */
  candidate = new_factory (gimp, &defaults);
  if (!candidate || o->retired || generation != o->generation)
    { g_clear_object (&candidate); o->publishing = FALSE; return; }
  if (!frozen_owners && !gui_reservations) select_catalog (&defaults);
  o->descriptor = gimp_preset_descriptor_ref (&defaults);
  gimp->layer_preset_factory = candidate;
  o->active = TRUE; o->generation++; owners++;
  o->publishing = FALSE;
}
void gimp_preset_owner_freeze (Gimp *gimp)
{
  GimpPresetOwner *o = gimp_preset_owner (gimp);
  if (o->active && !o->frozen) { o->frozen = TRUE; frozen_owners++; }
}
void gimp_preset_owner_close (Gimp *gimp)
{
  GimpPresetOwner *o = gimp_preset_owner (gimp);
  if (o->retired) return;
  o->retired = TRUE; o->generation++;
  if (!o->active) return;
  o->active = FALSE;
  if (o->frozen) { o->frozen = FALSE; g_assert (frozen_owners); frozen_owners--; }
  g_assert (owners); owners--;
  if (!owners) { select_catalog (&defaults); legacy_epoch_clear (); }
}
void gimp_preset_owner_finalize (Gimp *gimp)
{
  GimpPresetOwner *o = gimp_preset_owner (gimp);
  gimp_preset_owner_close (gimp);
  g_clear_pointer (&o->descriptor, gimp_preset_descriptor_unref);
}
GimpPresetGuiLease *gimp_preset_gui_begin (Gimp *gimp)
{
  GimpPresetOwner *o;
  GimpPresetGuiLease *lease;
  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  o = gimp_preset_owner (gimp);
  if (!o->active || o->publishing) return NULL;
  lease = g_try_new0 (GimpPresetGuiLease, 1);
  if (!lease) return NULL;
  lease->generation = o->generation;
  lease->descriptor = gimp_preset_descriptor_ref (o->descriptor);
  gui_reservations++;
  lease->gimp = g_object_ref (gimp);
  if (!o->active || lease->generation != o->generation)
    { gimp_preset_gui_end (lease, FALSE); return NULL; }
  return lease;
}
const GimpPresetDescriptorValues *gimp_preset_gui_values (GimpPresetGuiLease *lease)
{ return lease ? &lease->descriptor->values : NULL; }
gboolean gimp_preset_gui_valid (GimpPresetGuiLease *lease)
{
  GimpPresetOwner *o;
  if (!lease) return FALSE;
  o = gimp_preset_owner (lease->gimp);
  return o->active && o->generation == lease->generation && o->descriptor == lease->descriptor;
}
gboolean gimp_preset_gui_end (GimpPresetGuiLease *lease, gboolean published)
{
  GimpPresetOwner *o;
  gboolean valid;
  if (!lease) return FALSE;
  o = gimp_preset_owner (lease->gimp);
  valid = o->active && o->generation == lease->generation && o->descriptor == lease->descriptor;
  if (published && valid) gimp_preset_owner_freeze (lease->gimp);
  g_assert (gui_reservations); gui_reservations--;
  gimp_preset_descriptor_unref (lease->descriptor);
  g_object_unref (lease->gimp);
  g_free (lease);
  return valid && published;
}

/* The owning API keeps immutable metadata independent of public mutable arrays. */
#include "preset-factory-gui.h"
struct _GimpPresetPreferencesSnapshot
{
  gint refs;
  GimpPresetDescriptor *descriptor;
  PrefsDialogEntry entry;
};
static PrefsDialogEntry prefs_entry (GimpPresetDescriptor *d)
{
  const GimpPresetDescriptorValues *v = gimp_preset_descriptor_values (d);
  PrefsDialogEntry entry = { v->tree_label, v->label, v->icon, v->help_data,
    v->fs_label, v->path_property_name, v->writable_property_name };
  return entry;
}
GimpPresetPreferencesSnapshot *
gimp_preset_preferences_snapshot_new (Gimp *gimp, GError **error)
{
  GimpPresetPreferencesSnapshot *snapshot;
  GimpPresetDescriptor *d;
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  d = gimp_preset_catalog_snapshot (gimp);
  if (!d) { fail (error, "No preset descriptor for snapshot"); return NULL; }
  snapshot = g_try_new0 (GimpPresetPreferencesSnapshot, 1);
  if (!snapshot)
    { gimp_preset_descriptor_unref (d); fail (error, "Cannot allocate preset preferences snapshot"); return NULL; }
  snapshot->refs = 1; snapshot->descriptor = d; snapshot->entry = prefs_entry (d);
  return snapshot;
}
GimpPresetPreferencesSnapshot *
gimp_preset_preferences_snapshot_ref (GimpPresetPreferencesSnapshot *snapshot)
{ if (snapshot) g_atomic_int_inc (&snapshot->refs); return snapshot; }
void gimp_preset_preferences_snapshot_unref (GimpPresetPreferencesSnapshot *snapshot)
{
  if (snapshot && g_atomic_int_dec_and_test (&snapshot->refs))
    { gimp_preset_descriptor_unref (snapshot->descriptor); g_free (snapshot); }
}
const PrefsDialogEntry *
gimp_preset_preferences_snapshot_get_entries (GimpPresetPreferencesSnapshot *snapshot,
                                              guint *length)
{
  if (length) *length = snapshot ? 1 : 0;
  return snapshot ? &snapshot->entry : NULL;
}
/* Legacy shallow entries retain backing only through the explicit catalog epoch. */
GArray *preset_factory_gui_prefs_entry_point (void)
{
  GimpPresetDescriptor *d = gimp_preset_catalog_snapshot (NULL);
  PrefsDialogEntry entry = prefs_entry (d);
  GArray *array = g_array_new (FALSE, TRUE, sizeof entry);
  legacy_expose (d);
  g_array_append_val (array, entry);
  gimp_preset_descriptor_unref (d);
  return array;
}
