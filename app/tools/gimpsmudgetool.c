/* GIMP - The GNU Image Manipulation Program
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

#include "config.h"

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "paint/gimpsmudgeoptions.h"

#include "widgets/gimphelp-ids.h"
#include "widgets/gimppropwidgets.h"

#include "gimpsmudgetool.h"
#include "gimptooloptions-gui.h"
#include "gimppaintoptions-gui.h"
#include "gimptoolcontrol.h"

#include "gimp-intl.h"


static GtkWidget * gimp_smudge_options_gui (GimpToolOptions *tool_options);
static GtkWidget * gimp_smudge_options_gui_horizontal (GimpToolOptions *tool_options);
static GtkWidget * gimp_smudge_options_gui_full (GimpToolOptions *tool_options, gboolean horizontal);


G_DEFINE_TYPE (GimpSmudgeTool, gimp_smudge_tool, GIMP_TYPE_BRUSH_TOOL)


void
gimp_smudge_tool_register (GimpToolRegisterCallback  callback,
                           gpointer                  data)
{
  (* callback) (GIMP_TYPE_SMUDGE_TOOL,
                GIMP_TYPE_SMUDGE_OPTIONS,
                gimp_smudge_options_gui,
                gimp_smudge_options_gui_horizontal,
                GIMP_PAINT_OPTIONS_CONTEXT_MASK,
                "gimp-smudge-tool",
                _("Smudge"),
                _("Smudge Tool: Smudge selectively using a brush"),
                N_("_Smudge"), "S",
                NULL, GIMP_HELP_TOOL_SMUDGE,
                GIMP_STOCK_TOOL_SMUDGE,
                data);
}

static void
gimp_smudge_tool_class_init (GimpSmudgeToolClass *klass)
{
}

static void
gimp_smudge_tool_init (GimpSmudgeTool *smudge)
{
  GimpTool      *tool       = GIMP_TOOL (smudge);
  GimpPaintTool *paint_tool = GIMP_PAINT_TOOL (smudge);

  gimp_tool_control_set_tool_cursor (tool->control, GIMP_TOOL_CURSOR_SMUDGE);

  paint_tool->status      = _("Click to smudge");
  paint_tool->status_line = _("Click to smudge the line");
  paint_tool->status_ctrl = NULL;
}


/*  tool options stuff  */

static GtkWidget *
gimp_smudge_options_gui (GimpToolOptions *tool_options)
{
  return gimp_smudge_options_gui_full (tool_options, FALSE);
}

static GtkWidget *
gimp_smudge_options_gui_horizontal (GimpToolOptions *tool_options)
{
  return gimp_smudge_options_gui_full (tool_options, TRUE);
}

static GtkWidget *
gimp_smudge_options_gui_full (GimpToolOptions *tool_options, gboolean horizontal)
{
  GObject   *config = G_OBJECT (tool_options);
  GtkWidget *vbox   = gimp_paint_options_gui_full (tool_options, horizontal);
  GtkWidget *scale;
  GList     *children;

  /*  the rate scale  */
  scale = gimp_prop_spin_scale_new (config, "rate",
                                    _("Rate"),
                                    1.0, 10.0, 1);
  gtk_box_pack_start (GTK_BOX (vbox), scale, FALSE, FALSE, 0);
  gtk_widget_show (scale);

  children = gtk_container_get_children (GTK_CONTAINER (vbox));  
  gimp_tool_options_setup_popup_layout (children, FALSE);

  return vbox;
}
