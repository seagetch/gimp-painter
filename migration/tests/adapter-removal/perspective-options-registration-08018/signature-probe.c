#include <gegl.h>
#include <gtk/gtk.h>
#include "tools/tools-types.h"
#include "tools/gimpperspectiveguidetool.h"
#include "tools/gimpperspectiveguidetool.h"
GType (*options_entry)(void)=gimp_perspective_guide_options_get_type;
GType (*tool_entry)(void)=gimp_perspective_guide_tool_get_type;
void (*register_entry)(GimpToolRegisterCallback,gpointer)=gimp_perspective_guide_tool_register;
