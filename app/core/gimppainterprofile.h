/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_PAINTER_PROFILE_H__
#define __GIMP_PAINTER_PROFILE_H__

#include <glib.h>
G_BEGIN_DECLS

/* This marker only permits completing a migrated tool list with new tools.
 * It never proves that an arbitrary source profile originated in Painter. */
#define GIMP_PAINTER_MIGRATED_TOOLRC_VERSION 1001

gboolean gimp_painter_profile_detect    (const gchar *directory);
gboolean gimp_painter_profile_handles   (const gchar *basename);
gboolean gimp_painter_profile_preserve  (const gchar *source,
                                         const gchar *destination,
                                         GError     **error);
gboolean gimp_painter_profile_migrate   (const gchar *source,
                                         const gchar *destination,
                                         GError     **error);
/* Lossless, grammar-aware conversion. Unrecognised fields remain verbatim. */
gchar   *gimp_painter_profile_transform (const gchar *contents,
                                         const gchar *kind,
                                         gboolean     painter_origin,
                                         GError     **error);
/* Only resource search paths, never executable/module/plugin paths. */
gchar *gimp_painter_profile_resource_paths (const gchar *source, GError **error);
const gchar *gimp_painter_profile_action (const gchar *action);

G_END_DECLS
#endif
