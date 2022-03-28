/* GIMP-painter
 * Copyright (C) 2022 seagetch
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
#include "config.h"

#include <glib-object.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "libgimpwidgets/gimpwidgets.h"

#include "core/gimptooloptions.h"

#include "widgets/gimppropwidgets.h"
#include "widgets/gimppopupbutton.h"
#include "widgets/gimpspinscale.h"

#include "base/glib-cxx-utils.hpp"
#include "gimptooloptions-gui-cxx.hpp"

GtkWidget * 
gimp_tool_options_button_with_popup (GtkWidget*                label_widget,
                                     PopupCreateViewDelegator* create_view)
{
  GtkWidget *result = NULL;

  gtk_widget_show (label_widget);
  result = gimp_popup_button_new (label_widget);
  g_signal_connect_delegator (G_OBJECT(result), "create-view", create_view);

  return result;
}
