/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PRESET_DESCRIPTOR_H
#define GIMP_PRESET_DESCRIPTOR_H
#include <glib-object.h>
typedef struct _Gimp Gimp;
typedef struct _GimpPresetDescriptor GimpPresetDescriptor;
typedef struct _GimpPresetGuiLease GimpPresetGuiLease;
G_BEGIN_DECLS
/* Explicit native source migration: fixed layer-preset loader/schema, variable
 * existing directory-list config properties and presentation metadata. */
typedef struct
{
  const gchar *tree_label, *label, *icon, *help_data, *fs_label, *reset_label;
  const gchar *path_property_name, *writable_property_name;
  const gchar *dialog_label, *dialog_blurb, *dialog_icon, *dialog_help;
  const gchar *action_label, *action_short_label, *action_tooltip, *action_help;
} GimpPresetDescriptorValues;
GimpPresetDescriptor *gimp_preset_descriptor_new (const GimpPresetDescriptorValues *values, GError **error);
GimpPresetDescriptor *gimp_preset_descriptor_ref (GimpPresetDescriptor *descriptor);
void gimp_preset_descriptor_unref (GimpPresetDescriptor *descriptor);
const GimpPresetDescriptorValues *gimp_preset_descriptor_values (const GimpPresetDescriptor *descriptor);
/* Transfer full immutable snapshot. NULL selects the process catalog, not an
 * arbitrary application. Plain snapshots do not freeze registration. */
GimpPresetDescriptor *gimp_preset_catalog_snapshot (Gimp *gimp);
/* NULL target is a preentry catalog selection, overwritten by normal entry.
 * Existing owners retain their own snapshots. Main-thread lifecycle API. */
gboolean gimp_preset_catalog_register (Gimp *gimp, GimpPresetDescriptor *descriptor, GError **error);
/* Internal native GUI publication guard: cancel failed/empty construction;
 * successful publication freezes this owner. No resource or factory clone. */
GimpPresetGuiLease *gimp_preset_gui_begin (Gimp *gimp);
const GimpPresetDescriptorValues *gimp_preset_gui_values (GimpPresetGuiLease *lease);
gboolean gimp_preset_gui_valid (GimpPresetGuiLease *lease);
gboolean gimp_preset_gui_end (GimpPresetGuiLease *lease, gboolean published);
G_END_DECLS
#endif
