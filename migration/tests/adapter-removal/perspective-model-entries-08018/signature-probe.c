#include <glib-object.h>
#include "core/core-types.h"
#include "HEADER"
void check(void) {
 GType (*type)(void)=gimp_perspective_guide_get_type;
 GimpPerspectiveGuide *(*create)(guint32)=gimp_perspective_guide_new;
 gint (*count)(GimpPerspectiveGuide*)=gimp_perspective_guide_get_vanish_point_length;
 gint (*add)(GimpPerspectiveGuide*,gdouble,gdouble)=gimp_perspective_guide_add_vanish_points;
 gboolean (*remove)(GimpPerspectiveGuide*,gint)=gimp_perspective_guide_remove_vanish_points;
 gboolean (*get)(GimpPerspectiveGuide*,gint,gdouble*,gdouble*)=gimp_perspective_guide_get_vanish_points;
 gboolean (*set)(GimpPerspectiveGuide*,gint,gdouble,gdouble)=gimp_perspective_guide_set_vanish_points;
 (void)type;(void)create;(void)count;(void)add;(void)remove;(void)get;(void)set;
}
