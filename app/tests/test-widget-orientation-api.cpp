/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpwidgets/gimpwidgets.h"
#include "libgimpwidgets/gimpwidgets.h"
#include <type_traits>
using OrientationSignature0 = GtkWidget *(*) (GType, GCallback, gpointer, GtkWidget **, GtkOrientation);
constexpr OrientationSignature0 orientation_api_0 = &gimp_enum_radio_box_new_with_orientation;
using OrientationSignature1 = GtkWidget *(*) (GType, GtkWidget *, GCallback, gpointer, GtkWidget **, GtkOrientation);
constexpr OrientationSignature1 orientation_api_1 = &gimp_enum_radio_frame_new_with_orientation;
using OrientationSignature2 = GtkWidget *(*) (GType, gint, gint, GtkWidget *, GCallback, gpointer, GtkWidget **, GtkOrientation);
constexpr OrientationSignature2 orientation_api_2 = &gimp_enum_radio_frame_new_full;
using OrientationSignature3 = GtkWidget *(*) (GType, const gchar *, GtkIconSize, GCallback, gpointer, GtkWidget **, GtkOrientation);
constexpr OrientationSignature3 orientation_api_3 = &gimp_enum_stock_box_new_with_orientation;
using OrientationSignature4 = GtkWidget *(*) (GObject *, const gchar *, const gchar *, gint, gint, GtkOrientation);
constexpr OrientationSignature4 orientation_api_4 = &gimp_prop_enum_radio_frame_new_with_orientation;
using OrientationSignature5 = GtkWidget *(*) (GObject *, const gchar *, const gchar *, const gchar *, const gchar *, GtkOrientation);
constexpr OrientationSignature5 orientation_api_5 = &gimp_prop_boolean_radio_frame_new_with_orientation;
using OrientationSignature6 = GtkWidget *(*) (GObject *, const gchar *, const gchar *, gint, gint, GtkOrientation);
constexpr OrientationSignature6 orientation_api_6 = &gimp_prop_enum_stock_box_new_with_orientation;
using OrientationSignature7 = GtkWidget *(*) (gboolean, const gchar *, GCallback, gpointer, GtkOrientation, gint, ...);
constexpr OrientationSignature7 orientation_api_7 = &gimp_int_radio_group_new_with_orientation;
using OrientationSignature8 = GtkWidget *(*) (GType, GCallback, gpointer, GDestroyNotify, GtkWidget **);
constexpr OrientationSignature8 orientation_api_8 = &gimp_enum_radio_box_new;
using OrientationSignature9 = GtkWidget *(*) (GType, gint, gint, GtkWidget *, GCallback, gpointer, GDestroyNotify, GtkWidget **);
constexpr OrientationSignature9 orientation_api_9 = &gimp_enum_radio_frame_new_with_range;
using OrientationSignature10 = GtkWidget *(*) (GType, const gchar *, GtkIconSize, GCallback, gpointer, GDestroyNotify, GtkWidget **);
constexpr OrientationSignature10 orientation_api_10 = &gimp_enum_icon_box_new;
using OrientationSignature11 = GtkWidget *(*) (gboolean, const gchar *, GCallback, gpointer, GDestroyNotify, gint, ...);
constexpr OrientationSignature11 orientation_api_11 = &gimp_int_radio_group_new;
