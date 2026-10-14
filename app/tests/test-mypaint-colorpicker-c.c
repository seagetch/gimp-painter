/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "tools/tools-types.h"
#include "tools/gimpmypainttool.h"
#include "tools/gimptoolcontrol.h"

/* This translation unit is compiled as C, not included by the C++ test. */
_Static_assert (sizeof (GimpMypaintTool) == sizeof (GimpPainterMybrushTool),
                "The source name must designate the existing native owner");

typedef struct
{
  GimpPainterMybrushTool parent_instance;
} TestMypaintColorpicker;

typedef struct
{
  GimpPainterMybrushToolClass parent_class;
} TestMypaintColorpickerClass;

GType test_mypaint_colorpicker_get_type (void);
void  gimp_test_mypaint_colorpicker_c   (GimpMypaintTool *tool);

G_DEFINE_TYPE (TestMypaintColorpicker, test_mypaint_colorpicker,
               GIMP_TYPE_PAINTER_MYBRUSH_TOOL)

static void
test_mypaint_colorpicker_class_init (TestMypaintColorpickerClass *klass)
{
  (void) klass;
}

static void
test_mypaint_colorpicker_init (TestMypaintColorpicker *self)
{
  GimpColorTool *color_tool = GIMP_COLOR_TOOL (self);

  /* Parent instance init has registered its typed slot, but constructed()
   * has not received tool-info or activated that slot yet. */
  g_assert_null (GIMP_TOOL (self)->tool_info);
  g_assert_cmpint (color_tool->pick_target, ==,
                   GIMP_COLOR_PICK_TARGET_FOREGROUND);
  gimp_mypaint_tool_enable_color_picker (GIMP_MYPAINT_TOOL (self),
                                        GIMP_COLOR_PICK_TARGET_BACKGROUND);
  g_assert_cmpint (color_tool->pick_target, ==,
                   GIMP_COLOR_PICK_TARGET_BACKGROUND);
  g_assert_false (gimp_color_tool_is_enabled (color_tool));
  g_assert_false (gimp_tool_control_is_active (GIMP_TOOL (self)->control));
}

static gpointer
colorpicker_wrong_thread (gpointer data)
{
  gimp_mypaint_tool_enable_color_picker (data, GIMP_COLOR_PICK_TARGET_PALETTE);
  return NULL;
}

void
gimp_test_mypaint_colorpicker_c (GimpMypaintTool *tool)
{
  const GimpColorPickTarget targets[] = {
    GIMP_COLOR_PICK_TARGET_NONE,
    GIMP_COLOR_PICK_TARGET_FOREGROUND,
    GIMP_COLOR_PICK_TARGET_BACKGROUND,
    GIMP_COLOR_PICK_TARGET_PALETTE
  };
  GimpColorTool *color_tool = GIMP_COLOR_TOOL (tool);
  GimpTool      *native_tool = GIMP_TOOL (tool);
  gboolean       snap_to = gimp_tool_control_get_snap_to (native_tool->control);
  GObject       *wrong_object;
  GimpColorTool *wrong_tool;
  GimpMypaintTool *initialized;
  GThread       *thread;
  guint          i;

  g_assert_true (GIMP_IS_MYPAINT_TOOL (tool));
  for (i = 0; i < G_N_ELEMENTS (targets); ++i)
    {
      gimp_mypaint_tool_enable_color_picker (tool, targets[i]);
      /* All four values reach the inherited target, including pick-only and
       * palette. NONE is configuration, not an enable/disable flag. */
      g_assert_cmpint (color_tool->pick_target, ==, targets[i]);
      g_assert_false (gimp_color_tool_is_enabled (color_tool));
      g_assert_null (color_tool->options);
      g_assert_false (gimp_tool_control_is_active (native_tool->control));
      g_assert_cmpint (gimp_tool_control_get_snap_to (native_tool->control),
                       ==, snap_to);
    }

  gimp_mypaint_tool_enable_color_picker (tool, GIMP_COLOR_PICK_TARGET_BACKGROUND);
  gimp_mypaint_tool_enable_color_picker (tool, (GimpColorPickTarget) -1);
  gimp_mypaint_tool_enable_color_picker (tool, (GimpColorPickTarget) 1000);
  g_assert_cmpint (color_tool->pick_target, ==, GIMP_COLOR_PICK_TARGET_BACKGROUND);
  gimp_mypaint_tool_enable_color_picker (NULL, GIMP_COLOR_PICK_TARGET_FOREGROUND);

  wrong_object = g_object_new (G_TYPE_OBJECT, NULL);
  gimp_mypaint_tool_enable_color_picker ((GimpMypaintTool *) wrong_object,
                                        GIMP_COLOR_PICK_TARGET_BACKGROUND);
  g_assert_cmpuint (G_OBJECT_TYPE (wrong_object), ==, G_TYPE_OBJECT);
  g_object_unref (wrong_object);

  /* A real color-tool owner is also the wrong type. Its native target must
   * remain untouched, rather than accidentally accepting any color tool. */
  wrong_tool = g_object_new (GIMP_TYPE_COLOR_TOOL,
                             "tool-info", native_tool->tool_info, NULL);
  wrong_tool->pick_target = GIMP_COLOR_PICK_TARGET_NONE;
  gimp_mypaint_tool_enable_color_picker ((GimpMypaintTool *) wrong_tool,
                                        GIMP_COLOR_PICK_TARGET_PALETTE);
  g_assert_cmpint (wrong_tool->pick_target, ==, GIMP_COLOR_PICK_TARGET_NONE);
  g_object_unref (wrong_tool);

  /* The main thread owns the object throughout; the join makes the following
   * read sequential and also catches exceptions escaping the C boundary. */
  thread = g_thread_new ("mypaint-colorpicker-c", colorpicker_wrong_thread, tool);
  g_thread_join (thread);
  g_assert_cmpint (color_tool->pick_target, ==, GIMP_COLOR_PICK_TARGET_BACKGROUND);

  initialized = g_object_new (test_mypaint_colorpicker_get_type (),
                              "tool-info", native_tool->tool_info, NULL);
  g_assert_false (initialized->binding_failed);
  g_assert_cmpint (GIMP_COLOR_TOOL (initialized)->pick_target, ==,
                   GIMP_COLOR_PICK_TARGET_BACKGROUND);
  gimp_mypaint_tool_enable_color_picker (initialized, GIMP_COLOR_PICK_TARGET_NONE);
  g_assert_cmpint (GIMP_COLOR_TOOL (initialized)->pick_target, ==,
                   GIMP_COLOR_PICK_TARGET_NONE);
  g_object_run_dispose (G_OBJECT (initialized));
  gimp_mypaint_tool_enable_color_picker (initialized,
                                        GIMP_COLOR_PICK_TARGET_FOREGROUND);
  g_assert_cmpint (GIMP_COLOR_TOOL (initialized)->pick_target, ==,
                   GIMP_COLOR_PICK_TARGET_NONE);
  g_assert_false (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (initialized)));
  g_assert_false (gimp_tool_control_is_active (GIMP_TOOL (initialized)->control));
  g_object_run_dispose (G_OBJECT (initialized));
  g_object_unref (initialized);

  gimp_mypaint_tool_enable_color_picker (tool, GIMP_COLOR_PICK_TARGET_FOREGROUND);
}
