
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
#include <functional>

extern "C"{
#include "config.h"

#include <string.h>

#include <gegl.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "display-types.h"
#include "tools/tools-types.h"

#include "config/gimpcoreconfig.h"
#include "config/gimpdisplayconfig.h"
#include "config/gimpdisplayoptions.h"

#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-grid.h"
#include "core/gimpimage-guides.h"
#include "core/gimpimage-snap.h"
#include "core/gimpprojection.h"
#include "core/gimpmarshal.h"
#include "core/gimptemplate.h"

#include "widgets/gimpdevices.h"
#include "widgets/gimpdock.h" /* gimp-painter 2.8 */
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpuimanager.h"
#include "widgets/gimpwidgets-utils.h"
#include "widgets/gimplayertileview.h" /* gimp-painter 2.8 */
#include "widgets/gimptooltileview.h" /* gimp-painter 2.8 */
#include "widgets/gimptooloptionstoolbar.h" /* gimp-painter 2.8 */
#include "widgets/gimpfgbgeditor.h"
#include "widgets/gimpoverlayframe.h" /* gimp-painter 2.8 */
#include "widgets/gimpoverlaybox.h" /* gimp-painter 2.8 */
#include "widgets/gimpoverlaychild.h" /* gimp-painter 2.8 */

#include "menus/menus.h"
#include "tools/tool_manager.h"

#include "gimpcanvas.h"
#include "gimpcanvaslayerboundary.h"
#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-appearance.h"
#include "gimpdisplayshell-callbacks.h"
#include "gimpdisplayshell-cursor.h"
#include "gimpdisplayshell-dnd.h"
#include "gimpdisplayshell-expose.h"
#include "gimpdisplayshell-filter.h"
#include "gimpdisplayshell-handlers.h"
#include "gimpdisplayshell-items.h"
#include "gimpdisplayshell-overlays.h" /* gimp-painter 2.8 */
#include "gimpdisplayshell-progress.h"
#include "gimpdisplayshell-render.h"
#include "gimpdisplayshell-scale.h"
#include "gimpdisplayshell-scroll.h"
#include "gimpdisplayshell-selection.h"
#include "gimpdisplayshell-title.h"
#include "gimpdisplayshell-tool-events.h"
#include "gimpdisplayshell-transform.h"
#include "gimpimagewindow.h"
#include "gimpmotionbuffer.h"
#include "gimpstatusbar.h"

#include "about.h"
#include "gimp-log.h"
}

using namespace GLib;

class OverlayWidgetDecorator {
public:
  GimpDisplayShell* shell;
  GtkWidget*        widget;
  GtkWidget**       placeholder;
  gdouble opacity;
  CXXPointer<Delegators::Connection> connection;
  typedef OverlayWidgetDecorator Self;
  typedef GLib::Decorator<GtkWidget, Self> Decorator;

  std::function<void(GimpDisplayShell* shell)> updator;

  OverlayWidgetDecorator(GtkWidget* widget, GimpDisplayShell* shell, GtkWidget** placeholder, gdouble opacity = 0.85) 
    : updator(nullptr) 
  {
    g_return_if_fail(GTK_IS_WIDGET(widget));
    g_return_if_fail(GIMP_IS_DISPLAY_SHELL(shell));
    this->widget      = widget;
    this->shell       = shell;
    this->opacity     = opacity;
    this->placeholder = placeholder;
    gtk_widget_set_events(widget, GDK_ALL_EVENTS_MASK);
    connection = g_signal_connect_delegator (G_OBJECT(widget), "size-allocate", Delegators::delegator(this, &Self::on_size_allocate));
    g_signal_connect_delegator_noret (G_OBJECT(widget), "enter-notify-event", Delegators::delegator(this, &Self::on_enter_notify));
    g_signal_connect_delegator_noret (G_OBJECT(widget), "leave-notify-event", Delegators::delegator(this, &Self::on_leave_notify));
    decorator(widget, this);
  };

  ~OverlayWidgetDecorator() {
  };

  void on_size_allocate (GtkWidget* widget, GtkAllocation* allocation) {
    update();
  }

  gboolean on_enter_notify (GtkWidget* widget, GdkEventCrossing* evnet) {
    g_print("enter_notify\n");
    gimp_overlay_box_set_child_opacity (GIMP_OVERLAY_BOX (shell->canvas), widget, 1.0);
    return FALSE;
  }

  gboolean on_leave_notify (GtkWidget* widget, GdkEventCrossing* evnet) {
    g_print("leave_notify\n");
    gimp_overlay_box_set_child_opacity (GIMP_OVERLAY_BOX (shell->canvas), widget, opacity);
    return FALSE;
  }

  void reparent() {
    GtkWidget* old_parent;
    g_return_if_fail (widget != NULL);
    g_return_if_fail (GIMP_IS_DISPLAY_SHELL(shell));
    g_return_if_fail (GIMP_IS_CANVAS(shell->canvas));

    old_parent = gtk_widget_get_parent (widget);
    if (old_parent != shell->canvas) {
      if (old_parent) {
        g_object_ref (G_OBJECT (widget));
        if (connection) {
          connection = NULL;
        }
        gtk_container_remove(GTK_CONTAINER(old_parent), widget);
      }
      gimp_overlay_box_add_child (GIMP_OVERLAY_BOX (shell->canvas), widget, 0.0, 0.0);
      if (old_parent)
        g_object_unref (G_OBJECT (widget));

      connection = g_signal_connect_delegator (G_OBJECT(widget), "size-allocate", Delegators::delegator(this, &Self::on_size_allocate));

      gimp_overlay_box_set_child_opacity (GIMP_OVERLAY_BOX (shell->canvas), widget, opacity);
    }  

  }

  void update() {
    reparent();
    if (updator)
      updator(shell);
  }

  void dispose() {
    GimpImage* image;
    image = gimp_display_get_image (shell->display);

    if (!image) {
      GtkWidget* old_parent = gtk_widget_get_parent (widget);
      gtk_container_remove(GTK_CONTAINER(old_parent), widget);
      if (placeholder)
        *placeholder = NULL;
      return;
    }
  }

  bool get_position(gint* x, gint* y) {
    GimpOverlayChild* child = gimp_overlay_child_find (GIMP_OVERLAY_BOX (shell->canvas), widget);
    if (child) {
      *x = child->x;
      *y = child->y;
      return true;
    }
    return false;
  }

  void update_opacity(gint x, gint y, gboolean released) {
    if (released) {
        gimp_overlay_box_set_child_opacity (GIMP_OVERLAY_BOX (shell->canvas), widget, opacity);
        return;
    }
    gint wx, wy;
    GtkAllocation alloc, alloc2;
    gtk_widget_get_allocation(shell->canvas, &alloc);
    gtk_widget_get_allocation(widget, &alloc2);
    if (get_position(&wx, &wy)) {
      gint dx, dy;
      dx = (wx <= x && x <= wx + alloc2.width )? 0: MIN(ABS(wx + alloc2.width - x), ABS(wx - x));
      dy = (wy <= y && y <= wy + alloc2.height)? 0: MIN(ABS(wy + alloc2.height - y), ABS(wy - y));
      gint distance = dx * dx + dy * dy;

      if (distance < 200 * 200) {
        gimp_overlay_box_set_child_opacity (GIMP_OVERLAY_BOX (shell->canvas), widget, 0.1);
      } else if (distance > 400 * 400)
        gimp_overlay_box_set_child_opacity (GIMP_OVERLAY_BOX (shell->canvas), widget, opacity);
    }
  }


  static void dispose_obj(GtkWidget* widget) {
    Decorator::call(widget, &Self::dispose);
  }

  static void update_obj(GtkWidget* widget) {
    Decorator::call(widget, &Self::update);
  }

  static void update_opacity_obj(GtkWidget* widget, gint x, gint y, gboolean released) {
    Decorator::call<void, gint, gint, gboolean>(widget, &Self::update_opacity, x, y, released);
  }
};

class ColorSelectorDecorator {
  GimpDisplayShell* shell;
  GtkWidget*        widget;
  bool              blocked;
  CXXPointer<Delegators::Connection> fg_color_changed_handler;
  CXXPointer<Delegators::Connection> bg_color_changed_handler;
  typedef ColorSelectorDecorator Self;

public:
  ColorSelectorDecorator(GtkWidget* widget, GimpDisplayShell* shell) {
    this->shell  = shell;
    this->widget = widget;
    blocked      = false;
    g_signal_connect_delegator_noret (G_OBJECT(widget), "color-changed", Delegators::delegator(this, &Self::on_select_color));

    GimpContext* user_context;
    user_context = gimp_get_user_context (shell->display->gimp);

    fg_color_changed_handler = g_signal_connect_delegator (G_OBJECT(user_context), "foreground-changed", Delegators::delegator(this, &Self::on_fg_color_changed));
    bg_color_changed_handler = g_signal_connect_delegator (G_OBJECT(user_context), "background-changed", Delegators::delegator(this, &Self::on_bg_color_changed));

    decorator(widget, this);
  };

  ~ColorSelectorDecorator() {
  };

  void on_select_color(GimpColorSelector* selector, const GimpRGB* rgb, const GimpHSV* hsv) {
    GimpContext* user_context;
    user_context = gimp_get_user_context (shell->display->gimp);

    blocked = true;
    gimp_context_set_foreground (user_context, rgb);
    blocked = false;
  };

  void on_fg_color_changed(GimpContext* context, const GimpRGB   *rgb) {
    if (blocked)
      return;

    GimpHSV hsv;

    gimp_rgb_to_hsv (rgb, &hsv);
    gimp_color_selector_set_color (GIMP_COLOR_SELECTOR(widget), rgb, &hsv);

//    gimp_color_hex_entry_set_color (GIMP_COLOR_HEX_ENTRY (editor->hex_entry),
//                                    rgb);

  };

  void on_bg_color_changed(GimpContext* context, const GimpRGB   *rgb) {
    if (blocked)
      return;
  }

};

static GtkWidget*
create_layer_view(GimpDisplayShell* shell)
{
  GimpContext* user_context;
  GtkWidget* layer_view;
  GtkWidget* frame;
  layer_view = GTK_WIDGET(gimp_layer_tile_view_new ());
  user_context = gimp_get_user_context (shell->display->gimp);
  g_object_set (layer_view, "context", user_context, NULL);

  frame = gimp_overlay_frame_new ();
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_container_add (GTK_CONTAINER(frame), layer_view);

  gtk_widget_show_all (frame);

  auto impl = new OverlayWidgetDecorator(frame, shell, &shell->layer_view);

  impl->updator = [](GimpDisplayShell* shell) {
    GtkAllocation alloc;
    GimpImage* image = gimp_display_get_image (shell->display);
    GimpImage* image1;
    GtkWidget* layer_view;
    layer_view = gtk_bin_get_child (GTK_BIN(shell->layer_view));
    /* Set appropriate image and context for layer view */
    g_object_get(layer_view, "image", &image1, NULL);
    if (image) {
      if (image1 != image) {
        g_object_set (layer_view, "image", image, NULL);
      }
    } else {
      g_object_set(layer_view, "image", NULL, NULL);
    }
    gtk_widget_get_allocation(GTK_WIDGET(shell->canvas), &alloc);

    /* Resize and set position of the layer view */
    gtk_widget_set_size_request(shell->layer_view, 88, alloc.height * 8 / 10);
    gimp_overlay_box_set_child_position (GIMP_OVERLAY_BOX (shell->canvas), shell->layer_view, alloc.width-84, alloc.height/10);
  };

  return frame;
}


static GtkWidget*
create_toolbox (GimpDisplayShell* shell)
{
  GtkWidget* toolbox;
  GtkWidget* frame;
  toolbox = GTK_WIDGET(gimp_tool_tile_view_new ());
  g_object_set (toolbox, "context", gimp_get_user_context (shell->display->gimp), NULL);

  frame = gimp_overlay_frame_new ();
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_container_add (GTK_CONTAINER(frame), toolbox);

  gtk_widget_show_all (frame);

  auto impl = new OverlayWidgetDecorator(frame, shell, &shell->toolbox);

  impl->updator = [](GimpDisplayShell* shell) {
    GtkAllocation alloc, alloc2;
    GimpImage* image = gimp_display_get_image (shell->display);
    gtk_widget_get_allocation(GTK_WIDGET(shell->canvas), &alloc);
    gtk_widget_get_allocation(GTK_WIDGET (shell->toolbox), &alloc2);
    gimp_overlay_box_set_child_position (GIMP_OVERLAY_BOX (shell->canvas), shell->toolbox, -4, (alloc.height - alloc2.height) / 2);      
  };

  return frame;
}


static GtkWidget*
create_toolbar (GimpDisplayShell* shell)
{
  GtkWidget* toolbar;
  GtkWidget* frame;
  toolbar = gimp_tool_options_toolbar_new (shell->display->gimp, global_menu_factory);
  frame = gimp_overlay_frame_new ();
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_container_add (GTK_CONTAINER(frame), toolbar);

  gtk_widget_show_all (frame);
  
  auto impl = new OverlayWidgetDecorator(frame, shell, &shell->toolbar);

  impl->updator = [](GimpDisplayShell* shell) {
    GtkAllocation alloc, alloc2;
    GimpImage* image = gimp_display_get_image (shell->display);
    gtk_widget_get_allocation(GTK_WIDGET(shell->canvas), &alloc);
    GtkWidget* toolbar = gtk_bin_get_child (GTK_BIN(shell->toolbar));
    gtk_widget_set_size_request(toolbar, alloc.width * 0.9, -1);
    gtk_widget_get_allocation(GTK_WIDGET (shell->toolbar), &alloc2);
    gimp_overlay_box_set_child_position (GIMP_OVERLAY_BOX (shell->canvas), shell->toolbar, (alloc.width - alloc2.width) / 2, (alloc.height - alloc2.height)+4);
  };

  return frame;
}


static GtkWidget*
create_color_selector (GimpDisplayShell* shell)
{
  GtkWidget* frame;
  GimpRGB rgb;
  GimpHSV hsv;
  GtkWidget* color_selector;

  GimpContext* user_context;
  user_context = gimp_get_user_context (shell->display->gimp);

  gimp_context_get_foreground (user_context, &rgb);
  gimp_rgb_to_hsv (&rgb, &hsv);

  color_selector = gimp_color_selector_new (GIMP_TYPE_COLOR_NOTEBOOK, &rgb, &hsv, GIMP_COLOR_SELECTOR_HUE);
  frame = gimp_overlay_frame_new ();
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_container_add (GTK_CONTAINER(frame), color_selector);
  gtk_widget_show_all (frame);
  auto impl = new OverlayWidgetDecorator(frame, shell, &shell->color_selector, 1.0);

  impl->updator = [](GimpDisplayShell* shell) {
    GtkAllocation alloc, alloc2, alloc3, alloc4, alloc5;
    GimpImage* image = gimp_display_get_image (shell->display);
    gtk_widget_get_allocation(GTK_WIDGET(shell->canvas), &alloc);
    GtkWidget* color_selector = gtk_bin_get_child (GTK_BIN(shell->color_selector));
    gtk_widget_set_size_request(color_selector, 256, 256);

    gtk_widget_get_allocation(GTK_WIDGET (shell->color_selector), &alloc2);
    gtk_widget_get_allocation(GTK_WIDGET (shell->layer_view), &alloc3);
    gtk_widget_get_allocation(GTK_WIDGET (shell->fg_bg_edit), &alloc4);

    gimp_overlay_box_set_child_position (GIMP_OVERLAY_BOX (shell->canvas), 
        shell->color_selector, alloc.width - alloc2.width - alloc4.width, 
        0);
//        (alloc.height - alloc3.height) / 2 - alloc4.height - 4);

  };

  auto impl2 = new ColorSelectorDecorator(color_selector, shell);

  return frame;
}

static GtkWidget*
create_fg_bg_edit (GimpDisplayShell* shell)
{
  GtkWidget* frame;
  GtkWidget* fg_bg_edit;

  GimpContext* user_context;
  user_context = gimp_get_user_context (shell->display->gimp);

 // gimp_context_get_foreground (user_context, &rgb);
 // gimp_rgb_to_hsv (&rgb, &hsv);

  fg_bg_edit = gimp_fg_bg_editor_new (user_context);
  frame = gimp_overlay_frame_new ();
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_container_add (GTK_CONTAINER(frame), fg_bg_edit);
  gtk_widget_show_all (frame);
  auto impl = new OverlayWidgetDecorator(frame, shell, &shell->fg_bg_edit, 1.0);

  impl->updator = [](GimpDisplayShell* shell) {
    GtkAllocation alloc, alloc2, alloc3;
    GimpImage* image = gimp_display_get_image (shell->display);
    gtk_widget_get_allocation(GTK_WIDGET(shell->canvas), &alloc);
    GtkWidget* fg_bg_edit = gtk_bin_get_child (GTK_BIN(shell->fg_bg_edit));
    gtk_widget_set_size_request(fg_bg_edit, 80, 48);

    gtk_widget_get_allocation(GTK_WIDGET (shell->fg_bg_edit), &alloc2);
    gtk_widget_get_allocation(GTK_WIDGET (shell->layer_view), &alloc3);

    gimp_overlay_box_set_child_position (GIMP_OVERLAY_BOX (shell->canvas), shell->fg_bg_edit, 
        alloc.width - 84, 0);
//        (alloc.height - alloc3.height) / 2 - alloc2.height - 4);

  };

  return frame;
}


void
gimp_display_shell_update_on_canvas_views (GimpDisplayShell* shell) 
{
  GimpImage* image;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL(shell));
  g_return_if_fail (GIMP_IS_DISPLAY(shell->display));

  image = gimp_display_get_image (shell->display);
  if (!image) {
    OverlayWidgetDecorator::dispose_obj(shell->layer_view);
    OverlayWidgetDecorator::dispose_obj(shell->toolbox);
    OverlayWidgetDecorator::dispose_obj(shell->toolbar);
    OverlayWidgetDecorator::dispose_obj(shell->fg_bg_edit);
    OverlayWidgetDecorator::dispose_obj(shell->color_selector);
    // shell->docks should not be disposed.
    return;
  }

  if (!shell->layer_view) {
    shell->layer_view = create_layer_view(shell);
  }

  if (!shell->toolbox) {
    shell->toolbox = create_toolbox(shell);
  }

  if (!shell->toolbar) {
    shell->toolbar = create_toolbar(shell);
  }

  if (!shell->fg_bg_edit) {
#if 1
    shell->fg_bg_edit = create_fg_bg_edit(shell);
#endif
  }

  if (!shell->color_selector) {
#if 1
    shell->color_selector = create_color_selector(shell);
#endif
  }

  OverlayWidgetDecorator::update_obj(shell->layer_view);
  OverlayWidgetDecorator::update_obj(shell->toolbox);
  OverlayWidgetDecorator::update_obj(shell->toolbar);
  OverlayWidgetDecorator::update_obj(shell->fg_bg_edit);
  OverlayWidgetDecorator::update_obj(shell->color_selector);
  OverlayWidgetDecorator::update_obj(shell->docks);

}


void
gimp_display_shell_update_on_canvas_opacity (GimpDisplayShell* shell, gint x, gint y, gboolean released) 
{
  OverlayWidgetDecorator::update_opacity_obj(shell->layer_view, x, y, released);
  OverlayWidgetDecorator::update_opacity_obj(shell->toolbox, x, y, released);
  OverlayWidgetDecorator::update_opacity_obj(shell->toolbar, x, y, released);
  OverlayWidgetDecorator::update_opacity_obj(shell->fg_bg_edit, x, y, released);
  OverlayWidgetDecorator::update_opacity_obj(shell->color_selector, x, y, released);
  OverlayWidgetDecorator::update_opacity_obj(shell->docks, x, y, released);
}


void gimp_display_shell_attach_on_canvas_view (GimpDisplayShell* shell, GtkWidget* widget)
{
  if (!widget)
    return;
  shell->docks = widget;
  auto impl = new OverlayWidgetDecorator(shell->docks, shell, &shell->docks);
  impl->updator = [] (GimpDisplayShell* shell) {
    GtkAllocation alloc, alloc2, alloc3, alloc4;

    GimpImage* image = gimp_display_get_image (shell->display);
    if (!image) {
      gtk_widget_hide(shell->docks);
      g_print("hide and return\n");
      return;
    }
    gtk_widget_get_allocation(GTK_WIDGET(shell->canvas), &alloc);
    gtk_widget_get_allocation(GTK_WIDGET (shell->fg_bg_edit), &alloc2);
    gtk_widget_get_allocation(GTK_WIDGET (shell->layer_view), &alloc3);
    gtk_widget_get_allocation(GTK_WIDGET (shell->color_selector), &alloc4);

    if (alloc.width < alloc.height) {
      gimp_overlay_box_set_child_position (GIMP_OVERLAY_BOX (shell->canvas), shell->docks, 
          0,
          0);
  //        (alloc.height - alloc3.height) / 2 - alloc2.height - 4);

      gtk_widget_set_size_request (shell->docks,
          MAX(alloc.width - alloc4.width - alloc2.width - 4, 1), 
          MAX(alloc4.height, 1));

    } else {
      gint x, y;
      x = alloc.width - alloc3.width - alloc4.width - 4;
      y = alloc4.height + 4;
      gimp_overlay_box_set_child_position (GIMP_OVERLAY_BOX (shell->canvas), shell->docks, x,y);
      gtk_widget_set_size_request (shell->docks,
          MAX(alloc4.width, 1), 
          MAX(alloc.height - y, 1));

    }
    gtk_widget_show (shell->docks);
  };
}


void gimp_display_shell_detach_on_canvas_view (GimpDisplayShell* shell, GtkWidget* widget)
{
  GLib::undecorate<GtkWidget, OverlayWidgetDecorator>(widget);
  gtk_container_remove (GTK_CONTAINER(shell->canvas), widget);
  shell->docks = NULL;
}
