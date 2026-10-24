/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "tools/gimpmypaintoptions-gui.h"
GtkWidget *options_gui_c_consumer (GimpToolOptions *options, gboolean horizontal);
GtkWidget *options_gui_c_consumer (GimpToolOptions *options, gboolean horizontal)
{
  GtkWidget *(*vertical_entry) (GimpToolOptions *) = gimp_mypaint_options_gui;
  GtkWidget *(*horizontal_entry) (GimpToolOptions *) = gimp_mypaint_options_gui_horizontal;
  return horizontal ? horizontal_entry (options) : vertical_entry (options);
}
