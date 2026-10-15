/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * GTK3 port of the independent horizontal gimp_transform_options_gui_full()
 * and gimp_transform_options_create_view() at afa43fae3e920210146abed514f136fd49f671b5.
 * The target/direction controls stay inline; settings live in a details popup.
 * Do not call the dock constructor: it writes type_box/direction_frame and the
 * transform tool owns those pointers. Both views bind to the same options.
 */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpwidgets/gimpwidgets.h"
#include "tools-types.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "widgets/gimppropwidgets.h"
#include "widgets/gimpwidgets-utils.h"
#include "gimptransformgridoptions.h"
#include "gimppaintertransformtoolbar.h"
#include "gimp-intl.h"

static void
pack (GtkWidget *box, GtkWidget *widget)
{
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
}

static GtkWidget *
combo (GObject *config, const gchar *property, const gchar *label)
{
  GtkWidget *widget = gimp_prop_enum_combo_box_new (config, property, 0, 0);
  gimp_int_combo_box_set_label (GIMP_INT_COMBO_BOX (widget), label);
  return widget;
}

static gboolean
grid_sensitive (GBinding *binding, const GValue *source, GValue *target,
                gpointer data)
{
  GimpGuidesType type = g_value_get_enum (source);
  g_value_set_boolean (target, type == GIMP_GUIDES_N_LINES || type == GIMP_GUIDES_SPACING);
  return TRUE;
}

GtkWidget *
gimp_painter_transform_toolbar_new (GimpToolOptions *options)
{
  GObject *config;
  const gchar *name;
  GtkWidget *root;
  GtkWidget *details;
  GtkWidget *preview;
  GtkWidget *button;
  GtkWidget *popover;
  GtkWidget *widget;
  gchar *label;

  g_return_val_if_fail (GIMP_IS_TOOL_OPTIONS (options), NULL);
  if (! options->tool_info)
    return NULL;
  config = G_OBJECT (options);
  name = gimp_object_get_name (options->tool_info);

  /* These are the original grid-transform tools. Newer tool-specific modes
   * (3D, handle and unified transforms) are not represented as completed. */
  if (g_strcmp0 (name, "gimp-rotate-tool") &&
      g_strcmp0 (name, "gimp-scale-tool") &&
      g_strcmp0 (name, "gimp-shear-tool") &&
      g_strcmp0 (name, "gimp-perspective-tool"))
    return NULL;
  g_return_val_if_fail (GIMP_IS_TRANSFORM_GRID_OPTIONS (options), NULL);

  root = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  g_object_set_data_full (G_OBJECT (root), "gimp-painter-toolbar-options",
                          g_object_ref (options), g_object_unref);
  pack (root, gtk_label_new (_("Transform:")));
  pack (root, gimp_prop_enum_icon_box_new (config, "type", "gimp", 0, 0));
  pack (root, gimp_prop_enum_radio_frame_new_with_orientation
                 (config, "direction", _("Direction"), 0, 0, GTK_ORIENTATION_HORIZONTAL));

  button = gtk_menu_button_new ();
  gtk_button_set_label (GTK_BUTTON (button), _("Transformation details…"));
  pack (root, button);
  popover = gtk_popover_new (button);
  details = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_set_border_width (GTK_CONTAINER (details), 8);
  gtk_container_add (GTK_CONTAINER (popover), details);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);
  gtk_widget_show (details);

  pack (details, combo (config, "interpolation", _("Interpolation")));
  pack (details, combo (config, "clip", _("Clipping")));
  preview = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  pack (preview, gimp_prop_check_button_new (config, "composited-preview", NULL));
  widget = gimp_prop_check_button_new (config, "synchronous-preview", NULL);
  pack (preview, widget);
  g_object_bind_property (config, "composited-preview", widget, "sensitive", G_BINDING_SYNC_CREATE);
  widget = gimp_prop_spin_scale_new (config, "preview-opacity", 0.01, 0.1, 1);
  gimp_prop_widget_set_factor (widget, 100.0, 1.0, 10.0, 1);
  pack (preview, widget);
  g_object_bind_property (config, "composited-preview", widget, "sensitive",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  pack (details, gimp_prop_expanding_frame_new (config, "show-preview", NULL, preview, NULL));
  pack (details, combo (config, "grid-type", _("Guides")));
  widget = gimp_prop_spin_scale_new (config, "grid-size", 1.0, 8.0, 0);
  pack (details, widget);
  g_object_bind_property_full (config, "grid-type", widget, "sensitive",
                               G_BINDING_SYNC_CREATE, grid_sensitive, NULL, NULL, NULL);

  if (! g_strcmp0 (name, "gimp-rotate-tool"))
    {
      label = g_strdup_printf (_("15 degrees (%s)"),
                               gimp_get_mod_string (gimp_get_extend_selection_mask ()));
      pack (details, gimp_prop_check_button_new (config, "constrain-rotate", label));
      g_free (label);
    }
  else if (! g_strcmp0 (name, "gimp-scale-tool"))
    {
      label = g_strdup_printf (_("Keep aspect (%s)"),
                               gimp_get_mod_string (gimp_get_extend_selection_mask ()));
      pack (details, gimp_prop_check_button_new (config, "constrain-scale", label));
      g_free (label);
      pack (details, gimp_prop_check_button_new (config, "frompivot-scale", _("Around center")));
    }
  else if (! g_strcmp0 (name, "gimp-perspective-tool"))
    {
      pack (details, gimp_prop_check_button_new (config, "constrain-perspective", _("Constrain handles")));
      pack (details, gimp_prop_check_button_new (config, "frompivot-perspective", _("Around center")));
    }

  return root;
}
