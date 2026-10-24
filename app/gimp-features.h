/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FEATURES_H
#define GIMP_FEATURES_H
#include <glib-object.h>
#include "presets/gimppresetdescriptor.h"
typedef struct _Gimp Gimp;
G_BEGIN_DECLS
/* Original C registration entry. Native lifecycle owns all resources. */
void features_entry_point (Gimp *gimp);
/* Native phase boundaries, called at the existing initialization/restore/exit
 * positions. They do not repeat core data loading, saving or registration. */
void     gimp_features_initialized (Gimp *gimp);
gboolean gimp_features_restored    (Gimp *gimp, GError **error);
void     gimp_features_close       (Gimp *gimp);
G_END_DECLS
#endif
