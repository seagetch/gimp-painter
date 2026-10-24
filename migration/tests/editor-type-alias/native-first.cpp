#include "config.h"
#include <gegl.h>
#include <type_traits>
#include "widgets/gimpmypaintbrusheditor.hpp"
static_assert(std::is_same<GimpMypaintBrushEditor,GimpPainterMybrushEditor>::value,"same owner");
int main(){GType b=gimp_painter_mybrush_editor_get_type(); GType a=GimpPainter::TypeTraits<GimpMypaintBrushEditor>::type();g_assert_cmpuint(a, ==, b); g_assert_cmpstr(g_type_name(a), ==, "GimpPainterMybrushEditor"); g_assert_cmpstr(g_type_name(g_type_parent(a)), ==, "GimpEditor"); return 0;}
