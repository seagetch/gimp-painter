/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "tools/tools-types.h"
#include "core/gimp.h"
#include "core/gimpdisplay.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "tools/gimptool.h"
#include "tools/gimptoolcontrol.h"
#include "tests.h"
#include "gimp-app-test-utils.h"

typedef struct { GimpTool parent; GimpTool **caller; } PressOwnerTool;
typedef struct { GimpToolClass parent; } PressOwnerToolClass;
static Gimp *gimp;
GType press_owner_tool_get_type (void);
G_DEFINE_TYPE (PressOwnerTool, press_owner_tool, GIMP_TYPE_TOOL)
static void press (GimpTool *tool, const GimpCoords *coords, guint32 time,
                   GdkModifierType state, GimpButtonPressType type,
                   GimpDisplay *display)
{
  /* Force the public wrapper's post-callback state/coordinate bookkeeping. */
  gimp_tool_control_activate (tool->control);
  g_clear_object (((PressOwnerTool *) tool)->caller);
}
static void press_owner_tool_class_init (PressOwnerToolClass *klass)
{ GIMP_TOOL_CLASS (klass)->button_press = press; }
static void press_owner_tool_init (PressOwnerTool *tool) { tool->caller = NULL; }
static void finalized (gpointer data, GObject *object) { *(gboolean *) data = TRUE; }
static void public_press_owner (void)
{
  GimpToolInfo *info;
  GimpDisplay *display;
  GimpTool *tool;
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  gboolean gone = FALSE;
  info = gimp_tool_info_new (gimp, press_owner_tool_get_type (), GIMP_TYPE_TOOL_OPTIONS,
                            0, "press-owner-test", "Owner test", "Owner test",
                            NULL, NULL, NULL, "gimp-tool-paintbrush", "gimp-paintbrush", "gimp-tool-paintbrush");
  g_assert_nonnull (info);
  display = g_object_new (GIMP_TYPE_DISPLAY, "gimp", gimp, NULL);
  tool = g_object_new (press_owner_tool_get_type (), "tool-info", info, NULL);
  ((PressOwnerTool *) tool)->caller = &tool;
  g_object_weak_ref (G_OBJECT (tool), finalized, &gone);
  gimp_tool_button_press (tool, &coords, 123, 0, GIMP_BUTTON_PRESS_NORMAL, display);
  g_assert_null (tool);
  g_assert_true (gone);
  g_object_unref (display);
  g_object_unref (info);
}
int main (int argc, char **argv)
{
  gint result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/tool-owner/public-press-last-ref", public_press_owner);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE);
  return result;
}
