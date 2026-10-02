/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_HTTPD_H
#define GIMP_PAINTER_HTTPD_H
#include <glib-object.h>
#include "core/core-types.h"
G_BEGIN_DECLS
#define GIMP_TYPE_PAINTER_HTTPD (gimp_painter_httpd_get_type ())
GType    gimp_painter_httpd_get_type (void) G_GNUC_CONST;
/* Main-context API. Token must contain 32..256 URL-safe random characters.
 * Port zero is supported for ephemeral integration tests. No default listener. */
GObject *gimp_painter_httpd_start (Gimp *, guint port, const gchar *token,
                                   const gchar *webhook_origin, GError **);
void     gimp_painter_httpd_stop (GObject *);
guint    gimp_painter_httpd_port (GObject *);
void     gimp_painter_httpd_enable_navigation (GObject *);
/* Explicit GIMP_PAINTER_HTTP_ENABLE=1 + TOKEN required. Called after restore. */
GObject *gimp_painter_httpd_from_environment (Gimp *, GError **);
G_END_DECLS
#endif
