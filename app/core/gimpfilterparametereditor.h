/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FILTER_PARAMETER_EDITOR_H
#define GIMP_FILTER_PARAMETER_EDITOR_H
G_BEGIN_DECLS
void gimp_filter_parameter_editor_initialize (GimpPlugInManager *manager);
void gimp_filter_parameter_editor_activate   (GimpPlugInManager *manager);
void gimp_filter_parameter_editor_close      (GimpPlugInManager *manager);
/* Called once during normal restore, before temporary definitions are freed.
 * Only the four fixed bundled providers are retained; no query is performed. */
void gimp_filter_parameter_editor_capture (GimpPlugInManager *manager);
G_END_DECLS
#endif
