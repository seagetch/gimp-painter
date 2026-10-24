/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-1999 Spencer Kimball and Peter Mattis
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Independent horizontal paint options, ported from gimp-painter's
 * gimp_paint_options_gui_full(..., TRUE) and the corresponding paint-tool
 * factories at afa43fae3e920210146abed514f136fd49f671b5. The legacy layout
 * puts mode, opacity, brush and size on the bar, with dedicated brush,
 * dynamics, jitter, texture and smoothing popups. The property widgets and
 * resource selectors below use the current GIMP APIs and the SAME options
 * object as the dock. In particular, this must never call a dock GUI factory:
 * clone and several other factories save single-view widget pointers there.
 */

#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "config/gimpconfig-utils.h"

#include "core/gimp.h"
#include "core/gimpdatafactory.h"
#include "core/gimptoolinfo.h"

#include "paint/gimpinkoptions.h"
#include "paint/gimppaintoptions.h"

#include "widgets/gimpblobeditor.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimplayermodebox.h"
#include "widgets/gimppropwidgets.h"
#include "widgets/gimpviewablebox.h"
#include "widgets/gimpviewablebutton.h"
#include "widgets/gimpwidgets-constructors.h"
#include "widgets/gimpwidgets-utils.h"

#include "gimpairbrushtool.h"
#include "gimpclonetool.h"
#include "gimpconvolvetool.h"
#include "gimpdodgeburntool.h"
#include "gimperasertool.h"
#include "gimpfillbrushtool.h"
#include "gimphealtool.h"
#include "gimpinktool.h"
#include "gimpmybrushtool.h"
#include "gimppainterpainttoolbar.h"
#include "gimppaintertogglepopup.h"
#include "gimppaintersmudgetool.h"
#include "gimppenciltool.h"
#include "gimpperspectiveclonetool.h"
#include "gimpsmudgetool.h"

#include "gimp-intl.h"


static void
paint_toolbar_pack (GtkWidget *box,
                    GtkWidget *child)
{
  gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), child, FALSE, FALSE, 0);
  gtk_widget_show (child);
}

static GtkWidget *
paint_toolbar_column (void)
{
  return gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
}

static GtkWidget *
paint_toolbar_scale (GObject     *config,
                     const gchar *property,
                     gdouble      step,
                     gdouble      page,
                     gint         digits,
                     gint         width)
{
  GtkWidget *scale;

  scale = gimp_prop_spin_scale_new (config, property, step, page, digits);
  gimp_spin_scale_set_constrain_drag (GIMP_SPIN_SCALE (scale), TRUE);
  gtk_entry_set_width_chars (GTK_ENTRY (scale), 5);
  gtk_widget_set_size_request (scale, width, -1);

  return scale;
}

static GtkWidget *
paint_toolbar_enum (GObject     *config,
                    const gchar *property,
                    const gchar *label)
{
  GtkWidget *combo = gimp_prop_enum_combo_box_new (config, property, 0, 0);

  if (label)
    gimp_int_combo_box_set_label (GIMP_INT_COMBO_BOX (combo), label);
  g_object_set (combo, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

  return combo;
}

/* Presentation-owned content, never an expander taken from the dock. The
 * three historical toggle groups use native GimpPopup semantics; all other
 * callers retain their existing GtkPopover presentation unchanged.
 */
static GtkWidget *
paint_toolbar_popup (GtkWidget   *toolbar,
                     GObject     *config,
                     const gchar *id,
                     const gchar *label,
                     const gchar *enable_property,
                     GtkWidget   *contents)
{
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gboolean native = !g_strcmp0 (id, "jitter") ||
                    !g_strcmp0 (id, "texture") ||
                    !g_strcmp0 (id, "smoothing");
  GtkWidget *scroll = gtk_scrolled_window_new (NULL, NULL);
  GtkWidget *button = native ? gimp_painter_toggle_popup_button_new (row, scroll) :
                               gtk_menu_button_new ();
  GtkWidget *popover = native ? NULL : gtk_popover_new (button);
  gchar     *name = g_strconcat ("painter-paint-", id, NULL);

  gtk_widget_set_name (button, name);
  g_free (name);
  gtk_widget_set_tooltip_text (button, label);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  if (popover) gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_TOP);

  if (enable_property)
    {
      GtkWidget *toggle;

      toggle = gimp_prop_check_button_new (config, enable_property, label);
      paint_toolbar_pack (row, toggle);
      gtk_container_add (GTK_CONTAINER (button),
                         gtk_image_new_from_icon_name ("pan-down-symbolic",
                                                       GTK_ICON_SIZE_MENU));
      g_object_bind_property (config, enable_property,
                              native ? button : contents, "sensitive", G_BINDING_SYNC_CREATE);
    }
  else
    {
      gtk_button_set_label (GTK_BUTTON (button), label);
    }

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scroll), 300);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroll), 420);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroll), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (contents), 8);
  gtk_container_add (GTK_CONTAINER (scroll), contents);
  if (popover)
    {
      gtk_container_add (GTK_CONTAINER (popover), scroll);
      gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);
    }
  g_object_set_data (G_OBJECT (button), "gimp-painter-toolbar-popup", contents);
  gtk_widget_show_all (scroll);
  gtk_widget_show_all (button);
  paint_toolbar_pack (row, button);
  paint_toolbar_pack (toolbar, row);

  return button;
}

static GtkWidget *
paint_toolbar_resource (GimpToolOptions *options,
                        gboolean         dynamics)
{
  GimpContext *context = GIMP_CONTEXT (options);
  GtkWidget   *button;
  const gchar *view_type = dynamics ? "dynamics-view-type" : "brush-view-type";
  const gchar *view_size = dynamics ? "dynamics-view-size" : "brush-view-size";
  GimpViewType type;
  gint         size;

  g_object_get (options, view_type, &type, view_size, &size, NULL);
  button = gimp_viewable_button_new (
    gimp_data_factory_get_container (dynamics ? context->gimp->dynamics_factory :
                                               context->gimp->brush_factory),
    context, type, GIMP_VIEW_SIZE_SMALL, size, 1,
    gimp_dialog_factory_get_singleton (),
    dynamics ? "gimp-dynamics-list|gimp-dynamics-grid" :
               "gimp-brush-grid|gimp-brush-list",
    dynamics ? GIMP_ICON_DYNAMICS : GIMP_ICON_BRUSH,
    dynamics ? _("Open the dynamics selection dialog") :
               _("Open the brush selection dialog"));
  gimp_config_connect_full (G_OBJECT (options), G_OBJECT (button),
                            view_type, "popup-view-type");
  gimp_config_connect_full (G_OBJECT (options), G_OBJECT (button),
                            view_size, "popup-view-size");

  return button;
}

static void
paint_toolbar_reset_brush (GtkWidget        *button,
                           GimpPaintOptions *options)
{
  const gchar *property = g_object_get_data (G_OBJECT (button), "brush-property");
  GimpBrush   *brush = gimp_context_get_brush (GIMP_CONTEXT (options));

  if (g_str_equal (property, "brush-force"))
    g_object_set (options, "brush-force", 0.5, NULL);
  else if (brush)
    {
      if (g_str_equal (property, "brush-size"))
        gimp_paint_options_set_default_brush_size (options, brush);
      else if (g_str_equal (property, "brush-aspect-ratio"))
        gimp_paint_options_set_default_brush_aspect_ratio (options, brush);
      else if (g_str_equal (property, "brush-angle"))
        gimp_paint_options_set_default_brush_angle (options, brush);
      else if (g_str_equal (property, "brush-spacing"))
        gimp_paint_options_set_default_brush_spacing (options, brush);
      else if (g_str_equal (property, "brush-hardness"))
        gimp_paint_options_set_default_brush_hardness (options, brush);
    }
}

static GtkWidget *
paint_toolbar_brush_scale (GObject     *config,
                           const gchar *property,
                           const gchar *link_property,
                           gdouble      step,
                           gdouble      page,
                           gint         digits,
                           gdouble      minimum,
                           gdouble      maximum,
                           gdouble      factor,
                           gdouble      gamma,
                           gint         width)
{
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *scale = paint_toolbar_scale (config, property, step, page, digits, width);
  GtkWidget *reset = gimp_icon_button_new (GIMP_ICON_RESET, NULL);

  gimp_prop_widget_set_factor (scale, factor, step, page, digits);
  gimp_spin_scale_set_scale_limits (GIMP_SPIN_SCALE (scale), minimum, maximum);
  gimp_spin_scale_set_gamma (GIMP_SPIN_SCALE (scale), gamma);
  gtk_box_pack_start (GTK_BOX (row), scale, TRUE, TRUE, 0);
  gtk_widget_show (scale);

  gtk_button_set_relief (GTK_BUTTON (reset), GTK_RELIEF_NONE);
  gtk_image_set_from_icon_name (GTK_IMAGE (gtk_bin_get_child (GTK_BIN (reset))),
                                GIMP_ICON_RESET, GTK_ICON_SIZE_MENU);
  g_object_set_data_full (G_OBJECT (reset), "brush-property",
                          g_strdup (property), g_free);
  g_signal_connect_object (reset, "clicked",
                           G_CALLBACK (paint_toolbar_reset_brush), config, 0);
  gimp_help_set_help_data (reset, _("Reset to brush's native value"), NULL);
  paint_toolbar_pack (row, reset);

  if (link_property)
    {
      GtkWidget *link = gtk_toggle_button_new ();

      gtk_button_set_relief (GTK_BUTTON (link), GTK_RELIEF_NONE);
      gtk_container_add (GTK_CONTAINER (link),
                         gtk_image_new_from_icon_name (GIMP_ICON_LINKED,
                                                       GTK_ICON_SIZE_MENU));
      gtk_widget_show_all (link);
      g_object_bind_property (config, link_property, link, "active",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      gimp_help_set_help_data (link, _("Link to brush default"), NULL);
      paint_toolbar_pack (row, link);
    }

  return row;
}

static GtkWidget *
paint_toolbar_brush_details (GimpToolOptions *options,
                             GType            tool_type)
{
  GObject   *config = G_OBJECT (options);
  GtkWidget *box = paint_toolbar_column ();

  paint_toolbar_pack (box,
    gimp_prop_brush_box_new (NULL, GIMP_CONTEXT (options), _("Brush"), 2,
                             "brush-view-type", "brush-view-size",
                             "gimp-brush-editor", _("Edit this brush")));
  paint_toolbar_pack (box,
    paint_toolbar_brush_scale (config, "brush-aspect-ratio", "brush-link-aspect-ratio",
                                0.1, 1.0, 2, -20.0, 20.0, 1.0, 1.0, 200));
  paint_toolbar_pack (box,
    paint_toolbar_brush_scale (config, "brush-angle", "brush-link-angle",
                                0.1, 1.0, 2, -180.0, 180.0, 1.0, 1.0, 200));
  paint_toolbar_pack (box,
    paint_toolbar_brush_scale (config, "brush-spacing", "brush-link-spacing",
                                0.1, 1.0, 1, 1.0, 200.0, 100.0, 1.7, 200));
  paint_toolbar_pack (box,
    paint_toolbar_brush_scale (config, "brush-hardness", "brush-link-hardness",
                                0.1, 1.0, 1, 0.0, 100.0, 100.0, 1.0, 200));
  if (tool_type != GIMP_TYPE_PENCIL_TOOL)
    paint_toolbar_pack (box,
      paint_toolbar_brush_scale (config, "brush-force", NULL,
                                  0.1, 1.0, 1, 0.0, 100.0, 100.0, 1.0, 200));
  paint_toolbar_pack (box,
    gimp_prop_check_button_new (config, "brush-lock-to-view", NULL));

  return box;
}

static GtkWidget *
paint_toolbar_dynamics (GimpToolOptions *options,
                        GType            tool_type)
{
  GObject   *config = G_OBJECT (options);
  GtkWidget *box = paint_toolbar_column ();
  GtkWidget *frame;
  GtkWidget *inner;
  GtkWidget *row;
  GtkWidget *scale;

  paint_toolbar_pack (box,
    gimp_prop_dynamics_box_new (NULL, GIMP_CONTEXT (options), _("Dynamics"), 2,
                                "dynamics-view-type", "dynamics-view-size",
                                "gimp-dynamics-editor", _("Edit this dynamics")));
  frame = gimp_frame_new (_("Fade Options"));
  inner = paint_toolbar_column ();
  gtk_container_add (GTK_CONTAINER (frame), inner);
  paint_toolbar_pack (box, frame);
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  scale = paint_toolbar_scale (config, "fade-length", 1.0, 50.0, 0, 180);
  gimp_spin_scale_set_scale_limits (GIMP_SPIN_SCALE (scale), 1.0, 1000.0);
  paint_toolbar_pack (row, scale);
  paint_toolbar_pack (row, gimp_prop_unit_combo_box_new (config, "fade-unit"));
  paint_toolbar_pack (inner, row);
  paint_toolbar_pack (inner, paint_toolbar_enum (config, "fade-repeat", _("Repeat")));
  paint_toolbar_pack (inner, gimp_prop_check_button_new (config, "fade-reverse", NULL));

  if (g_type_is_a (tool_type, GIMP_TYPE_PAINTBRUSH_TOOL) ||
      tool_type == GIMP_TYPE_SMUDGE_TOOL)
    {
      frame = gimp_frame_new (_("Color Options"));
      inner = paint_toolbar_column ();
      gtk_container_add (GTK_CONTAINER (frame), inner);
      paint_toolbar_pack (box, frame);
      paint_toolbar_pack (inner,
        gimp_prop_gradient_box_new (NULL, GIMP_CONTEXT (options), _("Gradient"), 2,
                                    "gradient-view-type", "gradient-view-size",
                                    "gradient-reverse", "gradient-blend-color-space",
                                    "gimp-gradient-editor", _("Edit this gradient")));
      paint_toolbar_pack (inner,
        paint_toolbar_enum (config, "gradient-blend-color-space", _("Blend Color Space")));
    }

  return box;
}

static void
paint_toolbar_shared (GtkWidget       *toolbar,
                      GimpToolOptions *options,
                      GType            tool_type)
{
  GObject   *config = G_OBJECT (options);
  GtkWidget *mode;
  GtkWidget *scale;
  GtkWidget *box;

  mode = gimp_prop_layer_mode_box_new (config, "paint-mode", GIMP_LAYER_MODE_CONTEXT_PAINT);
  gimp_layer_mode_box_set_label (GIMP_LAYER_MODE_BOX (mode), _("Mode"));
  gimp_layer_mode_box_set_ellipsize (GIMP_LAYER_MODE_BOX (mode), PANGO_ELLIPSIZE_END);
  gtk_widget_set_size_request (mode, 140, -1);
  paint_toolbar_pack (toolbar, mode);
  g_object_set_data (G_OBJECT (toolbar), "gimp-paint-options-gui-paint-mode-box", mode);
  if (tool_type == GIMP_TYPE_ERASER_TOOL     ||
      tool_type == GIMP_TYPE_CONVOLVE_TOOL   ||
      tool_type == GIMP_TYPE_DODGE_BURN_TOOL ||
      tool_type == GIMP_TYPE_HEAL_TOOL       ||
      tool_type == GIMP_TYPE_MYBRUSH_TOOL    ||
      tool_type == GIMP_TYPE_SMUDGE_TOOL)
    gtk_widget_set_sensitive (mode, FALSE);

  scale = paint_toolbar_scale (config, "opacity", 0.01, 0.1, 0, 110);
  gimp_prop_widget_set_factor (scale, 100.0, 1.0, 10.0, 1);
  paint_toolbar_pack (toolbar, scale);

  if (g_type_is_a (tool_type, GIMP_TYPE_BRUSH_TOOL))
    {
      paint_toolbar_pack (toolbar, paint_toolbar_resource (options, FALSE));
      paint_toolbar_pack (toolbar,
        paint_toolbar_brush_scale (config, "brush-size", "brush-link-size",
                                    1.0, 10.0, 2, 1.0, 1000.0, 1.0, 1.7, 110));
      paint_toolbar_popup (toolbar, config, "brush", _("Brush"), NULL,
                            paint_toolbar_brush_details (options, tool_type));
      paint_toolbar_pack (toolbar, paint_toolbar_resource (options, TRUE));
      paint_toolbar_popup (toolbar, config, "dynamics", _("Dynamics"), "dynamics-enabled",
                            paint_toolbar_dynamics (options, tool_type));

      box = paint_toolbar_column ();
      scale = paint_toolbar_scale (config, "jitter-amount", 0.01, 1.0, 2, 240);
      gimp_spin_scale_set_scale_limits (GIMP_SPIN_SCALE (scale), 0.0, 5.0);
      paint_toolbar_pack (box, scale);
      paint_toolbar_popup (toolbar, config, "jitter", _("Jitter"), "use-jitter", box);

      box = paint_toolbar_column ();
      paint_toolbar_pack (box,
        gimp_prop_pattern_box_new (NULL, GIMP_CONTEXT (options), NULL, 2,
                                   "pattern-view-type", "pattern-view-size"));
      paint_toolbar_popup (toolbar, config, "texture", _("Texture"), "use-texture", box);
    }

  box = paint_toolbar_column ();
  paint_toolbar_pack (box, paint_toolbar_scale (config, "smoothing-quality", 1, 10, 1, 240));
  paint_toolbar_pack (box, paint_toolbar_scale (config, "smoothing-factor", 1, 10, 1, 240));
  paint_toolbar_popup (toolbar, config, "smoothing", _("Smooth stroke"), "use-smoothing", box);

  if (tool_type == GIMP_TYPE_PENCIL_TOOL     ||
      tool_type == GIMP_TYPE_PAINTBRUSH_TOOL ||
      tool_type == GIMP_TYPE_ERASER_TOOL     ||
      tool_type == GIMP_TYPE_DODGE_BURN_TOOL)
    paint_toolbar_pack (toolbar,
      gimp_prop_enum_check_button_new (config, "application-mode", NULL,
                                       GIMP_PAINT_CONSTANT, GIMP_PAINT_INCREMENTAL));

  if (tool_type == GIMP_TYPE_ERASER_TOOL            ||
      tool_type == GIMP_TYPE_CLONE_TOOL             ||
      tool_type == GIMP_TYPE_HEAL_TOOL              ||
      tool_type == GIMP_TYPE_PERSPECTIVE_CLONE_TOOL ||
      tool_type == GIMP_TYPE_CONVOLVE_TOOL          ||
      tool_type == GIMP_TYPE_DODGE_BURN_TOOL        ||
      tool_type == GIMP_TYPE_SMUDGE_TOOL)
    paint_toolbar_pack (toolbar, gimp_prop_check_button_new (config, "hard", NULL));
}

static gboolean
paint_toolbar_clone_source (GBinding     *binding,
                            const GValue *source,
                            GValue       *target,
                            gpointer      data)
{
  g_value_set_boolean (target, g_value_get_enum (source) == GPOINTER_TO_INT (data));
  return TRUE;
}

static GtkWidget *
paint_toolbar_source_options (GimpToolOptions *options,
                              gboolean         clone)
{
  GObject   *config = G_OBJECT (options);
  GtkWidget *box = paint_toolbar_column ();
  GtkWidget *merged;

  if (clone)
    paint_toolbar_pack (box, paint_toolbar_enum (config, "clone-type", _("Source")));

  merged = gimp_prop_check_button_new (config, "sample-merged", NULL);
  paint_toolbar_pack (box, merged);
  if (clone)
    {
      GtkWidget *pattern;

      gtk_widget_set_no_show_all (merged, TRUE);
      g_object_bind_property_full (config, "clone-type", merged, "visible",
                                   G_BINDING_SYNC_CREATE, paint_toolbar_clone_source,
                                   NULL, GINT_TO_POINTER (GIMP_CLONE_IMAGE), NULL);
      pattern = gimp_prop_pattern_box_new (NULL, GIMP_CONTEXT (options), NULL, 2,
                                           "pattern-view-type", "pattern-view-size");
      paint_toolbar_pack (box, pattern);
      gtk_widget_set_no_show_all (pattern, TRUE);
      g_object_bind_property_full (config, "clone-type", pattern, "visible",
                                   G_BINDING_SYNC_CREATE, paint_toolbar_clone_source,
                                   NULL, GINT_TO_POINTER (GIMP_CLONE_PATTERN), NULL);
    }
  paint_toolbar_pack (box, paint_toolbar_enum (config, "align-mode", _("Alignment")));

  return box;
}

static void
paint_toolbar_ink (GtkWidget       *toolbar,
                   GimpToolOptions *options)
{
  GObject        *config = G_OBJECT (options);
  GimpInkOptions *ink = GIMP_INK_OPTIONS (options);
  GtkWidget      *box;
  GtkWidget      *editor;
  GtkWidget      *shape;

  paint_toolbar_pack (toolbar, paint_toolbar_scale (config, "size", 1.0, 2.0, 1, 110));
  paint_toolbar_pack (toolbar, paint_toolbar_scale (config, "tilt-angle", 1.0, 10.0, 1, 110));
  box = paint_toolbar_column ();
  paint_toolbar_pack (box, paint_toolbar_scale (config, "size-sensitivity", 0.01, 0.1, 2, 240));
  paint_toolbar_pack (box, paint_toolbar_scale (config, "tilt-sensitivity", 0.01, 0.1, 2, 240));
  paint_toolbar_pack (box, paint_toolbar_scale (config, "vel-sensitivity", 0.01, 0.1, 2, 240));
  paint_toolbar_popup (toolbar, config, "ink-sensitivity", _("Sensitivity"), NULL, box);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  shape = gimp_prop_enum_icon_box_new (config, "blob-type", "gimp-shape", 0, 0);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (shape), GTK_ORIENTATION_VERTICAL);
  paint_toolbar_pack (box, shape);
  editor = gimp_blob_editor_new (ink->blob_type, ink->blob_aspect, ink->blob_angle);
  gtk_widget_set_size_request (editor, 160, 160);
  paint_toolbar_pack (box, editor);
  gimp_config_connect (config, G_OBJECT (editor), "blob-type");
  gimp_config_connect (config, G_OBJECT (editor), "blob-aspect");
  gimp_config_connect (config, G_OBJECT (editor), "blob-angle");
  paint_toolbar_popup (toolbar, config, "ink-blob", _("Ink Blob"), NULL, box);
}

static void
paint_toolbar_specific (GtkWidget       *toolbar,
                        GimpToolOptions *options,
                        GType            tool_type)
{
  GObject   *config = G_OBJECT (options);
  GtkWidget *box;

  if (tool_type == GIMP_TYPE_AIRBRUSH_TOOL)
    {
      paint_toolbar_pack (toolbar, gimp_prop_check_button_new (config, "motion-only", NULL));
      paint_toolbar_pack (toolbar, paint_toolbar_scale (config, "rate", 1.0, 10.0, 1, 100));
      paint_toolbar_pack (toolbar, paint_toolbar_scale (config, "flow", 1.0, 10.0, 1, 100));
    }
  else if (tool_type == GIMP_TYPE_ERASER_TOOL)
    {
      gchar *label = g_strdup_printf (_("Anti erase  (%s)"),
                                      gimp_get_mod_string (GDK_MOD1_MASK));
      paint_toolbar_pack (toolbar, gimp_prop_check_button_new (config, "anti-erase", label));
      g_free (label);
    }
  else if (tool_type == GIMP_TYPE_CLONE_TOOL ||
           tool_type == GIMP_TYPE_PERSPECTIVE_CLONE_TOOL ||
           tool_type == GIMP_TYPE_HEAL_TOOL)
    {
      if (tool_type == GIMP_TYPE_PERSPECTIVE_CLONE_TOOL)
        paint_toolbar_pack (toolbar, paint_toolbar_enum (config, "clone-mode", NULL));
      paint_toolbar_popup (toolbar, config, "source", _("Source"), NULL,
                            paint_toolbar_source_options (options, tool_type != GIMP_TYPE_HEAL_TOOL));
    }
  else if (tool_type == GIMP_TYPE_CONVOLVE_TOOL)
    {
      box = paint_toolbar_column ();
      paint_toolbar_pack (box,
        gimp_prop_enum_radio_frame_new (config, "type", _("Convolve Type"), 0, 0));
      paint_toolbar_pack (box, paint_toolbar_scale (config, "rate", 1.0, 10.0, 1, 240));
      paint_toolbar_popup (toolbar, config, "convolve", _("Details…"), NULL, box);
    }
  else if (tool_type == GIMP_TYPE_DODGE_BURN_TOOL)
    {
      box = paint_toolbar_column ();
      paint_toolbar_pack (box, gimp_prop_enum_radio_frame_new (config, "type", _("Type"), 0, 0));
      paint_toolbar_pack (box, gimp_prop_enum_radio_frame_new (config, "mode", _("Range"), 0, 0));
      paint_toolbar_pack (box, paint_toolbar_scale (config, "exposure", 1.0, 10.0, 1, 240));
      paint_toolbar_popup (toolbar, config, "dodge-burn", _("Details…"), NULL, box);
    }
  else if (tool_type == GIMP_TYPE_SMUDGE_TOOL)
    {
      paint_toolbar_pack (toolbar, paint_toolbar_scale (config, "rate", 1.0, 10.0, 1, 100));
      paint_toolbar_pack (toolbar, paint_toolbar_scale (config, "flow", 1.0, 10.0, 1, 100));
      box = paint_toolbar_column ();
      paint_toolbar_pack (box, gimp_prop_check_button_new (config, "no-erasing", NULL));
      paint_toolbar_pack (box, gimp_prop_check_button_new (config, "sample-merged", NULL));
      paint_toolbar_popup (toolbar, config, "smudge", _("Details…"), NULL, box);
    }
  else if (tool_type == GIMP_TYPE_FILL_BRUSH_TOOL)
    {
      paint_toolbar_pack (toolbar, paint_toolbar_scale (config, "rate", 1.0, 10.0, 1, 100));
      paint_toolbar_pack (toolbar, gimp_prop_check_button_new (config, "eraser-mode", _("Eraser mode")));
    }
  else if (tool_type == GIMP_TYPE_PAINTER_SMUDGE_TOOL)
    {
      paint_toolbar_pack (toolbar, paint_toolbar_scale (config, "rate", 1.0, 10.0, 1, 100));
      paint_toolbar_pack (toolbar, gimp_prop_check_button_new (config, "use-color-blending", _("Color blending")));
    }
  else if (tool_type == GIMP_TYPE_INK_TOOL)
    {
      paint_toolbar_ink (toolbar, options);
    }
  else if (tool_type == GIMP_TYPE_MYBRUSH_TOOL)
    {
      box = paint_toolbar_column ();
      paint_toolbar_pack (box,
        gimp_prop_mybrush_box_new (NULL, GIMP_CONTEXT (options), _("Brush"), 2, NULL, NULL));
      paint_toolbar_popup (toolbar, config, "mybrush", _("Brush"), NULL, box);
      paint_toolbar_pack (toolbar, paint_toolbar_scale (config, "radius", 0.1, 1.0, 2, 110));
      paint_toolbar_pack (toolbar, paint_toolbar_scale (config, "opaque", 0.1, 1.0, 2, 110));
      paint_toolbar_pack (toolbar, paint_toolbar_scale (config, "hardness", 0.1, 1.0, 2, 110));
      paint_toolbar_pack (toolbar, gimp_prop_check_button_new (config, "eraser", NULL));
      paint_toolbar_pack (toolbar, gimp_prop_check_button_new (config, "no-erasing", NULL));
    }
}

static void
paint_toolbar_expand (GtkWidget       *toolbar,
                      GimpToolOptions *options)
{
  GObject   *config = G_OBJECT (options);
  GtkWidget *box = paint_toolbar_column ();
  GtkWidget *scale = paint_toolbar_scale (config, "expand-amount", 1, 10, 2, 240);

  gimp_spin_scale_set_scale_limits (GIMP_SPIN_SCALE (scale), 1.0, 1000.0);
  paint_toolbar_pack (box, scale);
  paint_toolbar_pack (box, paint_toolbar_enum (config, "expand-fill-type", NULL));
  paint_toolbar_pack (box,
    gimp_prop_enum_radio_frame_new (config, "expand-mask-fill-type", _("Fill Layer Mask With"), 0, 1));
  paint_toolbar_popup (toolbar, config, "expand", _("Expand Layers"), "expand-use", box);
}

GtkWidget *
gimp_painter_paint_toolbar_new (GimpToolOptions *tool_options)
{
  GType      type;
  GtkWidget *toolbar;

  g_return_val_if_fail (GIMP_IS_TOOL_OPTIONS (tool_options), NULL);

  if (! GIMP_IS_PAINT_OPTIONS (tool_options) || ! tool_options->tool_info)
    return NULL;

  type = tool_options->tool_info->tool_type;
  if (type != GIMP_TYPE_PAINTBRUSH_TOOL        &&
      type != GIMP_TYPE_PENCIL_TOOL            &&
      type != GIMP_TYPE_AIRBRUSH_TOOL          &&
      type != GIMP_TYPE_ERASER_TOOL            &&
      type != GIMP_TYPE_CLONE_TOOL             &&
      type != GIMP_TYPE_HEAL_TOOL              &&
      type != GIMP_TYPE_PERSPECTIVE_CLONE_TOOL &&
      type != GIMP_TYPE_CONVOLVE_TOOL          &&
      type != GIMP_TYPE_DODGE_BURN_TOOL        &&
      type != GIMP_TYPE_SMUDGE_TOOL            &&
      type != GIMP_TYPE_INK_TOOL               &&
      type != GIMP_TYPE_MYBRUSH_TOOL           &&
      type != GIMP_TYPE_FILL_BRUSH_TOOL        &&
      type != GIMP_TYPE_PAINTER_SMUDGE_TOOL)
    return NULL;

  toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_widget_set_name (toolbar, "gimp-painter-paint-toolbar");
  gtk_widget_set_valign (toolbar, GTK_ALIGN_CENTER);
  g_object_set_data_full (G_OBJECT (toolbar), "gimp-painter-toolbar-options",
                          g_object_ref (tool_options), g_object_unref);
  paint_toolbar_shared (toolbar, tool_options, type);
  paint_toolbar_specific (toolbar, tool_options, type);

  if (type == GIMP_TYPE_PAINTBRUSH_TOOL ||
      type == GIMP_TYPE_PENCIL_TOOL     ||
      type == GIMP_TYPE_AIRBRUSH_TOOL   ||
      type == GIMP_TYPE_CLONE_TOOL      ||
      type == GIMP_TYPE_HEAL_TOOL       ||
      type == GIMP_TYPE_CONVOLVE_TOOL   ||
      type == GIMP_TYPE_SMUDGE_TOOL     ||
      type == GIMP_TYPE_INK_TOOL        ||
      type == GIMP_TYPE_MYBRUSH_TOOL)
    paint_toolbar_expand (toolbar, tool_options);

  return toolbar;
}
