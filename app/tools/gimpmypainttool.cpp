/* GIMP-painter
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "base/delegators.hpp"
#include "base/scopeguard.hpp"
#include "base/glib-cxx-bridge.hpp"
#include "base/glib-cxx-utils.hpp"
#include "base/glib-cxx-def-utils.hpp"

extern "C" {
#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>
#include <sys/time.h> // gimp-painter-2.7: for gettimeofday

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"

#include "tools-types.h"

#include "core/gimp.h"
#include "core/gimp-utils.h"
#include "core/gimpdrawable.h"
#include "core/gimperror.h"
#include "core/gimpimage.h"
#include "core/gimpprojection.h"
#include "core/gimptoolinfo.h"

#include "paint/gimpmypaintcore.hpp"
#include "paint/gimpmypaintoptions.h"

#include "widgets/gimpdevices.h"
#include "widgets/gimpwidgets-utils.h"

#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-selection.h"

#include "gimpcoloroptions.h"
#include "gimpmypainttool.h"
#include "gimptoolcontrol.h"

#include "gimp-intl.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "widgets/gimphelp-ids.h"
#include "gimpmypaintoptions-gui.h"
extern "C++" {
#include "base/delegators.hpp"  // g_object_set_cxx_object
};
static void   gimp_mypaint_tool_constructed    (GObject               *object);
static void   gimp_mypaint_tool_finalize       (GObject               *object);

static void   gimp_mypaint_tool_control        (GimpTool              *tool,
                                              GimpToolAction         action,
                                              GimpDisplay           *display);
static void   gimp_mypaint_tool_button_press   (GimpTool              *tool,
                                              const GimpCoords      *coords,
                                              guint32                time,
                                              GdkModifierType        state,
                                              GimpButtonPressType    press_type,
                                              GimpDisplay           *display);
static void   gimp_mypaint_tool_button_release (GimpTool              *tool,
                                              const GimpCoords      *coords,
                                              guint32                time,
                                              GdkModifierType        state,
                                              GimpButtonReleaseType  release_type,
                                              GimpDisplay           *display);
static void   gimp_mypaint_tool_motion         (GimpTool              *tool,
                                              const GimpCoords      *coords,
                                              guint32                time,
                                              GdkModifierType        state,
                                              GimpDisplay           *display);
static void   gimp_mypaint_tool_motion_internal (GimpTool              *tool,
                                              const GimpCoords      *coords,
                                              guint32                time,
                                              GdkModifierType        state,
                                              GimpDisplay           *display,
                                              gboolean               button_pressed);
static void   gimp_mypaint_tool_modifier_key   (GimpTool              *tool,
                                              GdkModifierType        key,
                                              gboolean               press,
                                              GdkModifierType        state,
                                              GimpDisplay           *display);
static void   gimp_mypaint_tool_cursor_update  (GimpTool              *tool,
                                              const GimpCoords      *coords,
                                              GdkModifierType        state,
                                              GimpDisplay           *display);
static void   gimp_mypaint_tool_oper_update    (GimpTool              *tool,
                                              const GimpCoords      *coords,
                                              GdkModifierType        state,
                                              gboolean               proximity,
                                              GimpDisplay           *display);

static void   gimp_mypaint_tool_draw           (GimpDrawTool          *draw_tool);

static void   gimp_mypaint_tool_hard_notify    (GimpMypaintOptions      *options,
                                              const GParamSpec      *pspec,
                                              GimpTool              *tool);


G_DEFINE_TYPE (GimpMypaintTool, gimp_mypaint_tool, GIMP_TYPE_COLOR_TOOL)

#define parent_class gimp_mypaint_tool_parent_class


static void
gimp_mypaint_tool_class_init (GimpMypaintToolClass *klass)
{
  GObjectClass      *object_class    = G_OBJECT_CLASS (klass);
  GimpToolClass     *tool_class      = GIMP_TOOL_CLASS (klass);
  GimpDrawToolClass *draw_tool_class = GIMP_DRAW_TOOL_CLASS (klass);

  object_class->constructed  = gimp_mypaint_tool_constructed;
  object_class->finalize     = gimp_mypaint_tool_finalize;

  tool_class->control        = gimp_mypaint_tool_control;
  tool_class->button_press   = gimp_mypaint_tool_button_press;
  tool_class->button_release = gimp_mypaint_tool_button_release;
  tool_class->motion         = gimp_mypaint_tool_motion;
  tool_class->modifier_key   = gimp_mypaint_tool_modifier_key;
  tool_class->cursor_update  = gimp_mypaint_tool_cursor_update;
  tool_class->oper_update    = gimp_mypaint_tool_oper_update;

  draw_tool_class->draw      = gimp_mypaint_tool_draw;
}

static void
gimp_mypaint_tool_init (GimpMypaintTool *paint_tool)
{
  GimpTool *tool = GIMP_TOOL (paint_tool);

  tool->want_full_motion_tracking = TRUE;
  
  gimp_tool_control_set_motion_mode    (tool->control, GIMP_MOTION_MODE_EXACT);
  gimp_tool_control_set_scroll_lock    (tool->control, TRUE);
  gimp_tool_control_set_action_value_1 (tool->control,
                                        "context/context-opacity-set");

  paint_tool->pick_colors = FALSE;
  paint_tool->draw_line   = FALSE;

  paint_tool->status      = _("Click to paint");
  paint_tool->status_line = _("Click to draw the line");
  paint_tool->status_ctrl = _("%s to pick a color");

  paint_tool->core        = NULL;

  //FIXME! pick_mode should be changed according to the attributes of the selected slash
  GIMP_COLOR_TOOL (tool)->pick_mode = GIMP_COLOR_PICK_MODE_FOREGROUND;
}

static void
gimp_mypaint_tool_constructed (GObject *object)
{
  GimpTool         *tool       = GIMP_TOOL (object);
  GimpMypaintTool    *paint_tool = GIMP_MYPAINT_TOOL (object);
  GimpMypaintOptions *options    = GIMP_MYPAINT_TOOL_GET_OPTIONS (tool);
//  GimpMypaintInfo    *paint_info;
  GimpMypaintCore    *core;

  if (G_OBJECT_CLASS (parent_class)->constructed)
    G_OBJECT_CLASS (parent_class)->constructed (object);

  g_assert (GIMP_IS_TOOL_INFO (tool->tool_info));
//  g_assert (GIMP_IS_MYPAINT_INFO (tool->tool_info->paint_info));

//  paint_info = tool->tool_info->paint_info;

//  g_assert (g_type_is_a (paint_info->mypaint_type, GIMP_TYPE_MYPAINT_CORE));

  paint_tool->core = core = new GimpMypaintCore();
  g_object_set_cxx_object(object, "paint-core", core);
/*
  g_signal_connect_object (options, "notify::hard",
                           G_CALLBACK (gimp_mypaint_tool_hard_notify),
                           tool, 0);
  gimp_mypaint_tool_hard_notify (options, NULL, tool);
*/
}

static void
gimp_mypaint_tool_finalize (GObject *object)
{
  GimpMypaintTool *paint_tool = GIMP_MYPAINT_TOOL (object);
/*
  if (paint_tool->core) {
    GimpMypaintCore* core = reinterpret_cast<GimpMypaintCore*>(paint_tool->core);
    delete core;
    paint_tool->core = NULL;
  }
*/
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gimp_mypaint_tool_enable_color_picker:
 * @tool: a #GimpMypaintTool
 * @mode: the #GimpColorPickMode to set
 *
 * This is a convenience function used from the init method of paint
 * tools that want the color picking functionality. The @mode that is
 * set here is used to decide what cursor modifier to draw and if the
 * picked color goes to the foreground or background color.
 **/
void
gimp_mypaint_tool_enable_color_picker (GimpMypaintTool     *tool,
                                     GimpColorPickMode  mode)
{
  g_return_if_fail (GIMP_IS_MYPAINT_TOOL (tool));

  tool->pick_colors = TRUE;

  GIMP_COLOR_TOOL (tool)->pick_mode = mode;
}

static void
gimp_mypaint_tool_control (GimpTool       *tool,
                         GimpToolAction  action,
                         GimpDisplay    *display)
{
  GimpMypaintTool *paint_tool = GIMP_MYPAINT_TOOL (tool);
  GimpMypaintCore* core = NULL;

  switch (action) {
    case GIMP_TOOL_ACTION_PAUSE:
    case GIMP_TOOL_ACTION_RESUME:
      break;

    case GIMP_TOOL_ACTION_HALT:
      core = reinterpret_cast<GimpMypaintCore*>(paint_tool->core);
      core->cleanup();
      break;
  }

  GIMP_TOOL_CLASS (parent_class)->control (tool, action, display);
}

static void
gimp_mypaint_tool_button_press (GimpTool            *tool,
                              const GimpCoords    *coords,
                              guint32              time,
                              GdkModifierType      state,
                              GimpButtonPressType  press_type,
                              GimpDisplay         *display)
{
  GimpDrawTool     *draw_tool     = GIMP_DRAW_TOOL (tool);
  GimpDisplayShell *shell         = gimp_display_get_shell (display);
  GimpImage        *image         = gimp_display_get_image (display);
  GimpCoords        curr_coords;
  gint              off_x, off_y;
  GError           *error = NULL;

  if (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (tool))) {
    g_print("button_press where ColorTool.is_enabled == TRUE\n");
    GIMP_TOOL_CLASS (parent_class)->button_press (tool, coords, time, state,
                                                  press_type, display);
    return;
  }

  if (gimp_draw_tool_is_active (draw_tool))
    gimp_draw_tool_stop (draw_tool);

  if (tool->display            &&
      tool->display != display &&
      gimp_display_get_image (tool->display) == image) {
    /*  if this is a different display, but the same image, HACK around
     *  in tool internals AFTER stopping the current draw_tool, so
     *  straight line drawing works across different views of the
     *  same image.
     */

    tool->display = display;
  }
/*
  if (! gimp_mypaint_core_start (core, drawable, paint_options, &curr_coords,
                               &error))
    {
      gimp_tool_message_literal (tool, display, error->message);
      g_clear_error (&error);
      return;
    }
*/

  {
      GimpImage *image = gimp_display_get_image (display);

      tool->display  = display;
      tool->drawable = gimp_image_get_active_drawable (image);

      gimp_tool_control_activate (tool->control);
    }

  /*  pause the current selection  */
  gimp_display_shell_selection_pause (shell);

  /*  Let the specific painting function initialize itself  */
  if (press_type == GIMP_BUTTON_PRESS_NORMAL) {  // ignore the extra double-click event
    if (state & GDK_BUTTON1_MASK) { // mouse button pressed (while painting without pressure information)
      // For the mouse we don't get a motion event for "pressure"
      // changes, so we simulate it. (Note: we can't use the
      // event's button state because it carries the old state.)
      gimp_mypaint_tool_motion_internal (tool, coords, time, state, display, coords->pressure > 0.0 || (state & GDK_BUTTON1_MASK));
    }
  }

  gimp_projection_flush_now (gimp_image_get_projection (image));
  gimp_display_flush_now (display);
  GIMP_MYPAINT_TOOL(tool)->last_flush_time = time;
  gimp_draw_tool_start (draw_tool, display);
}


#if 0
    def button_release_cb(self, win, event):
        # (see comment above in button_press_cb)
        if event.button == 1:
            if not self.last_event_had_pressure_info:
                self.motion_notify_cb(win, event, button1_pressed=False)
            # Outsiders can access this via gui.document
            for func in self._input_stroke_ended_observers:
                func(event)
#endif
static void
gimp_mypaint_tool_button_release (GimpTool              *tool,
                                const GimpCoords      *coords,
                                guint32                time,
                                GdkModifierType        state,
                                GimpButtonReleaseType  release_type,
                                GimpDisplay           *display)
{
  GimpMypaintTool    *paint_tool    = GIMP_MYPAINT_TOOL (tool);
  GimpDisplayShell *shell         = gimp_display_get_shell (display);
  GimpImage        *image         = gimp_display_get_image (display);

  if (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (tool)))
    {
      GIMP_TOOL_CLASS (parent_class)->button_release (tool, coords, time,
                                                      state, release_type,
                                                      display);
      return;
    }

  gimp_draw_tool_pause (GIMP_DRAW_TOOL (tool));

  /*  Let the specific painting function finish up  */
  gimp_mypaint_tool_motion_internal (tool, coords, time, state, display, coords->pressure > 0.0 || (state & GDK_BUTTON1_MASK));

  /*  resume the current selection  */
  gimp_display_shell_selection_resume (shell);
  gimp_image_flush (image);

  gimp_draw_tool_resume (GIMP_DRAW_TOOL (tool));
}

static void
gimp_mypaint_tool_motion (GimpTool         *tool,
                        const GimpCoords *coords,
                        guint32           time,
                        GdkModifierType   state,
                        GimpDisplay      *display)
{
  gimp_mypaint_tool_motion_internal (tool, coords, time, state, display, coords->pressure > 0.0 || (state & GDK_BUTTON1_MASK));
}


static void
gimp_mypaint_tool_motion_internal (GimpTool         *tool,
                                   const GimpCoords *coords,
                                   guint32           time,
                                   GdkModifierType   state,
                                   GimpDisplay      *display,
                                   gboolean          button1_pressed)
{
  GimpMypaintTool    *paint_tool    = GIMP_MYPAINT_TOOL (tool);
  GimpMypaintOptions *paint_options = GIMP_MYPAINT_TOOL_GET_OPTIONS (tool);
  GimpMypaintCore    *core = reinterpret_cast<GimpMypaintCore*>(paint_tool->core);
  GimpImage        *image         = gimp_display_get_image (display);
  GimpDrawable     *drawable      = gimp_image_get_active_drawable (image);
  GimpCoords        curr_coords;
  gint              off_x, off_y;
  gdouble           dtime;

  if (button1_pressed)
    GIMP_TOOL_CLASS (parent_class)->motion (tool, coords, time, state, display);

  if (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (tool)))
    return;

  curr_coords = *coords;

  gimp_item_get_offset (GIMP_ITEM (drawable), &off_x, &off_y);

  curr_coords.x -= off_x;
  curr_coords.y -= off_y;


  if (paint_tool->last_event_time > 0) {
      dtime = (time - paint_tool->last_event_time) / 1000.0;
  } else
      dtime = 0;

  paint_tool->last_event_x = curr_coords.x;
  paint_tool->last_event_y = curr_coords.y;
  paint_tool->last_event_time = time;
  if (dtime == 0)
      return;

  // Refuse drawing if the layer is locked or hidden
  if (! gimp_item_is_editable (GIMP_ITEM (drawable))) {
      gimp_tool_message_literal (tool, display,
                                 _("Cannot paint on this layer."));
      return;
  }

  if (gimp_item_is_content_locked (GIMP_ITEM (drawable))) {
      gimp_tool_message_literal (tool, display,
                                 _("The active layer's pixels are locked."));
      return;
  }

  // NOTICE: coordinates is already transformed at 
  //         gimp_display_shell_canvas_tool_events @ gimpdisplayshell-tool-events.c
  /*
        cr = self.get_model_coordinates_cairo_context()
        x, y = cr.device_to_user(event.x, event.y)
  pressure = event.get_axis(gdk.AXIS_PRESSURE)
  */

  // FIXME: Gimp sets default pressure to 1.0 at gimpdeviceinfo-coords.c while MyPaint sets 0.5 or 0
  //         according to its button state.

  if (curr_coords.pressure > 0.0) {
    paint_tool->last_painting_x = curr_coords.x;
    paint_tool->last_painting_y = curr_coords.y;
  }

  // If the device has changed and the last pressure value from the previous device
  // is not equal to 0.0, this can leave a visible stroke on the layer even if the 'new'
  // device is not pressed on the tablet and has a pressure axis == 0.0.
  // Reseting the brush when the device changes fixes this issue, but there may be a
  // much more elegant solution that only resets the brush on this edge-case.
  // FIXME: Can't get device information here!
//  if (!same_device)
//    core->reset_brush();

  //# On Windows, GTK timestamps have a resolution around
  //# 15ms, but tablet events arrive every 8ms.
  //# https://gna.org/bugs/index.php?16569
  //# TODO: proper fix in the brush engine, using only smooth,
  //#       filtered speed inputs, will make this unneccessary
  /*
  if dtime < 0.0:
      print 'Time is running backwards, dtime=%f' % dtime
      dtime = 0.0
  data = (x, y, pressure, xtilt, ytilt)
  if dtime == 0.0:
      self.motions.append(data)
  elif dtime > 0.0:
      if self.motions:
          # replay previous events that had identical timestamp
          if dtime > 0.1:
              # really old events, don't associate them with the new one
              step = 0.1
          else:
              step = dtime
          step /= len(self.motions)+1
          for data_old in self.motions:
              self.doc.stroke_to(step, *data_old)
              dtime -= step
          self.motions = []
      self.doc.stroke_to(dtime, *data)
  */
  if (!button1_pressed)
    curr_coords.pressure = 0.0;

  gimp_draw_tool_pause (GIMP_DRAW_TOOL (tool));

  core->stroke_to(drawable, dtime, &curr_coords, paint_options);

  gimp_projection_finish_draw (gimp_image_get_projection (image)); 
  gimp_projection_flush (gimp_image_get_projection (image));
  gimp_display_flush_now (display);
  paint_tool->last_flush_time = time;

  gimp_draw_tool_resume (GIMP_DRAW_TOOL (tool));
}

static void
gimp_mypaint_tool_modifier_key (GimpTool        *tool,
                              GdkModifierType  key,
                              gboolean         press,
                              GdkModifierType  state,
                              GimpDisplay     *display)
{
  GimpMypaintTool *paint_tool = GIMP_MYPAINT_TOOL (tool);
  GimpDrawTool  *draw_tool  = GIMP_DRAW_TOOL (tool);

  if (key != gimp_get_constrain_behavior_mask ())
    return;

  if (press) {
    GimpToolInfo *info = gimp_get_tool_info (display->gimp,
                                             "gimp-color-picker-tool");

    if (GIMP_IS_TOOL_INFO (info)) {
        if (gimp_draw_tool_is_active (draw_tool))
          gimp_draw_tool_stop (draw_tool);

        gimp_color_tool_enable (GIMP_COLOR_TOOL (tool),
                                GIMP_COLOR_OPTIONS (info->tool_options));

        switch (GIMP_COLOR_TOOL (tool)->pick_mode) {
          case GIMP_COLOR_PICK_MODE_FOREGROUND:
            gimp_tool_push_status (tool, display,
                                   _("Click in any image to pick the "
                                     "foreground color"));
            break;

          case GIMP_COLOR_PICK_MODE_BACKGROUND:
            gimp_tool_push_status (tool, display,
                                   _("Click in any image to pick the "
                                     "background color"));
            break;

          default:
            break;
        }
    }
  } else if (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (tool))) {
    gimp_tool_pop_status (tool, display);
    gimp_color_tool_disable (GIMP_COLOR_TOOL (tool));
  }
}

static void
gimp_mypaint_tool_cursor_update (GimpTool         *tool,
                               const GimpCoords *coords,
                               GdkModifierType   state,
                               GimpDisplay      *display)
{
  GimpCursorModifier  modifier;
  GimpCursorModifier  toggle_modifier;
  GimpCursorModifier  old_modifier;
  GimpCursorModifier  old_toggle_modifier;

  modifier        = tool->control->cursor_modifier;
  toggle_modifier = tool->control->toggle_cursor_modifier;

  old_modifier        = modifier;
  old_toggle_modifier = toggle_modifier;

  if (! gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (tool))) {
    GimpImage    *image    = gimp_display_get_image (display);
    GimpDrawable *drawable = gimp_image_get_active_drawable (image);

    if (! gimp_item_is_editable (GIMP_ITEM (drawable)) ||
      gimp_item_is_content_locked (GIMP_ITEM (drawable))) {
      modifier        = GIMP_CURSOR_MODIFIER_BAD;
      toggle_modifier = GIMP_CURSOR_MODIFIER_BAD;
    }

    gimp_tool_control_set_cursor_modifier        (tool->control,
                                                  modifier);
    gimp_tool_control_set_toggle_cursor_modifier (tool->control,
                                                  toggle_modifier);
  }

  GIMP_TOOL_CLASS (parent_class)->cursor_update (tool, coords, state,
                                                 display);

  /*  reset old stuff here so we are not interferring with the modifiers
   *  set by our subclasses
   */
  gimp_tool_control_set_cursor_modifier        (tool->control,
                                                old_modifier);
  gimp_tool_control_set_toggle_cursor_modifier (tool->control,
                                                old_toggle_modifier);
}

static void
gimp_mypaint_tool_oper_update (GimpTool         *tool,
                             const GimpCoords *coords,
                             GdkModifierType   state,
                             gboolean          proximity,
                             GimpDisplay      *display)
{
  GimpMypaintTool    *paint_tool    = GIMP_MYPAINT_TOOL (tool);
  GimpDrawTool     *draw_tool     = GIMP_DRAW_TOOL (tool);
  GimpMypaintOptions *paint_options = GIMP_MYPAINT_TOOL_GET_OPTIONS (tool);
  GimpMypaintCore    *core          = reinterpret_cast<GimpMypaintCore*>(paint_tool->core);
  GimpDisplayShell *shell         = gimp_display_get_shell (display);
  GimpImage        *image         = gimp_display_get_image (display);
  GimpDrawable     *drawable      = gimp_image_get_active_drawable (image);

  if (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (tool)))
    {
      GIMP_TOOL_CLASS (parent_class)->oper_update (tool, coords, state,
                                                   proximity, display);
      return;
    }
#if 0
  gimp_draw_tool_pause (draw_tool);

  timeval tv;
  gettimeofday(&tv, NULL);
  guint32 time = (guint32)((tv.tv_sec) * 1000.0 + (tv.tv_usec / 1000.0));
  g_print("time=%d\n", time);
  gimp_mypaint_tool_motion_internal (tool, coords, time, state, display, FALSE);
#endif
#if 0
  if (gimp_draw_tool_is_active (draw_tool) &&
      draw_tool->display != display)
    gimp_draw_tool_stop (draw_tool);

  gimp_tool_pop_status (tool, display);

  if (tool->display            &&
      tool->display != display &&
      gimp_display_get_image (tool->display) == image)
    {
      /*  if this is a different display, but the same image, HACK around
       *  in tool internals AFTER stopping the current draw_tool, so
       *  straight line drawing works across different views of the
       *  same image.
       */

      tool->display = display;
    }

  if (drawable && proximity)
    {
      gboolean constrain_mask = gimp_get_constrain_behavior_mask ();

      if (display == tool->display && (state & GDK_SHIFT_MASK))
        {
          /*  If shift is down and this is not the first paint stroke,
           *  draw a line.
           */

          gchar   *status_help;
          gdouble  dx, dy, dist;
          gint     off_x, off_y;

          paint_tool->curr_coords = *coords;

          gimp_item_get_offset (GIMP_ITEM (drawable), &off_x, &off_y);

          paint_tool->curr_coords.x -= off_x;
          paint_tool->curr_coords.y -= off_y;

//          gimp_mypaint_core_round_line (core, paint_options,
//                                      (state & constrain_mask) != 0);

          dx = paint_tool->curr_coords.x - paint_tool->last_event_x;
          dy = paint_tool->curr_coords.y - paint_tool->last_event_y;

          status_help = gimp_suggest_modifiers (paint_tool->status_line,
                                                GdkModifierType(constrain_mask & ~state),
                                                NULL,
                                                _("%s for constrained angles"),
                                                NULL);

          /*  show distance in statusbar  */
          if (shell->unit == GIMP_UNIT_PIXEL)
            {
              dist = sqrt (SQR (dx) + SQR (dy));

              gimp_tool_push_status (tool, display, "%.1f %s.  %s",
                                     dist, _("pixels"), status_help);
            }
          else
            {
              gdouble xres;
              gdouble yres;
              gchar   format_str[64];

              gimp_image_get_resolution (image, &xres, &yres);

              g_snprintf (format_str, sizeof (format_str), "%%.%df %s.  %%s",
                          gimp_unit_get_digits (shell->unit),
                          gimp_unit_get_symbol (shell->unit));

              dist = (gimp_unit_get_factor (shell->unit) *
                      sqrt (SQR (dx / xres) +
                            SQR (dy / yres)));

              gimp_tool_push_status (tool, display, format_str,
                                     dist, status_help);
            }

          g_free (status_help);

          paint_tool->draw_line = TRUE;
        }
      else
        {
          gchar           *status;
          GdkModifierType  modifiers = GdkModifierType(0);

          /* HACK: A paint tool may set status_ctrl to NULL to indicate that
           * it ignores the Ctrl modifier (temporarily or permanently), so
           * it should not be suggested.  This is different from how
           * gimp_suggest_modifiers() would interpret this parameter.
           */
          if (paint_tool->status_ctrl != NULL)
            modifiers = GdkModifierType(modifiers|constrain_mask);

          /* suggest drawing lines only after the first point is set
           */
          if (display == tool->display)
            modifiers = GdkModifierType(modifiers|GDK_SHIFT_MASK);

          status = gimp_suggest_modifiers (paint_tool->status,
                                           GdkModifierType(modifiers & ~state),
                                           _("%s for a straight line"),
                                           paint_tool->status_ctrl,
                                           NULL);
          gimp_tool_push_status (tool, display, "%s", status);
          g_free (status);

          paint_tool->draw_line = FALSE;
        }

      if (! gimp_draw_tool_is_active (draw_tool))
        gimp_draw_tool_start (draw_tool, display);
    }
  else if (gimp_draw_tool_is_active (draw_tool))
    {
      gimp_draw_tool_stop (draw_tool);
    }
#endif
  GIMP_TOOL_CLASS (parent_class)->oper_update (tool, coords, state, proximity,
                                               display);
#if 0
  gimp_draw_tool_resume (draw_tool);
#endif
}

static void
gimp_mypaint_tool_draw (GimpDrawTool *draw_tool)
{
  if (! gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (draw_tool)))
    {
      GimpMypaintTool *paint_tool = GIMP_MYPAINT_TOOL (draw_tool);

      if (paint_tool->draw_line &&
          ! gimp_tool_control_is_active (GIMP_TOOL (draw_tool)->control))
        {
          GimpMypaintCore *core     = reinterpret_cast<GimpMypaintCore*>(paint_tool->core);
          GimpImage     *image      = gimp_display_get_image (draw_tool->display);
          GimpDrawable  *drawable   = gimp_image_get_active_drawable (image);
          gint           off_x, off_y;

          gimp_item_get_offset (GIMP_ITEM (drawable), &off_x, &off_y);

          /*  Draw the line between the start and end coords  */
          gimp_draw_tool_add_line (draw_tool,
                                   paint_tool->last_event_x + off_x,
                                   paint_tool->last_event_y + off_y,
                                   paint_tool->curr_coords.x + off_x,
                                   paint_tool->curr_coords.y + off_y);

          /*  Draw start target  */
          gimp_draw_tool_add_handle (draw_tool,
                                     GIMP_HANDLE_CROSS,
                                     paint_tool->last_event_x + off_x,
                                     paint_tool->last_event_y + off_y,
                                     GIMP_TOOL_HANDLE_SIZE_CROSS,
                                     GIMP_TOOL_HANDLE_SIZE_CROSS,
                                     GIMP_HANDLE_ANCHOR_CENTER);

          /*  Draw end target  */
          gimp_draw_tool_add_handle (draw_tool,
                                     GIMP_HANDLE_CROSS,
                                     paint_tool->curr_coords.x + off_x,
                                     paint_tool->curr_coords.y + off_y,
                                     GIMP_TOOL_HANDLE_SIZE_CROSS,
                                     GIMP_TOOL_HANDLE_SIZE_CROSS,
                                     GIMP_HANDLE_ANCHOR_CENTER);
        }
    }

  GIMP_DRAW_TOOL_CLASS (parent_class)->draw (draw_tool);
}

static void
gimp_mypaint_tool_hard_notify (GimpMypaintOptions *options,
                             const GParamSpec *pspec,
                             GimpTool         *tool)
{
  gimp_tool_control_set_precision (tool->control,
                                   GIMP_CURSOR_PRECISION_SUBPIXEL);
}


void
gimp_mypaint_tool_register (GimpToolRegisterCallback  callback,
                            gpointer                  data)
{
  (* callback) (GIMP_TYPE_MYPAINT_TOOL,
                GIMP_TYPE_MYPAINT_OPTIONS,
                gimp_mypaint_options_gui,
                gimp_mypaint_options_gui_horizontal,
                GimpContextPropMask(GIMP_MYPAINT_OPTIONS_CONTEXT_MASK),
                "gimp-mypaint-tool",
                _("Mypaint Brush"),
                _("Mypaint Brush Tool: Paint using a mypaint brush"),
                N_("_Mypaintbrush"), "P",
                NULL, GIMP_HELP_TOOL_MYPAINT,
                GIMP_STOCK_TOOL_MYPAINT,
                data);
}


}
