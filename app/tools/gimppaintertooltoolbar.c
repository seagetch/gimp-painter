/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 * Copyright (C) 2026 GIMP-Painter contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Independent horizontal tool controls, ported from the horizontal factories
 * in GIMP-Painter afa43fae3e920210146abed514f136fd49f671b5:
 * gimpmoveoptions, gimpselectionoptions, gimpregionselectoptions,
 * gimprectangleselectoptions, gimprectangleoptions, gimpflipoptions,
 * gimpmagnifyoptions, gimpcoloroptions, gimpcolorpickeroptions,
 * gimpbucketfilloptions, gimpblendoptions, gimpcageoptions,
 * gimpiscissorsoptions, gimpcropoptions, gimpmeasureoptions,
 * gimphistogramoptions, and gimpforegroundselectoptions.
 *
 * The old inline/details split is retained, with GTK3 popovers replacing the
 * legacy popup window helpers. All controls bind directly to the current
 * options object. In particular, no ordinary options-GUI constructor is
 * called, and no native options widget pointer is replaced. Multiple views
 * can therefore coexist with the dockable and with each other.
 */

#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "widgets/gimpcolorpanel.h"
#include "widgets/gimplayermodebox.h"
#include "widgets/gimppropwidgets.h"
#include "widgets/gimpviewablebox.h"
#include "widgets/gimpwidgets-constructors.h"
#include "widgets/gimpwidgets-utils.h"

#include "gimpbucketfilloptions.h"
#include "gimpcageoptions.h"
#include "gimpcolorpickeroptions.h"
#include "gimpcropoptions.h"
#include "gimpflipoptions.h"
#include "gimpforegroundselectoptions.h"
#include "gimpgradientoptions.h"
#include "gimphistogramoptions.h"
#include "gimpiscissorsoptions.h"
#include "gimpmagnifyoptions.h"
#include "gimpmeasureoptions.h"
#include "gimpmoveoptions.h"
#include "gimprectangleoptions.h"
#include "gimprectangleselectoptions.h"
#include "gimpregionselectoptions.h"
#include "gimpselectionoptions.h"
#include "gimppaintertooltoolbar.h"

#include "gimp-intl.h"


static GtkWidget *
painter_pack (GtkWidget *box,
              GtkWidget *widget)
{
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_widget_show (widget);
  return widget;
}

static GtkWidget *
painter_check (GtkWidget   *box,
               GObject     *config,
               const gchar *property,
               const gchar *label)
{
  return painter_pack (box, gimp_prop_check_button_new (config, property, label));
}

static GtkWidget *
painter_number (GtkWidget   *box,
                GObject     *config,
                const gchar *property,
                const gchar *label,
                gdouble      step,
                gint         digits)
{
  GtkWidget *row  = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *spin = gimp_prop_spin_button_new (config, property,
                                               step, step * 10.0, digits);

  painter_pack (row, gtk_label_new (label));
  gtk_entry_set_width_chars (GTK_ENTRY (spin), 6);
  painter_pack (row, spin);
  painter_pack (box, row);
  return row;
}

static GtkWidget *
painter_combo (GtkWidget   *box,
               GObject     *config,
               const gchar *property,
               const gchar *label,
               gint         minimum,
               gint         maximum)
{
  GtkWidget *combo = gimp_prop_enum_combo_box_new (config, property,
                                                   minimum, maximum);

  gimp_int_combo_box_set_label (GIMP_INT_COMBO_BOX (combo), label);
  g_object_set (combo, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  return painter_pack (box, combo);
}

static GtkWidget *
painter_details (GtkWidget   *box,
                 const gchar *label,
                 const gchar *name)
{
  GtkWidget *button  = gtk_menu_button_new ();
  GtkWidget *popover = gtk_popover_new (button);
  GtkWidget *scroll  = gtk_scrolled_window_new (NULL, NULL);
  GtkWidget *content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  gtk_button_set_label (GTK_BUTTON (button), label);
  gtk_widget_set_name (button, name);
  gtk_widget_set_tooltip_text (button, label);
  gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_TOP);
  gtk_container_set_border_width (GTK_CONTAINER (content), 8);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (scroll), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroll), TRUE);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroll), 420);
  gtk_container_add (GTK_CONTAINER (scroll), content);
  gtk_container_add (GTK_CONTAINER (popover), scroll);
  gtk_widget_show_all (scroll);
  /* GtkMenuButton applies its direction when attaching the popover, which
   * otherwise overwrites the TOP position above with its default BOTTOM. */
  gtk_menu_button_set_direction (GTK_MENU_BUTTON (button), GTK_ARROW_UP);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);
  painter_pack (box, button);
  return content;
}

static void
painter_sensitive (GObject     *config,
                   const gchar *property,
                   GtkWidget   *widget)
{
  g_object_bind_property (config, property, widget, "sensitive",
                          G_BINDING_SYNC_CREATE);
}

static GtkWidget *
painter_toggle_details (GtkWidget   *box,
                        GObject     *config,
                        const gchar *property,
                        const gchar *label,
                        const gchar *name)
{
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *content;

  painter_check (row, config, property, label);
  content = painter_details (row, "\342\226\276", name);
  gtk_widget_set_tooltip_text (gtk_popover_get_relative_to
                               (GTK_POPOVER (gtk_widget_get_ancestor (content, GTK_TYPE_POPOVER))),
                               label);
  painter_sensitive (config, property, content);
  painter_pack (box, row);
  return content;
}

static void
painter_move_notify (GObject    *config,
                     GParamSpec *pspec,
                     GtkWidget  *button)
{
  gint type;

  g_object_get (config, "move-type", &type, NULL);
  gtk_button_set_label (GTK_BUTTON (button),
                        type == GIMP_TRANSFORM_TYPE_PATH ? _("Move active path") :
                        type == GIMP_TRANSFORM_TYPE_SELECTION ? _("Move selection") :
                        _("Move selected layers"));
  gtk_widget_set_tooltip_text (button,
                               type == GIMP_TRANSFORM_TYPE_PATH ?
                               _("When disabled, pick a path on the canvas") :
                               _("When disabled, pick a layer or guide on the canvas"));
  gtk_widget_set_sensitive (button, type != GIMP_TRANSFORM_TYPE_SELECTION);
}

static void
painter_move (GtkWidget *box,
              GObject   *config)
{
  GtkWidget *button;

  painter_pack (box, gtk_label_new (_("Move:")));
  painter_pack (box, gimp_prop_enum_icon_box_new (config, "move-type", "gimp", 0, 0));
  button = painter_check (box, config, "move-current", _("Move selected layers"));
  g_signal_connect_object (config, "notify::move-type",
                            G_CALLBACK (painter_move_notify), button, 0);
  painter_move_notify (config, NULL, button);
}

static void
painter_fixed_rule_notify (GObject    *config,
                          GParamSpec *pspec,
                          GtkWidget  *row)
{
  gint rule;
  gint expected = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row),
                                                     "painter-fixed-rule"));

  g_object_get (config, "fixed-rule", &rule, NULL);
  gtk_widget_set_visible (row, rule == expected);
}

static void
painter_fixed_rule_row (GObject                *config,
                        GtkWidget              *row,
                        GimpRectangleFixedRule  rule)
{
  g_object_set_data (G_OBJECT (row), "painter-fixed-rule", GINT_TO_POINTER (rule));
  gtk_widget_set_no_show_all (row, TRUE);
  g_signal_connect_object (config, "notify::fixed-rule",
                            G_CALLBACK (painter_fixed_rule_notify), row, 0);
  painter_fixed_rule_notify (config, NULL, row);
}

static GtkWidget *
painter_rectangle_size (GtkWidget      *box,
                         GObject        *config,
                         const gchar    *property,
                         const gchar    *unit_property,
                         const gchar    *label,
                         GtkOrientation  axis,
                         gboolean        show_unit)
{
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *entry;

  painter_pack (box, row);
  painter_pack (row, gtk_label_new (label));
  entry = gimp_prop_size_entry_new (config, property, TRUE, unit_property, "%a",
                                   GIMP_SIZE_ENTRY_UPDATE_SIZE, 300);
  gimp_size_entry_show_unit_menu (GIMP_SIZE_ENTRY (entry), show_unit);
  gimp_rectangle_options_bind_size_entry (GIMP_RECTANGLE_OPTIONS (config),
                                          GIMP_SIZE_ENTRY (entry), 0, axis);
  painter_pack (row, entry);
  return row;
}

static void
painter_rectangle (GtkWidget *box,
                   GObject   *config)
{
  GtkWidget *details;
  GtkWidget *rules;
  GtkWidget *row;
  GtkWidget *entry;
  GtkWidget *opacity;

  details = painter_details (box,
                              GIMP_IS_CROP_OPTIONS (config) ? _("Crop details…") :
                                                             _("Selection details…"),
                              "gimp-painter-selection-details");
  painter_check (details, config, "fixed-center", _("Expand from center"));
  painter_check (details, config, "fixed-rule-active", _("Fixed rule"));
  rules = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  painter_pack (details, rules);
  painter_sensitive (config, "fixed-rule-active", rules);
  painter_combo (rules, config, "fixed-rule", _("Rule"), 0, 0);

  /* Independent property entries deliberately do not use the rectangle
   * options' private position/size/fixed-rule widget handles. */
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  painter_pack (rules, row);
  painter_pack (row, gtk_label_new (_("Aspect ratio")));
  entry = gimp_prop_number_pair_entry_new (config,
                                            "aspect-numerator", "aspect-denominator",
                                            "default-aspect-numerator",
                                            "default-aspect-denominator",
                                            "overridden-fixed-aspect",
                                            FALSE, TRUE, ":/xX*", TRUE,
                                            0.001, GIMP_MAX_IMAGE_SIZE);
  painter_pack (row, entry);
  painter_fixed_rule_row (config, row, GIMP_RECTANGLE_FIXED_ASPECT);
  row = painter_rectangle_size (rules, config, "desired-fixed-width", "fixed-unit",
                                 _("Fixed width"), GTK_ORIENTATION_HORIZONTAL, TRUE);
  painter_fixed_rule_row (config, row, GIMP_RECTANGLE_FIXED_WIDTH);
  row = painter_rectangle_size (rules, config, "desired-fixed-height", "fixed-unit",
                                 _("Fixed height"), GTK_ORIENTATION_VERTICAL, TRUE);
  painter_fixed_rule_row (config, row, GIMP_RECTANGLE_FIXED_HEIGHT);
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  painter_pack (rules, row);
  painter_pack (row, gtk_label_new (_("Fixed size (px)")));
  entry = gimp_prop_number_pair_entry_new (config,
                                            "desired-fixed-size-width",
                                            "desired-fixed-size-height",
                                            "default-fixed-size-width",
                                            "default-fixed-size-height",
                                            "overridden-fixed-size",
                                            TRUE, FALSE, "xX*:/", FALSE,
                                            1.0, GIMP_MAX_IMAGE_SIZE);
  painter_pack (row, entry);
  painter_fixed_rule_row (config, row, GIMP_RECTANGLE_FIXED_SIZE);

  painter_rectangle_size (details, config, "x", "position-unit", _("X"),
                           GTK_ORIENTATION_HORIZONTAL, FALSE);
  row = painter_rectangle_size (details, config, "y", "position-unit", _("Y"),
                                 GTK_ORIENTATION_VERTICAL, FALSE);
  painter_pack (row, gimp_prop_unit_combo_box_new (config, "position-unit"));
  painter_rectangle_size (details, config, "width", "size-unit", _("Width"),
                           GTK_ORIENTATION_HORIZONTAL, FALSE);
  row = painter_rectangle_size (details, config, "height", "size-unit", _("Height"),
                                 GTK_ORIENTATION_VERTICAL, FALSE);
  painter_pack (row, gimp_prop_unit_combo_box_new (config, "size-unit"));
  painter_check (details, config, "highlight", _("Highlight"));
  opacity = gimp_prop_spin_scale_new (config, "highlight-opacity", 0.01, 0.1, 0);
  gimp_prop_widget_set_factor (opacity, 100.0, 1.0, 10.0, 1);
  painter_pack (details, opacity);
  painter_sensitive (config, "highlight", opacity);
  painter_combo (details, config, "guide", _("Guides"),
                  GIMP_GUIDES_NONE, GIMP_GUIDES_DIAGONALS);

  painter_pack (details,
                gimp_rectangle_options_auto_shrink_button_new (GIMP_RECTANGLE_OPTIONS (config)));
  painter_check (details, config, "shrink-merged", _("Shrink merged"));

  /* Ratio/size history remains in the ordinary dock. */
}

static void
painter_foreground_reset_width (GtkWidget       *button,
                                 GimpToolOptions *options)
{
  g_object_set (options, "stroke-width", 10, NULL);
}

static gboolean
painter_foreground_engine_visible (GBinding     *binding,
                                    const GValue *source,
                                    GValue       *target,
                                    gpointer      engine)
{
  g_value_set_boolean (target,
                       g_value_get_enum (source) == GPOINTER_TO_INT (engine));
  return TRUE;
}

/* Legacy gimpforegroundselectoptions.c:261-414 (afa43fae), hunks
 * 01.002/001256-001263: separate horizontal selection controls and Details.
 * The current tool uses GEGL matting, not the old SIOX model. Background
 * marking maps to draw-mode, not the inherited context background color;
 * stroke-width survives; mask-color is now a GeglColor. Contiguous, smoothness and L/a/b sensitivity have no current
 * property/algorithm counterpart. Their legacy parity remains unresolved
 * under 29.010/standard-tool-options; do not invent inert replacement widgets.
 * Preview mode, unknown drawing, width reset and engine parameters below are
 * current-tool additions, not claims of restored legacy SIOX controls. */
static void
painter_foreground (GtkWidget       *box,
                    GimpToolOptions *options)
{
  GObject   *config = G_OBJECT (options);
  GtkWidget *details;
  GtkWidget *frame;
  GtkWidget *row;
  GtkWidget *scale;
  GtkWidget *button;
  GtkWidget *combo;
  const struct
  {
    const gchar       *property;
    GimpMattingEngine  engine;
  } parameters[] = {
    { "levels",        GIMP_MATTING_ENGINE_LEVIN  },
    { "active-levels", GIMP_MATTING_ENGINE_LEVIN  },
    { "iterations",    GIMP_MATTING_ENGINE_GLOBAL }
  };

  details = painter_details (box, _("Details…"),
                              "gimp-painter-foreground-details");
  frame = gimp_prop_enum_radio_frame_new (config, "draw-mode", NULL, 0, 0);
  gtk_widget_set_name (frame, "gimp-painter-foreground-draw-mode");
  painter_pack (details, frame);

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  painter_pack (details, row);
  scale = gimp_prop_spin_scale_new (config, "stroke-width", 1.0, 10.0, 2);
  gimp_spin_scale_set_scale_limits (GIMP_SPIN_SCALE (scale), 1.0, 1000.0);
  gimp_spin_scale_set_gamma (GIMP_SPIN_SCALE (scale), 1.7);
  gtk_box_pack_start (GTK_BOX (row), scale, TRUE, TRUE, 0);
  button = gimp_icon_button_new (GIMP_ICON_RESET, NULL);
  gtk_widget_set_name (button, "gimp-painter-foreground-reset-width");
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_image_set_from_icon_name (GTK_IMAGE (gtk_bin_get_child (GTK_BIN (button))),
                                GIMP_ICON_RESET, GTK_ICON_SIZE_MENU);
  gimp_help_set_help_data (button, _("Reset stroke width native size"), NULL);
  painter_pack (row, button);
  g_signal_connect_object (button, "clicked",
                            G_CALLBACK (painter_foreground_reset_width), options, 0);

  frame = gimp_prop_enum_radio_frame_new (config, "preview-mode", NULL, 0, 0);
  gtk_widget_set_name (frame, "gimp-painter-foreground-preview-mode");
  painter_pack (details, frame);
  button = gimp_prop_color_button_new (config, "mask-color", NULL,
                                       128, 24, GIMP_COLOR_AREA_SMALL_CHECKS);
  gimp_color_panel_set_context (GIMP_COLOR_PANEL (button), GIMP_CONTEXT (options));
  painter_pack (details, button);

  frame = gimp_frame_new (NULL);
  painter_pack (details, frame);
  combo = gimp_prop_enum_combo_box_new (config, "engine", 0, 0);
  gimp_int_combo_box_set_label (GIMP_INT_COMBO_BOX (combo), _("Engine"));
  g_object_set (combo, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), combo);
  gtk_widget_set_sensitive (combo, gegl_has_operation ("gegl:matting-levin"));
  row = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_container_add (GTK_CONTAINER (frame), row);
  gtk_widget_show (row);
  for (guint i = 0; i < G_N_ELEMENTS (parameters); i++)
    {
      scale = gimp_prop_spin_scale_new (config, parameters[i].property,
                                        1.0, 1.0, 0);
      painter_pack (row, scale);
      /* A later show_all() must not undo the current engine's visibility. */
      gtk_widget_set_no_show_all (scale, TRUE);
      g_object_bind_property_full (config, "engine", scale, "visible",
                                   G_BINDING_SYNC_CREATE,
                                   painter_foreground_engine_visible, NULL,
                                   GINT_TO_POINTER (parameters[i].engine), NULL);
    }
}

static void
painter_selection (GtkWidget       *box,
                   GimpToolOptions *options)
{
  GObject     *config = G_OBJECT (options);
  GtkWidget   *mode;
  GtkWidget   *antialias;
  GtkWidget   *details;
  GList       *children;
  const gchar *tool = options->tool_info ?
                     g_type_name (options->tool_info->tool_type) : "";

  painter_pack (box, gtk_label_new (_("Mode:")));
  mode = gimp_prop_enum_icon_box_new (config, "operation", "gimp-selection", 0, 0);
  painter_pack (box, mode);
  children = gtk_container_get_children (GTK_CONTAINER (mode));
  if (g_list_length (children) > GIMP_CHANNEL_OP_REPLACE)
    gtk_box_reorder_child (GTK_BOX (mode),
                           g_list_nth_data (children, GIMP_CHANNEL_OP_REPLACE), 0);
  g_list_free (children);
  antialias = painter_check (box, config, "antialias", _("Antialiasing"));
  details = painter_toggle_details (box, config, "feather", _("Feather"),
                                     "gimp-painter-selection-feather");
  painter_number (details, config, "feather-radius", _("Radius"), 1.0, 1);

  if (GIMP_IS_FOREGROUND_SELECT_OPTIONS (options))
    {
      GtkWidget *button = gtk_popover_get_relative_to
        (GTK_POPOVER (gtk_widget_get_ancestor (details, GTK_TYPE_POPOVER)));

      /* The old foreground factory disabled the shared antialias control;
       * the current native GUI hides it. Keep it hidden even after show_all,
       * without replacing the ordinary GUI's antialias_toggle pointer. */
      gtk_widget_set_no_show_all (antialias, TRUE);
      gtk_widget_hide (antialias);
      painter_sensitive (config, "feather", button);
      painter_foreground (box, options);
    }
  else if (GIMP_IS_ISCISSORS_OPTIONS (options))
    painter_check (box, config, "interactive", _("Interactive boundary"));
  else if (GIMP_IS_REGION_SELECT_OPTIONS (options))
    {
      painter_number (box, config, "threshold", _("Threshold"), 1.0, 1);
      details = painter_details (box, _("Selection details…"),
                                  "gimp-painter-region-details");
      painter_check (details, config, "select-transparent", _("Select transparent areas"));
      painter_check (details, config, "sample-merged", _("Sample merged"));
      if (g_strcmp0 (tool, "GimpFuzzySelectTool") == 0)
        painter_check (details, config, "diagonal-neighbors", _("Diagonal neighbors"));
      painter_combo (details, config, "select-criterion", _("Select by"), 0, 0);
      painter_check (details, config, "draw-mask", _("Draw mask"));
    }
  else if (GIMP_IS_RECTANGLE_SELECT_OPTIONS (options))
    {
      if (g_strcmp0 (tool, "GimpRectangleSelectTool") == 0)
        {
          painter_sensitive (config, "round-corners", antialias);
          details = painter_toggle_details (box, config, "round-corners",
                                             _("Rounded corners"),
                                             "gimp-painter-rounded-corners");
          painter_number (details, config, "corner-radius", _("Radius"), 1.0, 1);
        }
      painter_rectangle (box, config);
    }
}

static void
painter_flip (GtkWidget *box,
              GObject   *config)
{
  painter_pack (box, gtk_label_new (_("Affect:")));
  painter_pack (box, gimp_prop_enum_icon_box_new (config, "type", "gimp", 0, 0));
  painter_pack (box, gimp_prop_enum_radio_frame_new_with_orientation
                (config, "flip-type", _("Direction"),
                 GIMP_ORIENTATION_HORIZONTAL, GIMP_ORIENTATION_VERTICAL,
                 GTK_ORIENTATION_HORIZONTAL));
  painter_combo (box, config, "clip", _("Clipping"),
                  GIMP_TRANSFORM_RESIZE_ADJUST, GIMP_TRANSFORM_RESIZE_CLIP);
}

static void
painter_magnify (GtkWidget *box,
                 GObject   *config)
{
  painter_check (box, config, "auto-resize", _("Auto-resize window"));
  painter_pack (box, gimp_prop_enum_radio_frame_new_with_orientation
                (config, "zoom-type", _("Direction"), 0, 0,
                 GTK_ORIENTATION_HORIZONTAL));
}

static void
painter_color_picker (GtkWidget *box,
                      GObject   *config)
{
  GtkWidget *details;

  painter_check (box, config, "sample-merged", _("Sample merged"));
  details = painter_toggle_details (box, config, "sample-average",
                                     _("Sample average"),
                                     "gimp-painter-color-average");
  painter_number (details, config, "average-radius", _("Radius"), 1.0, 0);
  painter_combo (box, config, "pick-target", _("Pick target"), 0, 0);
  painter_check (box, config, "use-info-window", _("Use info window"));
}

/* The legacy Threshold factory has only the histogram scale. Levels and
 * Curves use the color-options factory, which adds the average toggle and
 * radius popup. Filter-dialog controls remain owned by the active tool. */
static void
painter_histogram (GtkWidget       *box,
                   GimpToolOptions *options)
{
  GObject   *config = G_OBJECT (options);
  GtkWidget *frame;
  GtkWidget *details;

  frame = gimp_prop_enum_radio_frame_new_with_orientation
    (config, "histogram-scale", _("Histogram Scale"), 0, 0,
     GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_name (frame, "gimp-painter-histogram-scale");
  painter_pack (box, frame);

  if (options->tool_info &&
      g_strcmp0 (gimp_object_get_name (options->tool_info),
                 "gimp-threshold-tool") != 0)
    {
      GtkWidget *button;

      details = painter_toggle_details (box, config, "sample-average", _("Average"),
                                         "gimp-painter-histogram-average");
      button = gtk_popover_get_relative_to
        (GTK_POPOVER (gtk_widget_get_ancestor (details, GTK_TYPE_POPOVER)));
      gtk_widget_set_tooltip_text (button, _("Sample Average"));
      /* The legacy horizontal helper disables the popup arrow itself. */
      painter_sensitive (config, "sample-average", button);
      painter_pack (details, gimp_prop_spin_scale_new (config, "average-radius",
                                                       1.0, 10.0, 0));
    }
}

static void
painter_cage (GtkWidget *box,
              GObject   *config)
{
  painter_pack (box, gimp_prop_enum_radio_frame_new_with_orientation
                (config, "cage-mode", _("Cage edit mode"), 0, 0,
                 GTK_ORIENTATION_HORIZONTAL));
  painter_check (box, config, "fill-plain-color",
                   _("Fill the original cage position with a color"));
}

static void
painter_crop (GtkWidget *box,
              GObject   *config)
{
  GtkWidget *button;
  GtkWidget *details;

  painter_check (box, config, "layer-only", _("Selected layers only"));
  button = painter_check (box, config, "delete-pixels", _("Delete cropped pixels"));
  g_object_bind_property (config, "layer-only", button, "sensitive",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  details = painter_toggle_details (box, config, "allow-growing", _("Allow growing"),
                                     "gimp-painter-crop-growing");
  painter_combo (details, config, "fill-type", _("Fill with"), 0, 0);
  painter_rectangle (box, config);
}

static void
painter_measure (GtkWidget *box,
                 GObject   *config)
{
  painter_combo (box, config, "orientation", _("Orientation"), 0, 0);
  painter_check (box, config, "use-info-window", _("Use info window"));
  /* The legacy measure toolbar had only the information-window toggle.
   * Modern Straighten is an active-tool action, so its callback, transform
   * controls and native straighten_button remain in the full options GUI. */
}

static void
painter_composite (GtkWidget *box,
                   GObject   *config)
{
  GtkWidget *mode;
  GtkWidget *opacity;

  mode = gimp_prop_layer_mode_box_new (config, "paint-mode", GIMP_LAYER_MODE_CONTEXT_PAINT);
  gimp_layer_mode_box_set_label (GIMP_LAYER_MODE_BOX (mode), _("Mode"));
  gimp_layer_mode_box_set_ellipsize (GIMP_LAYER_MODE_BOX (mode), PANGO_ELLIPSIZE_END);
  painter_pack (box, mode);
  opacity = gimp_prop_spin_scale_new (config, "opacity", 0.01, 0.1, 0);
  gimp_prop_widget_set_factor (opacity, 100.0, 1.0, 10.0, 1);
  gtk_widget_set_size_request (opacity, 130, -1);
  painter_pack (box, opacity);
}

static void
painter_bucket_area_notify (GObject    *config,
                            GParamSpec *pspec,
                            GtkWidget  *details)
{
  gint area;

  g_object_get (config, "fill-area", &area, NULL);
  gtk_widget_set_sensitive (details, area == GIMP_BUCKET_FILL_SIMILAR_COLORS);
}

static void
painter_bucket (GtkWidget       *box,
                GimpToolOptions *options)
{
  GObject   *config = G_OBJECT (options);
  GtkWidget *details;
  GtkWidget *similar;

  painter_composite (box, config);
  painter_combo (box, config, "fill-mode", _("Fill"), 0, 0);
  details = painter_details (box, _("Details…"), "gimp-painter-fill-details");
  painter_pack (details, gimp_prop_pattern_box_new
                (NULL, GIMP_CONTEXT (options), _("Pattern"), 2,
                 "pattern-view-type", "pattern-view-size"));
  painter_combo (details, config, "fill-area", _("Affected area"), 0, 0);
  similar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  painter_pack (details, similar);
  painter_check (similar, config, "fill-transparent", _("Fill transparent areas"));
  painter_check (similar, config, "sample-merged", _("Sample merged"));
  painter_check (similar, config, "diagonal-neighbors", _("Diagonal neighbors"));
  painter_check (similar, config, "antialias", _("Antialiasing"));
  painter_number (similar, config, "threshold", _("Threshold"), 1.0, 1);
  painter_combo (similar, config, "fill-criterion", _("Fill by"), 0, 0);
  g_signal_connect_object (config, "notify::fill-area",
                            G_CALLBACK (painter_bucket_area_notify), similar, 0);
  painter_bucket_area_notify (config, NULL, similar);
  /* Current line-art detection/stroke settings have no legacy equivalent;
   * those controls remain in the full dockable rather than taking over its
   * line_art_busy_box or stroke_options objects. */
}

static void
painter_gradient_type_notify (GObject    *config,
                              GParamSpec *pspec,
                              GtkWidget  *combo)
{
  gint     type;
  gboolean metric = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo),
                                                        "painter-gradient-metric"));

  g_object_get (config, "gradient-type", &type, NULL);
  gtk_widget_set_sensitive (combo,
                            metric ? (type >= GIMP_GRADIENT_SHAPEBURST_ANGULAR &&
                                      type <= GIMP_GRADIENT_SHAPEBURST_DIMPLED) :
                                     type < GIMP_GRADIENT_SHAPEBURST_ANGULAR);
}

static void
painter_gradient (GtkWidget       *box,
                  GimpToolOptions *options)
{
  GObject   *config = G_OBJECT (options);
  GtkWidget *details;
  GtkWidget *combo;
  GtkWidget *sampling;

  painter_composite (box, config);
  painter_pack (box, gimp_prop_gradient_box_new
                (NULL, GIMP_CONTEXT (options), _("Gradient"), 2,
                 "gradient-view-type", "gradient-view-size", "gradient-reverse",
                 "gradient-blend-color-space", "gimp-gradient-editor",
                 _("Edit this gradient")));
  painter_combo (box, config, "gradient-type", _("Shape"), 0, 0);
  details = painter_details (box, _("Details…"), "gimp-painter-gradient-details");
  painter_combo (details, config, "gradient-blend-color-space", _("Blend Color Space"), 0, 0);
  combo = painter_combo (details, config, "gradient-repeat", _("Repeat"), 0, 0);
  g_signal_connect_object (config, "notify::gradient-type",
                            G_CALLBACK (painter_gradient_type_notify), combo, 0);
  painter_gradient_type_notify (config, NULL, combo);
  combo = painter_combo (details, config, "distance-metric", _("Metric"), 0, 0);
  g_object_set_data (G_OBJECT (combo), "painter-gradient-metric", GINT_TO_POINTER (TRUE));
  g_signal_connect_object (config, "notify::gradient-type",
                            G_CALLBACK (painter_gradient_type_notify), combo, 0);
  painter_gradient_type_notify (config, NULL, combo);
  painter_number (details, config, "offset", _("Offset"), 1.0, 1);
  painter_check (details, config, "dither", _("Dithering"));
  painter_check (details, config, "supersample", _("Adaptive supersampling"));
  sampling = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  painter_pack (details, sampling);
  painter_sensitive (config, "supersample", sampling);
  painter_number (sampling, config, "supersample-depth", _("Max depth"), 1.0, 0);
  painter_number (sampling, config, "supersample-threshold", _("Threshold"), 0.01, 2);
  /* Modern instant/modify-active controls depend on in-progress editing and
   * gradient writability, beyond the legacy blend options. They remain in
   * the dockable; never replace its frame/hint/instant_toggle pointers. */
}

GtkWidget *
gimp_painter_tool_toolbar_new (GimpToolOptions *options)
{
  GtkWidget *box;
  GObject   *config;

  g_return_val_if_fail (GIMP_IS_TOOL_OPTIONS (options), NULL);

  if (! GIMP_IS_MOVE_OPTIONS (options) &&
      ! GIMP_IS_FOREGROUND_SELECT_OPTIONS (options) &&
      ! GIMP_IS_REGION_SELECT_OPTIONS (options) &&
      ! GIMP_IS_ISCISSORS_OPTIONS (options) &&
      ! GIMP_IS_CAGE_OPTIONS (options) &&
      ! GIMP_IS_CROP_OPTIONS (options) &&
      ! GIMP_IS_MEASURE_OPTIONS (options) &&
      ! GIMP_IS_RECTANGLE_SELECT_OPTIONS (options) &&
      G_OBJECT_TYPE (options) != GIMP_TYPE_SELECTION_OPTIONS &&
      ! GIMP_IS_FLIP_OPTIONS (options) &&
      ! GIMP_IS_MAGNIFY_OPTIONS (options) &&
      ! GIMP_IS_COLOR_PICKER_OPTIONS (options) &&
      ! GIMP_IS_HISTOGRAM_OPTIONS (options) &&
      ! GIMP_IS_BUCKET_FILL_OPTIONS (options) &&
      ! GIMP_IS_GRADIENT_OPTIONS (options))
    return NULL;

  config = G_OBJECT (options);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_name (box, "gimp-painter-tool-toolbar");
  g_object_set_data_full (G_OBJECT (box), "gimp-painter-toolbar-options",
                          g_object_ref (options), g_object_unref);

  if (GIMP_IS_MOVE_OPTIONS (options))
    painter_move (box, config);
  else if (GIMP_IS_SELECTION_OPTIONS (options))
    painter_selection (box, options);
  else if (GIMP_IS_FLIP_OPTIONS (options))
    painter_flip (box, config);
  else if (GIMP_IS_MAGNIFY_OPTIONS (options))
    painter_magnify (box, config);
  else if (GIMP_IS_COLOR_PICKER_OPTIONS (options))
    painter_color_picker (box, config);
  else if (GIMP_IS_HISTOGRAM_OPTIONS (options))
    painter_histogram (box, options);
  else if (GIMP_IS_CAGE_OPTIONS (options))
    painter_cage (box, config);
  else if (GIMP_IS_CROP_OPTIONS (options))
    painter_crop (box, config);
  else if (GIMP_IS_MEASURE_OPTIONS (options))
    painter_measure (box, config);
  else if (GIMP_IS_BUCKET_FILL_OPTIONS (options))
    painter_bucket (box, options);
  else if (GIMP_IS_GRADIENT_OPTIONS (options))
    painter_gradient (box, options);

  return box;
}
