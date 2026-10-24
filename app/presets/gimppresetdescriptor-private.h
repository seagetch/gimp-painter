/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PRESET_DESCRIPTOR_PRIVATE_H
#define GIMP_PRESET_DESCRIPTOR_PRIVATE_H
#include "gimppresetdescriptor.h"
G_BEGIN_DECLS
typedef struct
{
  GimpPresetDescriptor *descriptor;
  guint64 generation;
  gboolean active, frozen, publishing, retired;
} GimpPresetOwner;
/* Sole owner storage is standard Gimp instance-private data, not qdata/map. */
GimpPresetOwner *gimp_preset_owner (Gimp *gimp);
void gimp_preset_owner_open (Gimp *gimp);
void gimp_preset_owner_freeze (Gimp *gimp);
void gimp_preset_owner_close (Gimp *gimp);
void gimp_preset_owner_finalize (Gimp *gimp);
/* Metadata allocations retained by the borrowed legacy API in this epoch. */
void gimp_preset_catalog_legacy_usage (guint *count, gsize *bytes);
G_END_DECLS
#endif
