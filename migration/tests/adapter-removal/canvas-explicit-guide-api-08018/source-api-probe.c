#include <gegl.h>
#include <gtk/gtk.h>
#include "display/display-types.h"
#include "display/gimpcanvasitem.h"
#include "HEADER"
static GimpCanvasPerspectiveGuide *(*entry)(GimpDisplayShell *,GimpPerspectiveGuide *)=gimp_canvas_perspective_guide_new;
GimpCanvasPerspectiveGuide *probe(GimpDisplayShell *shell,GimpPerspectiveGuide *guide){return entry(shell,guide);}
