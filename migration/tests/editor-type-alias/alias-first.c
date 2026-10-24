#include "config.h"
#include <gegl.h>
#include "widgets/gimpmypaintbrusheditor.h"
extern GType gimp_painter_mybrush_editor_get_type(void);
int main(void){ GType a=gimp_mypaint_brush_editor_get_type(); GType b=gimp_painter_mybrush_editor_get_type();g_assert_cmpuint(a, ==, b); g_assert_cmpstr(g_type_name(a), ==, "GimpPainterMybrushEditor"); g_assert_cmpstr(g_type_name(g_type_parent(a)), ==, "GimpEditor"); return 0;}
