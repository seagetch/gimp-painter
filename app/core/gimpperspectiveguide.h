/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_PERSPECTIVE_GUIDE_H__
#define __GIMP_PERSPECTIVE_GUIDE_H__
#include <glib-object.h>
G_BEGIN_DECLS
#define GIMP_TYPE_PERSPECTIVE_GUIDE (gimp_perspective_guide_get_type ())
#define GIMP_PERSPECTIVE_GUIDE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE, GimpPerspectiveGuide))
#define GIMP_IS_PERSPECTIVE_GUIDE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE))
typedef struct _GimpPerspectiveGuide GimpPerspectiveGuide;
typedef struct _GimpPerspectiveGuideClass GimpPerspectiveGuideClass;
struct _GimpPerspectiveGuide { GObject parent_instance; gboolean binding_failed; };
struct _GimpPerspectiveGuideClass {
  GObjectClass parent_class;
  void (*removed) (GimpPerspectiveGuide *guide);
};
typedef struct {
  guint32 id;
  gdouble angle;
  gint n_points;
  struct { gdouble x, y; } points[3];
} GimpPerspectiveGuideState;
GType gimp_perspective_guide_get_type (void) G_GNUC_CONST;
GimpPerspectiveGuide *gimp_perspective_guide_new (guint32 id);
GimpPerspectiveGuide *gimp_perspective_guide_duplicate (GimpPerspectiveGuide *guide);
gboolean gimp_perspective_guide_get_state (GimpPerspectiveGuide *, GimpPerspectiveGuideState *);
gint gimp_perspective_guide_get_vanish_point_length (GimpPerspectiveGuide *);
gint gimp_perspective_guide_add_vanish_points (GimpPerspectiveGuide *, gdouble x, gdouble y);
gboolean gimp_perspective_guide_remove_vanish_points (GimpPerspectiveGuide *, gint index);
gboolean gimp_perspective_guide_get_vanish_points (GimpPerspectiveGuide *, gint index, gdouble *x, gdouble *y);
gboolean gimp_perspective_guide_set_vanish_points (GimpPerspectiveGuide *, gint index, gdouble x, gdouble y);
/* Legacy direction selection: 32 scaled pixels, ordered candidates, first tie wins. */
gboolean gimp_perspective_guide_snap_angle (GimpPerspectiveGuide *, gdouble origin_x, gdouble origin_y,
                                            gdouble x, gdouble y, gdouble scale_x, gdouble scale_y,
                                            gdouble *angle);
/* Preserve the dominant coordinate; this is deliberately not orthogonal projection. */
void gimp_perspective_guide_constrain (gdouble angle, gdouble origin_x, gdouble origin_y,
                                      gdouble *x, gdouble *y);
G_END_DECLS
#endif
