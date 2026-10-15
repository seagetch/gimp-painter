/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-1999 Spencer Kimball and Peter Mattis
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "core/gimpimage.h"
#include "core/gimptooloptions.h"

#include "widgets/gimppropwidgets.h"

#include "gimprectangleoptions.h"
#include "gimptooloptions-gui.h"

#include "gimp-intl.h"


#define SB_WIDTH 7


enum
{
  COLUMN_LEFT_NUMBER,
  COLUMN_RIGHT_NUMBER,
  COLUMN_TEXT,
  N_COLUMNS
};


enum
{
  AUTO_SHRINK_REQUESTED,
  AUTO_SHRINK_CHANGED,
  SIZE_METRICS_CHANGED,
  LAST_SIGNAL
};

static guint rectangle_signals[LAST_SIGNAL];

/*  local function prototypes  */

static void rectangle_metrics_clear (GimpRectangleOptionsPrivate *private);

static void gimp_rectangle_options_shrink_owner_gone (gpointer data,
                                                      GObject *where_object_was);


static void     gimp_rectangle_options_fixed_rule_changed     (GtkWidget                   *widget,
                                                               GimpRectangleOptionsPrivate *private);

static void     gimp_rectangle_options_string_current_updates (GimpNumberPairEntry         *entry,
                                                               GParamSpec                  *param,
                                                               GimpRectangleOptions        *rectangle_options);
static void     gimp_rectangle_options_setup_ratio_completion (GimpRectangleOptions        *rectangle_options,
                                                               GtkWidget                   *entry,
                                                               GtkListStore                *history);

static gboolean gimp_number_pair_entry_history_select         (GtkEntryCompletion          *completion,
                                                               GtkTreeModel                *model,
                                                               GtkTreeIter                 *iter,
                                                               GimpNumberPairEntry         *entry);

static void     gimp_number_pair_entry_history_add            (GtkWidget                   *entry,
                                                               GtkTreeModel                *model);


G_DEFINE_INTERFACE (GimpRectangleOptions, gimp_rectangle_options, GIMP_TYPE_TOOL_OPTIONS)


static void
gimp_rectangle_options_default_init (GimpRectangleOptionsInterface *iface)
{
  /* Action availability belongs to the model, not any particular GUI. */
  rectangle_signals[AUTO_SHRINK_REQUESTED] =
    g_signal_new ("auto-shrink-requested", G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
  rectangle_signals[AUTO_SHRINK_CHANGED] =
    g_signal_new ("auto-shrink-changed", G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  rectangle_signals[SIZE_METRICS_CHANGED] =
    g_signal_new ("size-metrics-changed", G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("auto-shrink",
                                                             NULL,
                                                             _("Automatically shrink to the nearest "
                                                               "rectangular shape in a layer"),
                                                             FALSE,
                                                             GIMP_CONFIG_PARAM_FLAGS |
                                                             GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("shrink-merged",
                                                             _("Shrink merged"),
                                                             _("Use all visible layers when shrinking "
                                                               "the selection"),
                                                             FALSE,
                                                             GIMP_CONFIG_PARAM_FLAGS |
                                                             GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_enum ("guide",
                                                          NULL,
                                                          _("Composition guides such as rule of thirds"),
                                                          GIMP_TYPE_GUIDES_TYPE,
                                                          GIMP_GUIDES_NONE,
                                                          GIMP_CONFIG_PARAM_FLAGS |
                                                          GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("x",
                                                            NULL,
                                                            _("X coordinate of top left corner"),
                                                            -GIMP_MAX_IMAGE_SIZE,
                                                            GIMP_MAX_IMAGE_SIZE,
                                                            0.0,
                                                            GIMP_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("y",
                                                            NULL,
                                                            _("Y coordinate of top left corner"),
                                                            -GIMP_MAX_IMAGE_SIZE,
                                                            GIMP_MAX_IMAGE_SIZE,
                                                            0.0,
                                                            GIMP_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("width",
                                                            NULL,
                                                            _("Width of selection"),
                                                            0.0, GIMP_MAX_IMAGE_SIZE,
                                                            0.0,
                                                            GIMP_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("height",
                                                            NULL,
                                                            _("Height of selection"),
                                                            0.0, GIMP_MAX_IMAGE_SIZE,
                                                            0.0,
                                                            GIMP_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT));

  g_object_interface_install_property (iface,
                                       gimp_param_spec_unit ("position-unit",
                                                             NULL,
                                                             _("Unit of top left corner coordinate"),
                                                             TRUE, TRUE,
                                                             gimp_unit_pixel (),
                                                             GIMP_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT));

  g_object_interface_install_property (iface,
                                       gimp_param_spec_unit ("size-unit",
                                                             NULL,
                                                             _("Unit of selection size"),
                                                             TRUE, TRUE,
                                                             gimp_unit_pixel (),
                                                             GIMP_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("fixed-rule-active",
                                                             NULL,
                                                             _("Enable lock of aspect ratio, "
                                                               "width, height or size"),
                                                             FALSE,
                                                             GIMP_CONFIG_PARAM_FLAGS |
                                                             GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_enum ("fixed-rule",
                                                          NULL,
                                                          _("Choose what has to be locked"),
                                                          GIMP_TYPE_RECTANGLE_FIXED_RULE,
                                                          GIMP_RECTANGLE_FIXED_ASPECT,
                                                          GIMP_CONFIG_PARAM_FLAGS |
                                                          GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("desired-fixed-width",
                                                            NULL,
                                                            _("Custom fixed width"),
                                                            0.0, GIMP_MAX_IMAGE_SIZE,
                                                            100.0,
                                                            GIMP_CONFIG_PARAM_FLAGS |
                                                            GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("desired-fixed-height",
                                                            NULL,
                                                            _("Custom fixed height"),
                                                            0.0, GIMP_MAX_IMAGE_SIZE,
                                                            100.0,
                                                            GIMP_CONFIG_PARAM_FLAGS |
                                                            GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("desired-fixed-size-width",
                                                            NULL, NULL,
                                                            0.0, GIMP_MAX_IMAGE_SIZE,
                                                            100.0,
                                                            GIMP_CONFIG_PARAM_FLAGS |
                                                            GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("desired-fixed-size-height",
                                                            NULL, NULL,
                                                            0.0, GIMP_MAX_IMAGE_SIZE,
                                                            100.0,
                                                            GIMP_CONFIG_PARAM_FLAGS |
                                                            GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("default-fixed-size-width",
                                                            NULL, NULL,
                                                            0.0, GIMP_MAX_IMAGE_SIZE,
                                                            100.0,
                                                            GIMP_PARAM_READWRITE |
                                                            GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("default-fixed-size-height",
                                                            NULL, NULL,
                                                            0.0, GIMP_MAX_IMAGE_SIZE,
                                                            100.0,
                                                            GIMP_PARAM_READWRITE |
                                                            GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("overridden-fixed-size",
                                                             NULL, NULL,
                                                             FALSE,
                                                             GIMP_CONFIG_PARAM_FLAGS |
                                                             GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("aspect-numerator",
                                                            NULL, NULL,
                                                            0.0, GIMP_MAX_IMAGE_SIZE,
                                                            1.0,
                                                            GIMP_CONFIG_PARAM_FLAGS |
                                                            GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("aspect-denominator",
                                                            NULL, NULL,
                                                            0.0, GIMP_MAX_IMAGE_SIZE,
                                                            1.0,
                                                            GIMP_CONFIG_PARAM_FLAGS |
                                                            GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("default-aspect-numerator",
                                                            NULL, NULL,
                                                            0.001, GIMP_MAX_IMAGE_SIZE,
                                                            1.0,
                                                            GIMP_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("default-aspect-denominator",
                                                            NULL, NULL,
                                                            0.001, GIMP_MAX_IMAGE_SIZE,
                                                            1.0,
                                                            GIMP_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("overridden-fixed-aspect",
                                                             NULL, NULL,
                                                             FALSE,
                                                             GIMP_CONFIG_PARAM_FLAGS |
                                                             GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("use-string-current",
                                                             NULL, NULL,
                                                             FALSE,
                                                             GIMP_PARAM_READWRITE |
                                                             GIMP_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       gimp_param_spec_unit ("fixed-unit",
                                                             NULL,
                                                             _("Unit of fixed width, height or size"),
                                                             TRUE, TRUE,
                                                             gimp_unit_pixel (),
                                                             GIMP_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("fixed-center",
                                                             _("Expand from center"),
                                                             _("Expand selection from center outwards"),
                                                             FALSE,
                                                             GIMP_CONFIG_PARAM_FLAGS |
                                                             GIMP_PARAM_STATIC_STRINGS));
}

static void
gimp_rectangle_options_private_finalize (GimpRectangleOptionsPrivate *private)
{
  if (private->shrink_object)
    g_object_weak_unref (private->shrink_object,
                         gimp_rectangle_options_shrink_owner_gone,
                         private->action_options);

  rectangle_metrics_clear (private);

  g_clear_object (&private->aspect_history);
  g_clear_object (&private->size_history);

  g_slice_free (GimpRectangleOptionsPrivate, private);
}

GimpRectangleOptionsPrivate *
gimp_rectangle_options_get_private (GimpRectangleOptions *options)
{
  GimpRectangleOptionsPrivate *private;

  static GQuark private_key = 0;

  g_return_val_if_fail (GIMP_IS_RECTANGLE_OPTIONS (options), NULL);

  if (! private_key)
    private_key = g_quark_from_static_string ("gimp-rectangle-options-private");

  private = g_object_get_qdata (G_OBJECT (options), private_key);

  if (! private)
    {
      private = g_slice_new0 (GimpRectangleOptionsPrivate);
      private->action_options = options;

      private->aspect_history = gtk_list_store_new (N_COLUMNS,
                                                    G_TYPE_DOUBLE,
                                                    G_TYPE_DOUBLE,
                                                    G_TYPE_STRING);

      private->size_history = gtk_list_store_new (N_COLUMNS,
                                                  G_TYPE_DOUBLE,
                                                  G_TYPE_DOUBLE,
                                                  G_TYPE_STRING);

      g_object_set_qdata_full (G_OBJECT (options), private_key, private,
                               (GDestroyNotify) gimp_rectangle_options_private_finalize);
    }

  return private;
}

/**
 * gimp_rectangle_options_install_properties:
 * @klass: the class structure for a type deriving from #GObject
 *
 * Installs the necessary properties for a class implementing
 * #GimpRectangleOptions. A #GimpRectangleOptionsProp property is installed
 * for each property, using the values from the #GimpRectangleOptionsProp
 * enumeration. The caller must make sure itself that the enumeration
 * values don't collide with some other property values they
 * are using (that's what %GIMP_RECTANGLE_OPTIONS_PROP_LAST is good for).
 **/
void
gimp_rectangle_options_install_properties (GObjectClass *klass)
{
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_AUTO_SHRINK,
                                    "auto-shrink");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_SHRINK_MERGED,
                                    "shrink-merged");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_GUIDE,
                                    "guide");

  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_X,
                                    "x");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_Y,
                                    "y");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_WIDTH,
                                    "width");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_HEIGHT,
                                    "height");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_POSITION_UNIT,
                                    "position-unit");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_SIZE_UNIT,
                                    "size-unit");

  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_FIXED_RULE_ACTIVE,
                                    "fixed-rule-active");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_FIXED_RULE,
                                    "fixed-rule");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_WIDTH,
                                    "desired-fixed-width");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_HEIGHT,
                                    "desired-fixed-height");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_SIZE_WIDTH,
                                    "desired-fixed-size-width");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_SIZE_HEIGHT,
                                    "desired-fixed-size-height");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_FIXED_SIZE_WIDTH,
                                    "default-fixed-size-width");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_FIXED_SIZE_HEIGHT,
                                    "default-fixed-size-height");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_OVERRIDDEN_FIXED_SIZE,
                                    "overridden-fixed-size");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_ASPECT_NUMERATOR,
                                    "aspect-numerator");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_ASPECT_DENOMINATOR,
                                    "aspect-denominator");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_ASPECT_NUMERATOR,
                                    "default-aspect-numerator");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_ASPECT_DENOMINATOR,
                                    "default-aspect-denominator");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_OVERRIDDEN_FIXED_ASPECT,
                                    "overridden-fixed-aspect");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_USE_STRING_CURRENT,
                                    "use-string-current");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_FIXED_UNIT,
                                    "fixed-unit");

  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_FIXED_CENTER,
                                    "fixed-center");
}

void
gimp_rectangle_options_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GimpRectangleOptions        *options  = GIMP_RECTANGLE_OPTIONS (object);
  GimpRectangleOptionsPrivate *private;

  private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options);

  switch (property_id)
    {
    case GIMP_RECTANGLE_OPTIONS_PROP_AUTO_SHRINK:
      private->auto_shrink = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_SHRINK_MERGED:
      private->shrink_merged = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_HIGHLIGHT:
      private->highlight = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_HIGHLIGHT_OPACITY:
      private->highlight_opacity = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_GUIDE:
      private->guide = g_value_get_enum (value);
      break;

    case GIMP_RECTANGLE_OPTIONS_PROP_X:
      private->x = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_Y:
      private->y = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_WIDTH:
      private->width = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_HEIGHT:
      private->height = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_POSITION_UNIT:
      private->position_unit = g_value_get_object (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_SIZE_UNIT:
      private->size_unit = g_value_get_object (value);
      break;

    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_RULE_ACTIVE:
      private->fixed_rule_active = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_RULE:
      private->fixed_rule = g_value_get_enum (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_WIDTH:
      private->desired_fixed_width = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_HEIGHT:
      private->desired_fixed_height = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_SIZE_WIDTH:
      private->desired_fixed_size_width = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_SIZE_HEIGHT:
      private->desired_fixed_size_height = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_FIXED_SIZE_WIDTH:
      private->default_fixed_size_width = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_FIXED_SIZE_HEIGHT:
      private->default_fixed_size_height = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_OVERRIDDEN_FIXED_SIZE:
      private->overridden_fixed_size = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_ASPECT_NUMERATOR:
      private->aspect_numerator = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_ASPECT_DENOMINATOR:
      private->aspect_denominator = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_ASPECT_NUMERATOR:
      private->default_aspect_numerator = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_ASPECT_DENOMINATOR:
      private->default_aspect_denominator = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_OVERRIDDEN_FIXED_ASPECT:
      private->overridden_fixed_aspect = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_USE_STRING_CURRENT:
      private->use_string_current = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_UNIT:
      private->fixed_unit = g_value_get_object (value);
      break;

    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_CENTER:
      private->fixed_center = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

void
gimp_rectangle_options_get_property (GObject      *object,
                                     guint         property_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  GimpRectangleOptions        *options  = GIMP_RECTANGLE_OPTIONS (object);
  GimpRectangleOptionsPrivate *private;

  private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options);

  switch (property_id)
    {
    case GIMP_RECTANGLE_OPTIONS_PROP_AUTO_SHRINK:
      g_value_set_boolean (value, private->auto_shrink);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_SHRINK_MERGED:
      g_value_set_boolean (value, private->shrink_merged);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_HIGHLIGHT:
      g_value_set_boolean (value, private->highlight);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_HIGHLIGHT_OPACITY:
      g_value_set_double (value, private->highlight_opacity);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_GUIDE:
      g_value_set_enum (value, private->guide);
      break;

    case GIMP_RECTANGLE_OPTIONS_PROP_X:
      g_value_set_double (value, private->x);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_Y:
      g_value_set_double (value, private->y);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_WIDTH:
      g_value_set_double (value, private->width);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_HEIGHT:
      g_value_set_double (value, private->height);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_POSITION_UNIT:
      g_value_set_object (value, private->position_unit);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_SIZE_UNIT:
      g_value_set_object (value, private->size_unit);
      break;

    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_RULE_ACTIVE:
      g_value_set_boolean (value, private->fixed_rule_active);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_RULE:
      g_value_set_enum (value, private->fixed_rule);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_WIDTH:
      g_value_set_double (value, private->desired_fixed_width);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_HEIGHT:
      g_value_set_double (value, private->desired_fixed_height);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_SIZE_WIDTH:
      g_value_set_double (value, private->desired_fixed_size_width);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_SIZE_HEIGHT:
      g_value_set_double (value, private->desired_fixed_size_height);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_FIXED_SIZE_WIDTH:
      g_value_set_double (value, private->default_fixed_size_width);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_FIXED_SIZE_HEIGHT:
      g_value_set_double (value, private->default_fixed_size_height);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_OVERRIDDEN_FIXED_SIZE:
      g_value_set_boolean (value, private->overridden_fixed_size);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_ASPECT_NUMERATOR:
      g_value_set_double (value, private->aspect_numerator);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_ASPECT_DENOMINATOR:
      g_value_set_double (value, private->aspect_denominator);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_ASPECT_NUMERATOR:
      g_value_set_double (value, private->default_aspect_numerator);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_ASPECT_DENOMINATOR:
      g_value_set_double (value, private->default_aspect_denominator);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_OVERRIDDEN_FIXED_ASPECT:
      g_value_set_boolean (value, private->overridden_fixed_aspect);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_USE_STRING_CURRENT:
      g_value_set_boolean (value, private->use_string_current);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_UNIT:
      g_value_set_object (value, private->fixed_unit);
      break;

    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_CENTER:
      g_value_set_boolean (value, private->fixed_center);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

/**
 * gimp_rectangle_options_get_size_entry:
 * @rectangle_options:
 *
 * Returns: GtkEntry used to enter desired size of rectangle. For
 *          testing purposes.
 **/
GtkWidget *
gimp_rectangle_options_get_size_entry (GimpRectangleOptions *rectangle_options)
{
  GimpRectangleOptionsPrivate *private;

  private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (rectangle_options);

  return private->size_entry;
}

/**
 * gimp_rectangle_options_fixed_rule_changed:
 * @widget:
 * @private:
 *
 * Updates tool options widgets depending on current fixed rule state.
 */
static void
gimp_rectangle_options_fixed_rule_changed (GtkWidget                   *widget,
                                           GimpRectangleOptionsPrivate *private)
{
  /* Update fixed_rule_active if checkbox was changed */
  if (GTK_IS_TOGGLE_BUTTON (widget))
    private->fixed_rule_active =
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

  /* Setup sensitivity for Width and Height entries */

  gtk_widget_set_sensitive (gimp_size_entry_get_help_widget (
                              GIMP_SIZE_ENTRY (private->size_entry), 0),
                            ! (private->fixed_rule_active &&
                               (private->fixed_rule ==
                                GIMP_RECTANGLE_FIXED_WIDTH ||
                                private->fixed_rule ==
                                GIMP_RECTANGLE_FIXED_SIZE)));

  gtk_widget_set_sensitive (gimp_size_entry_get_help_widget (
                              GIMP_SIZE_ENTRY (private->size_entry), 1),
                            ! (private->fixed_rule_active &&
                               (private->fixed_rule ==
                                GIMP_RECTANGLE_FIXED_HEIGHT ||
                                private->fixed_rule ==
                                GIMP_RECTANGLE_FIXED_SIZE)));

  /* Setup current fixed rule entries */

  gtk_widget_hide (private->fixed_width_entry);
  gtk_widget_hide (private->fixed_height_entry);
  gtk_widget_hide (private->fixed_aspect_hbox);
  gtk_widget_hide (private->fixed_size_hbox);

  switch (private->fixed_rule)
    {
    case GIMP_RECTANGLE_FIXED_ASPECT:
      gtk_widget_show (private->fixed_aspect_hbox);
      break;

    case GIMP_RECTANGLE_FIXED_WIDTH:
      gtk_widget_show (private->fixed_width_entry);
      break;

    case GIMP_RECTANGLE_FIXED_HEIGHT:
      gtk_widget_show (private->fixed_height_entry);
      break;

    case GIMP_RECTANGLE_FIXED_SIZE:
      gtk_widget_show (private->fixed_size_hbox);
      break;
    }
}

static void
gimp_rectangle_options_string_current_updates (GimpNumberPairEntry  *entry,
                                               GParamSpec           *param,
                                               GimpRectangleOptions *rectangle_options)
{
  GimpRectangleOptionsPrivate *private;
  gboolean                     user_override;

  private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (rectangle_options);

  user_override = gimp_number_pair_entry_get_user_override (entry);

  gimp_number_pair_entry_set_default_text (entry,
                                           private->use_string_current ?
                                           /* Current, as in what is currently in use. */
                                           _("Current") : NULL);

  gtk_widget_set_sensitive (private->aspect_button_box,
                            ! private->use_string_current || user_override);
}

static GtkWidget *
gimp_rectangle_options_prop_dimension_frame_new (GObject      *config,
                                                 const gchar  *x_property_name,
                                                 const gchar  *y_property_name,
                                                 const gchar  *unit_property_name,
                                                 const gchar  *table_label,
                                                 GtkSizeGroup *label_group,
                                                 GtkWidget   **entry)
{
  GimpUnit      *unit_value;
  GtkWidget     *frame;
  GtkWidget     *hbox;
  GtkWidget     *label;
  GtkWidget     *menu;
  GtkWidget     *spinbutton;
  GtkAdjustment *adjustment;

  g_object_get (config,
                unit_property_name, &unit_value,
                NULL);

  frame = gimp_frame_new (NULL);

  /*  title  */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_frame_set_label_widget (GTK_FRAME (frame), hbox);
  gtk_widget_show (hbox);

  label = gtk_label_new (table_label);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_size_group_add_widget (label_group, label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  menu = gimp_prop_unit_combo_box_new (config, unit_property_name);
  gtk_box_pack_end (GTK_BOX (hbox), menu, FALSE, FALSE, 0);

  /*  content  */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);

  adjustment = gtk_adjustment_new (1, 1, 1, 1, 10, 0);
  spinbutton = gimp_spin_button_new (adjustment, 1.0, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton), TRUE);
  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton), SB_WIDTH);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_widget_show (spinbutton);

  *entry = gimp_size_entry_new (1, unit_value, "%a", TRUE, TRUE, FALSE,
                                SB_WIDTH, GIMP_SIZE_ENTRY_UPDATE_SIZE);
  gtk_grid_set_column_spacing (GTK_GRID (*entry), 0);
  gimp_size_entry_show_unit_menu (GIMP_SIZE_ENTRY (*entry), FALSE);
  gimp_size_entry_add_field (GIMP_SIZE_ENTRY (*entry),
                             GTK_SPIN_BUTTON (spinbutton), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), *entry, FALSE, FALSE, 0);
  gtk_widget_show (*entry);

  gimp_prop_coordinates_connect (config,
                                 x_property_name, y_property_name,
                                 unit_property_name,
                                 *entry, NULL, 300, 300);

  return frame;
}

GtkWidget *
gimp_rectangle_options_gui (GimpToolOptions *tool_options)
{
  GimpRectangleOptionsPrivate *private;
  GObject                     *config = G_OBJECT (tool_options);
  GtkWidget                   *vbox   = gimp_tool_options_gui (tool_options);
  GtkWidget                   *button;
  GtkWidget                   *combo;
  GtkWidget                   *frame;
  GtkSizeGroup                *label_group;

  private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (tool_options);

  /* Fixed Center */
  button = gimp_prop_check_button_new (config, "fixed-center", NULL);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  /* Rectangle fixed-rules (e.g. aspect or width). */
  {
    GtkWidget    *vbox2;
    GtkWidget    *hbox;
    GtkWidget    *entry;
    GtkSizeGroup *size_group;
    GList        *children;

    frame = gimp_frame_new (NULL);
    gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
    gtk_widget_show (frame);

    /* Setup frame title widgets */

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_frame_set_label_widget (GTK_FRAME (frame), hbox);
    gtk_widget_show (hbox);

    button = gimp_prop_check_button_new (config, "fixed-rule-active", NULL);
    gtk_widget_destroy (gtk_bin_get_child (GTK_BIN (button)));
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);

    g_signal_connect (button, "toggled",
                      G_CALLBACK (gimp_rectangle_options_fixed_rule_changed),
                      private);

    combo = gimp_prop_enum_combo_box_new (config, "fixed-rule", 0, 0);
    gimp_int_combo_box_set_label (GIMP_INT_COMBO_BOX (combo), _("Fixed"));
    g_object_set (combo, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);

    g_signal_connect (combo, "changed",
                      G_CALLBACK (gimp_rectangle_options_fixed_rule_changed),
                      private);

    /* Setup frame content */

    vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (frame), vbox2);
    gtk_widget_show (vbox2);

    size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

    /* Fixed aspect entry/buttons */
    g_set_weak_pointer (&private->fixed_aspect_hbox,
                        gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2));
    gtk_box_pack_start (GTK_BOX (vbox2), private->fixed_aspect_hbox,
                        FALSE, FALSE, 0);
    gtk_size_group_add_widget (size_group, private->fixed_aspect_hbox);
    g_object_unref (size_group);
    /* don't show */

    entry = gimp_prop_number_pair_entry_new (config,
                                             "aspect-numerator",
                                             "aspect-denominator",
                                             "default-aspect-numerator",
                                             "default-aspect-denominator",
                                             "overridden-fixed-aspect",
                                             FALSE, TRUE,
                                             ":/" "xX*",
                                             TRUE,
                                             0.001, GIMP_MAX_IMAGE_SIZE);
    gtk_box_pack_start (GTK_BOX (private->fixed_aspect_hbox), entry,
                        TRUE, TRUE, 0);

    g_signal_connect (entry, "notify::user-override",
                      G_CALLBACK (gimp_rectangle_options_string_current_updates),
                      config);
    g_signal_connect_swapped (config, "notify::use-string-current",
                              G_CALLBACK (gimp_rectangle_options_string_current_updates),
                              entry);

    gimp_rectangle_options_setup_ratio_completion (GIMP_RECTANGLE_OPTIONS (tool_options),
                                                   entry,
                                                   private->aspect_history);

    g_set_weak_pointer (&private->aspect_button_box,
                        gimp_prop_enum_icon_box_new (G_OBJECT (entry),
                                                     "aspect", "gimp", -1, -1));
    gtk_box_pack_start (GTK_BOX (private->fixed_aspect_hbox),
                        private->aspect_button_box, FALSE, FALSE, 0);

    /* hide "square" */
    children =
      gtk_container_get_children (GTK_CONTAINER (private->aspect_button_box));
    gtk_widget_hide (children->data);
    g_list_free (children);

    /* Fixed width entry */
    g_set_weak_pointer (&private->fixed_width_entry,
                        gimp_prop_size_entry_new (config,
                                                  "desired-fixed-width",
                                                  TRUE, "fixed-unit", "%a",
                                                  GIMP_SIZE_ENTRY_UPDATE_SIZE,
                                                  300));
    gtk_box_pack_start (GTK_BOX (vbox2), private->fixed_width_entry,
                        FALSE, FALSE, 0);
    gtk_size_group_add_widget (size_group, private->fixed_width_entry);
    gtk_widget_hide (private->fixed_width_entry);

    /* Fixed height entry */
    g_set_weak_pointer (&private->fixed_height_entry,
                        gimp_prop_size_entry_new (config,
                                                  "desired-fixed-height",
                                                  TRUE, "fixed-unit", "%a",
                                                  GIMP_SIZE_ENTRY_UPDATE_SIZE,
                                                  300));
    gtk_box_pack_start (GTK_BOX (vbox2), private->fixed_height_entry,
                        FALSE, FALSE, 0);
    gtk_size_group_add_widget (size_group, private->fixed_height_entry);
    gtk_widget_hide (private->fixed_height_entry);

    /* Fixed size entry */
    g_set_weak_pointer (&private->fixed_size_hbox,
                        gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2));
    gtk_box_pack_start (GTK_BOX (vbox2), private->fixed_size_hbox,
                        FALSE, FALSE, 0);
    gtk_size_group_add_widget (size_group, private->fixed_size_hbox);
    gtk_widget_hide (private->fixed_size_hbox);

    entry = gimp_prop_number_pair_entry_new (config,
                                             "desired-fixed-size-width",
                                             "desired-fixed-size-height",
                                             "default-fixed-size-width",
                                             "default-fixed-size-height",
                                             "overridden-fixed-size",
                                             TRUE, FALSE,
                                             "xX*" ":/",
                                             FALSE,
                                             1, GIMP_MAX_IMAGE_SIZE);
    gtk_box_pack_start (GTK_BOX (private->fixed_size_hbox), entry,
                        TRUE, TRUE, 0);

    gimp_rectangle_options_setup_ratio_completion (GIMP_RECTANGLE_OPTIONS (tool_options),
                                                   entry,
                                                   private->size_history);

    private->size_button_box =
      gimp_prop_enum_icon_box_new (G_OBJECT (entry),
                                   "aspect", "gimp", -1, -1);
    gtk_box_pack_start (GTK_BOX (private->fixed_size_hbox),
                        private->size_button_box, FALSE, FALSE, 0);

    /* hide "square" */
    children =
      gtk_container_get_children (GTK_CONTAINER (private->size_button_box));
    gtk_widget_hide (children->data);
    g_list_free (children);
  }

  label_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  /* X, Y */
  frame = gimp_rectangle_options_prop_dimension_frame_new (config,
                                                           "x", "y",
                                                           "position-unit",
                                                           _("Position:"),
                                                           label_group,
                                                           &private->position_entry);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /* Width, Height */
  frame = gimp_rectangle_options_prop_dimension_frame_new (config,
                                                           "width", "height",
                                                           "size-unit",
                                                           _("Size:"),
                                                           label_group,
                                                           &private->size_entry);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  g_object_unref (label_group);

  /*  the Highlight frame  */
  {
    GtkWidget *scale;

    scale = gimp_prop_spin_scale_new (config, "highlight-opacity",
                                      0.01, 0.1, 0);
    gimp_prop_widget_set_factor (scale, 100.0, 1.0, 10.0, 1);

    frame = gimp_prop_expanding_frame_new (config, "highlight", NULL,
                                           scale, NULL);
    gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  }

  /*  Guide  */
  combo = gimp_prop_enum_combo_box_new (config, "guide",
                                        GIMP_GUIDES_NONE,
                                        GIMP_GUIDES_DIAGONALS);
  gtk_box_pack_start (GTK_BOX (vbox), combo, FALSE, FALSE, 0);

  /*  Auto Shrink  */
  g_set_weak_pointer (&private->auto_shrink_button,
                      gimp_rectangle_options_auto_shrink_button_new (GIMP_RECTANGLE_OPTIONS (config)));
  gtk_box_pack_start (GTK_BOX (vbox), private->auto_shrink_button,
                      FALSE, FALSE, 0);
  gtk_widget_show (private->auto_shrink_button);

  button = gimp_prop_check_button_new (config, "shrink-merged", NULL);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  /* Setup initial fixed rule widgets */
  gimp_rectangle_options_fixed_rule_changed (NULL, private);

  return vbox;
}

/* Independent fields observe the current native rectangle context. They never
 * become canonical converters: only the ordinary dock's existing connect path
 * preserves physical intent when changing image resolution. */
typedef struct
{
  guint                 references;
  GimpRectangleOptions *options;  /* weak */
  GimpSizeEntry        *entry;    /* weak; entry owns the destroy closure */
  gint                  field;
  GtkOrientation        axis;
  gulong                handler;
} RectangleSizeBinding;

static void
rectangle_metrics_image_gone (gpointer data,
                              GObject *where_image_was)
{
  GimpRectangleOptionsPrivate *private = data;

  private->metrics_image = NULL;
  private->metrics_resolution_handler = 0;
  private->metrics_size_handler = 0;
  private->metrics_serial++;
}

static void
rectangle_metrics_clear (GimpRectangleOptionsPrivate *private)
{
  if (private->metrics_image)
    {
      /* Object-bound closures may already be invalidated during options
       * finalization, before the private data destructor runs. */
      if (g_signal_handler_is_connected (private->metrics_image,
                                          private->metrics_resolution_handler))
        g_signal_handler_disconnect (private->metrics_image,
                                     private->metrics_resolution_handler);
      if (g_signal_handler_is_connected (private->metrics_image,
                                          private->metrics_size_handler))
        g_signal_handler_disconnect (private->metrics_image,
                                     private->metrics_size_handler);
      g_object_weak_unref (G_OBJECT (private->metrics_image),
                           rectangle_metrics_image_gone, private);
      private->metrics_image = NULL;
      private->metrics_resolution_handler = 0;
      private->metrics_size_handler = 0;
    }
  private->metrics_serial++;
}

static void
rectangle_metrics_changed (GimpImage            *image,
                            GimpRectangleOptions *options)
{
  GimpRectangleOptionsPrivate *private =
    GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options);

  if (private->metrics_image == image)
    {
      private->metrics_serial++;
      g_signal_emit (options, rectangle_signals[SIZE_METRICS_CHANGED], 0);
    }
}

static void
rectangle_metrics_set_image (GimpRectangleOptions *options,
                              GimpImage            *image)
{
  GimpRectangleOptionsPrivate *private =
    GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options);

  if (private->metrics_image != image)
    {
      rectangle_metrics_clear (private);
      private->metrics_image = image;
      g_object_weak_ref (G_OBJECT (image), rectangle_metrics_image_gone, private);
      private->metrics_resolution_handler =
        g_signal_connect_object (image, "resolution-changed",
                                  G_CALLBACK (rectangle_metrics_changed), options, 0);
      private->metrics_size_handler =
        g_signal_connect_object (image, "size-changed",
                                  G_CALLBACK (rectangle_metrics_changed), options, 0);
    }
  private->metrics_serial++;
}

static void
rectangle_size_binding_refresh (GimpRectangleOptions *options,
                                  RectangleSizeBinding *binding)
{
  GimpRectangleOptionsPrivate *private =
    GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options);
  GimpSizeEntry  *entry = binding->entry;
  GtkWidget     *spin;
  GtkAdjustment *adjustment;
  guint64        serial = private->metrics_serial;
  gdouble        xres, yres;
  gint           extent;
  gint           field = binding->field;

  if (! private->metrics_image || ! entry)
    return;

  gimp_image_get_resolution (private->metrics_image, &xres, &yres);
  extent = binding->axis == GTK_ORIENTATION_HORIZONTAL ?
           gimp_image_get_width (private->metrics_image) :
           gimp_image_get_height (private->metrics_image);

  binding->references++;
  g_object_ref (options);
  g_object_ref (entry);
  /* Native setters continue using the field's adjustment after emitting its
   * signals. A nested destroy can release the child before the setter returns. */
  spin = g_object_ref (gimp_size_entry_get_help_widget (entry, field));
  adjustment = g_object_ref (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (spin)));
  /* Native property helpers emit even for unchanged refvals. Metadata is a
   * passive reflection, so suppress their write-back throughout the refresh. */
  g_signal_handlers_block_matched (entry, G_SIGNAL_MATCH_DATA,
                                   0, 0, NULL, NULL, options);
  /* Set the extent first: resolution refresh also rebuilds percent display
   * boundaries, which must use the new extent rather than the previous one. */
  gimp_size_entry_set_size (entry, field, 0, extent);
  /* A signal observer may destroy the view or replace the current context. */
  if (binding->entry && serial == private->metrics_serial)
    gimp_size_entry_set_resolution (entry, field,
                                    binding->axis == GTK_ORIENTATION_HORIZONTAL ?
                                    xres : yres, TRUE);
  g_signal_handlers_unblock_matched (entry, G_SIGNAL_MATCH_DATA,
                                     0, 0, NULL, NULL, options);
  g_object_unref (adjustment);
  g_object_unref (spin);
  g_object_unref (entry);
  g_object_unref (options);
  if (--binding->references == 0)
    g_free (binding);
}

static void
rectangle_size_binding_options_gone (gpointer data,
                                      GObject *where_options_was)
{
  RectangleSizeBinding *binding = data;

  /* The native one-field helper uses raw config callback data. Make retained
   * entries inert if their model disappears before the widget. */
  if (binding->entry)
    g_signal_handlers_disconnect_by_data (binding->entry, where_options_was);
  binding->options = NULL;
  binding->handler = 0;
}

static void
rectangle_size_binding_destroy (GtkWidget            *widget,
                                 RectangleSizeBinding *binding)
{
  if (binding->options)
    {
      g_signal_handler_disconnect (binding->options, binding->handler);
      g_signal_handlers_disconnect_by_data (binding->options, widget);
      g_signal_handlers_disconnect_by_data (widget, binding->options);
      g_object_weak_unref (G_OBJECT (binding->options),
                           rectangle_size_binding_options_gone, binding);
      binding->options = NULL;
      binding->handler = 0;
    }
  binding->entry = NULL;
}

static void
rectangle_size_binding_free (gpointer data,
                              GClosure *closure)
{
  RectangleSizeBinding *binding = data;

  if (--binding->references == 0)
    g_free (binding);
}

void
gimp_rectangle_options_bind_size_entry (GimpRectangleOptions *options,
                                        GimpSizeEntry        *entry,
                                        gint                  field,
                                        GtkOrientation        axis)
{
  RectangleSizeBinding *binding;

  g_return_if_fail (GIMP_IS_RECTANGLE_OPTIONS (options));
  g_return_if_fail (GIMP_IS_SIZE_ENTRY (entry));
  g_return_if_fail (field >= 0 && field < gimp_size_entry_get_n_fields (entry));
  g_return_if_fail (axis == GTK_ORIENTATION_HORIZONTAL ||
                    axis == GTK_ORIENTATION_VERTICAL);

  binding = g_new0 (RectangleSizeBinding, 1);
  binding->references = 1;
  binding->options = options;
  binding->entry = entry;
  binding->field = field;
  binding->axis = axis;
  binding->handler =
    g_signal_connect (options, "size-metrics-changed",
                       G_CALLBACK (rectangle_size_binding_refresh), binding);
  g_object_weak_ref (G_OBJECT (options),
                     rectangle_size_binding_options_gone, binding);
  g_signal_connect_data (entry, "destroy",
                          G_CALLBACK (rectangle_size_binding_destroy), binding,
                          rectangle_size_binding_free, 0);
  rectangle_size_binding_refresh (options, binding);
}

static void
gimp_rectangle_options_shrink_owner_gone (gpointer data,
                                          GObject *where_object_was)
{
  GimpRectangleOptions *options = data;
  GimpRectangleOptionsPrivate *private =
    GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options);

  rectangle_metrics_clear (private);

  /* The object-bound request closure is invalidated by the same finalization. */
  private->shrink_object = NULL;
  private->shrink_callback = NULL;
  private->shrink_handler = 0;
  g_signal_emit (options, rectangle_signals[AUTO_SHRINK_CHANGED], 0);
}

static void
gimp_rectangle_options_shrink_changed (GimpRectangleOptions *options,
                                        GtkWidget            *button)
{
  gtk_widget_set_sensitive (button,
                            GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options)->shrink_object != NULL);
}

static void
gimp_rectangle_options_shrink_clicked (GtkWidget            *button,
                                        GimpRectangleOptions *options)
{
  if (GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options)->shrink_object)
    g_signal_emit (options, rectangle_signals[AUTO_SHRINK_REQUESTED], 0);
}

static void
gimp_rectangle_options_shrink_button_destroy (GtkWidget            *button,
                                               GimpRectangleOptions *options)
{
  /* GtkWidget::destroy can precede finalization while callers retain refs. */
  g_signal_handlers_disconnect_by_func (button,
                                        gimp_rectangle_options_shrink_clicked,
                                        options);
  g_signal_handlers_disconnect_by_func (options,
                                        gimp_rectangle_options_shrink_changed,
                                        button);
  gtk_widget_set_sensitive (button, FALSE);
}

GtkWidget *
gimp_rectangle_options_auto_shrink_button_new (GimpRectangleOptions *options)
{
  GtkWidget *button;

  g_return_val_if_fail (GIMP_IS_RECTANGLE_OPTIONS (options), NULL);

  button = gtk_button_new_with_label (_("Auto Shrink"));
  g_signal_connect_object (button, "clicked",
                           G_CALLBACK (gimp_rectangle_options_shrink_clicked),
                           options, 0);
  g_signal_connect_object (button, "destroy",
                           G_CALLBACK (gimp_rectangle_options_shrink_button_destroy),
                           options, 0);
  g_signal_connect_object (options, "auto-shrink-changed",
                           G_CALLBACK (gimp_rectangle_options_shrink_changed),
                           button, 0);
  gimp_rectangle_options_shrink_changed (options, button);

  return button;
}

static void
gimp_rectangle_options_shrink_detach (GimpRectangleOptions *options)
{
  GimpRectangleOptionsPrivate *private =
    GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options);

  if (private->shrink_object)
    {
      g_object_weak_unref (private->shrink_object,
                          gimp_rectangle_options_shrink_owner_gone, options);
      g_signal_handler_disconnect (options, private->shrink_handler);
      private->shrink_handler = 0;
      private->shrink_object = NULL;
      private->shrink_callback = NULL;
    }
}

void
gimp_rectangle_options_connect (GimpRectangleOptions *options,
                                GimpImage            *image,
                                GCallback             shrink_callback,
                                gpointer              shrink_object)
{
  GimpRectangleOptionsPrivate *options_private;
  gdouble                      xres;
  gdouble                      yres;

  g_return_if_fail (GIMP_IS_RECTANGLE_OPTIONS (options));
  g_return_if_fail (GIMP_IS_IMAGE (image));
  g_return_if_fail (shrink_callback != NULL);
  g_return_if_fail (G_IS_OBJECT (shrink_object));

  options_private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options);

  gimp_image_get_resolution (image, &xres, &yres);

  if (options_private->fixed_width_entry)
    {
      GtkWidget *entry = options_private->fixed_width_entry;

      gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (entry), 0, xres, FALSE);
      gimp_size_entry_set_size (GIMP_SIZE_ENTRY (entry), 0,
                                0, gimp_image_get_width (image));
    }

  if (options_private->fixed_height_entry)
    {
      GtkWidget *entry = options_private->fixed_height_entry;

      gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (entry), 0, yres, FALSE);
      gimp_size_entry_set_size (GIMP_SIZE_ENTRY (entry), 0,
                                0, gimp_image_get_height (image));
    }

  if (options_private->position_entry)
    {
      GtkWidget *entry = options_private->position_entry;

      gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (entry), 0, xres, FALSE);
      gimp_size_entry_set_size (GIMP_SIZE_ENTRY (entry), 0,
                                0, gimp_image_get_width (image));

      gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (entry), 1, yres, FALSE);
      gimp_size_entry_set_size (GIMP_SIZE_ENTRY (entry), 1,
                                0, gimp_image_get_height (image));
    }

  if (options_private->size_entry)
    {
      GtkWidget *entry = options_private->size_entry;

      gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (entry), 0, xres, FALSE);
      gimp_size_entry_set_size (GIMP_SIZE_ENTRY (entry), 0,
                                0, gimp_image_get_width (image));

      gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (entry), 1, yres, FALSE);
      gimp_size_entry_set_size (GIMP_SIZE_ENTRY (entry), 1,
                                0, gimp_image_get_height (image));
    }

  /* Finish the single canonical dock conversion before passive view updates.
   * An identical action endpoint can still refer to a different image. */
  rectangle_metrics_set_image (options, image);

  if (options_private->shrink_object == shrink_object &&
      options_private->shrink_callback == shrink_callback)
    {
      g_signal_emit (options, rectangle_signals[SIZE_METRICS_CHANGED], 0);
      return;
    }

  /* Replace the endpoint atomically before notifying any view. */
  gimp_rectangle_options_shrink_detach (options);

  options_private->shrink_object = G_OBJECT (shrink_object);
  options_private->shrink_callback = shrink_callback;
  options_private->shrink_handler =
    g_signal_connect_object (options, "auto-shrink-requested",
                             shrink_callback, shrink_object,
                             G_CONNECT_SWAPPED);
  g_object_weak_ref (options_private->shrink_object,
                    gimp_rectangle_options_shrink_owner_gone, options);
  g_signal_emit (options, rectangle_signals[SIZE_METRICS_CHANGED], 0);
  g_signal_emit (options, rectangle_signals[AUTO_SHRINK_CHANGED], 0);
}

void
gimp_rectangle_options_disconnect (GimpRectangleOptions *options,
                                   GCallback             shrink_callback,
                                   gpointer              shrink_object)
{
  GimpRectangleOptionsPrivate *options_private;

  g_return_if_fail (GIMP_IS_RECTANGLE_OPTIONS (options));
  g_return_if_fail (shrink_callback != NULL);
  g_return_if_fail (G_IS_OBJECT (shrink_object));

  options_private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options);

  if (options_private->shrink_object == shrink_object &&
      options_private->shrink_callback == shrink_callback)
    {
      rectangle_metrics_clear (options_private);
      gimp_rectangle_options_shrink_detach (options);
      g_signal_emit (options, rectangle_signals[AUTO_SHRINK_CHANGED], 0);
    }
}

/**
 * gimp_rectangle_options_fixed_rule_active:
 * @rectangle_options:
 * @fixed_rule:
 *
 * Returns: %TRUE if @fixed_rule is active, %FALSE otherwise.
 */
gboolean
gimp_rectangle_options_fixed_rule_active (GimpRectangleOptions   *rectangle_options,
                                          GimpRectangleFixedRule  fixed_rule)
{
  GimpRectangleOptionsPrivate *private;

  g_return_val_if_fail (GIMP_IS_RECTANGLE_OPTIONS (rectangle_options), FALSE);

  private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (rectangle_options);

  return private->fixed_rule_active &&
         private->fixed_rule == fixed_rule;
}

static void
gimp_rectangle_options_setup_ratio_completion (GimpRectangleOptions *rectangle_options,
                                               GtkWidget            *entry,
                                               GtkListStore         *history)
{
  GtkEntryCompletion *completion;

  completion = g_object_new (GTK_TYPE_ENTRY_COMPLETION,
                             "model", history,
                             "inline-completion", TRUE,
                             NULL);

  gtk_entry_completion_set_text_column (completion, COLUMN_TEXT);
  gtk_entry_set_completion (GTK_ENTRY (entry), completion);
  g_object_unref (completion);

  g_signal_connect (entry, "ratio-changed",
                    G_CALLBACK (gimp_number_pair_entry_history_add),
                    history);

  g_signal_connect (completion, "match-selected",
                    G_CALLBACK (gimp_number_pair_entry_history_select),
                    entry);
}

static gboolean
gimp_number_pair_entry_history_select (GtkEntryCompletion  *completion,
                                       GtkTreeModel        *model,
                                       GtkTreeIter         *iter,
                                       GimpNumberPairEntry *entry)
{
  gdouble left_number;
  gdouble right_number;

  gtk_tree_model_get (model, iter,
                      COLUMN_LEFT_NUMBER,  &left_number,
                      COLUMN_RIGHT_NUMBER, &right_number,
                      -1);

  gimp_number_pair_entry_set_values (entry, left_number, right_number);

  return TRUE;
}

static void
gimp_number_pair_entry_history_add (GtkWidget    *entry,
                                    GtkTreeModel *model)
{
  GValue       value = G_VALUE_INIT;
  GtkTreeIter  iter;
  gboolean     iter_valid;
  gdouble      left_number;
  gdouble      right_number;
  gchar       *text;

  /* Adding or moving a row emits signals.  Keep the history and an owned
   * snapshot of the text alive if an observer closes the editing view.
   */
  g_object_ref (model);
  text = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
  gimp_number_pair_entry_get_values (GIMP_NUMBER_PAIR_ENTRY (entry),
                                     &left_number,
                                     &right_number);

  for (iter_valid = gtk_tree_model_get_iter_first (model, &iter);
       iter_valid;
       iter_valid = gtk_tree_model_iter_next (model, &iter))
    {
      gtk_tree_model_get_value (model, &iter, COLUMN_TEXT, &value);

      if (strcmp (text, g_value_get_string (&value)) == 0)
        {
          g_value_unset (&value);
          break;
        }

      g_value_unset (&value);
    }

  if (iter_valid)
    {
      gtk_list_store_move_after (GTK_LIST_STORE (model), &iter, NULL);
    }
  else
    {
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);

      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          COLUMN_LEFT_NUMBER,  left_number,
                          COLUMN_RIGHT_NUMBER, right_number,
                          COLUMN_TEXT,        text,
                          -1);

      /* FIXME: limit the size of the history */
    }

  g_free (text);
  g_object_unref (model);
}
