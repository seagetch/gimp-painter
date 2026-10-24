#include "config.h"
#include <gtk/gtk.h>
#include "core/core-types.h"
#include "display/display-types.h"
#include "display/gimpimagewindow.h"
GType gimp_image_window_get_type(void) G_GNUC_CONST;
void gimp_image_window_set_active_shell(GimpImageWindow*,GimpDisplayShell*);
GimpDisplayShell *gimp_image_window_get_active_shell(GimpImageWindow*);
