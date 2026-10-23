/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpwidgets/gimpwidgets.h"
#include "libgimpwidgets/gimpwidgets.h"
typedef GtkWidget *(*OrientationSignature0) (GType, GCallback, gpointer, GtkWidget **, GtkOrientation);
_Static_assert (_Generic (&gimp_enum_radio_box_new_with_orientation, OrientationSignature0: 1, default: 0), "gimp_enum_radio_box_new_with_orientation ABI");
typedef GtkWidget *(*OrientationSignature1) (GType, GtkWidget *, GCallback, gpointer, GtkWidget **, GtkOrientation);
_Static_assert (_Generic (&gimp_enum_radio_frame_new_with_orientation, OrientationSignature1: 1, default: 0), "gimp_enum_radio_frame_new_with_orientation ABI");
typedef GtkWidget *(*OrientationSignature2) (GType, gint, gint, GtkWidget *, GCallback, gpointer, GtkWidget **, GtkOrientation);
_Static_assert (_Generic (&gimp_enum_radio_frame_new_full, OrientationSignature2: 1, default: 0), "gimp_enum_radio_frame_new_full ABI");
typedef GtkWidget *(*OrientationSignature3) (GType, const gchar *, GtkIconSize, GCallback, gpointer, GtkWidget **, GtkOrientation);
_Static_assert (_Generic (&gimp_enum_stock_box_new_with_orientation, OrientationSignature3: 1, default: 0), "gimp_enum_stock_box_new_with_orientation ABI");
typedef GtkWidget *(*OrientationSignature4) (GObject *, const gchar *, const gchar *, gint, gint, GtkOrientation);
_Static_assert (_Generic (&gimp_prop_enum_radio_frame_new_with_orientation, OrientationSignature4: 1, default: 0), "gimp_prop_enum_radio_frame_new_with_orientation ABI");
typedef GtkWidget *(*OrientationSignature5) (GObject *, const gchar *, const gchar *, const gchar *, const gchar *, GtkOrientation);
_Static_assert (_Generic (&gimp_prop_boolean_radio_frame_new_with_orientation, OrientationSignature5: 1, default: 0), "gimp_prop_boolean_radio_frame_new_with_orientation ABI");
typedef GtkWidget *(*OrientationSignature6) (GObject *, const gchar *, const gchar *, gint, gint, GtkOrientation);
_Static_assert (_Generic (&gimp_prop_enum_stock_box_new_with_orientation, OrientationSignature6: 1, default: 0), "gimp_prop_enum_stock_box_new_with_orientation ABI");
typedef GtkWidget *(*OrientationSignature7) (gboolean, const gchar *, GCallback, gpointer, GtkOrientation, gint, ...);
_Static_assert (_Generic (&gimp_int_radio_group_new_with_orientation, OrientationSignature7: 1, default: 0), "gimp_int_radio_group_new_with_orientation ABI");
typedef GtkWidget *(*OrientationSignature8) (GType, GCallback, gpointer, GDestroyNotify, GtkWidget **);
_Static_assert (_Generic (&gimp_enum_radio_box_new, OrientationSignature8: 1, default: 0), "gimp_enum_radio_box_new ABI");
typedef GtkWidget *(*OrientationSignature9) (GType, gint, gint, GtkWidget *, GCallback, gpointer, GDestroyNotify, GtkWidget **);
_Static_assert (_Generic (&gimp_enum_radio_frame_new_with_range, OrientationSignature9: 1, default: 0), "gimp_enum_radio_frame_new_with_range ABI");
typedef GtkWidget *(*OrientationSignature10) (GType, const gchar *, GtkIconSize, GCallback, gpointer, GDestroyNotify, GtkWidget **);
_Static_assert (_Generic (&gimp_enum_icon_box_new, OrientationSignature10: 1, default: 0), "gimp_enum_icon_box_new ABI");
typedef GtkWidget *(*OrientationSignature11) (gboolean, const gchar *, GCallback, gpointer, GDestroyNotify, gint, ...);
_Static_assert (_Generic (&gimp_int_radio_group_new, OrientationSignature11: 1, default: 0), "gimp_int_radio_group_new ABI");
