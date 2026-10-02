/* Included after the real app/header prelude by the C and C++ probes. */
#include "core/gimpdata.h"
#include "text/gimpfont.h"
void painter_header_property_probe (GObjectClass *klass);
void painter_header_property_probe (GObjectClass *klass)
{
  GIMP_CONFIG_PROP_BOOLEAN (klass, 1, "p1", "p1", "p1", FALSE, 0);
  GIMP_CONFIG_PROP_INT (klass, 2, "p2", "p2", "p2", 0, 9, 1, 0);
  GIMP_CONFIG_PROP_UINT (klass, 3, "p3", "p3", "p3", 0, 9, 1, 0);
  GIMP_CONFIG_PROP_INT64 (klass, 4, "p4", "p4", "p4", 0, 9, 1, 0);
  GIMP_CONFIG_PROP_UINT64 (klass, 5, "p5", "p5", "p5", 0, 9, 1, 0);
  GIMP_CONFIG_PROP_UNIT (klass, 6, "p6", "p6", "p6", TRUE, FALSE, NULL, 0);
  GIMP_CONFIG_PROP_MEMSIZE (klass, 7, "p7", "p7", "p7", 0, 9, 1, 0);
  GIMP_CONFIG_PROP_DOUBLE (klass, 8, "p8", "p8", "p8", 0.0, 9.0, 1.0, 0);
  GIMP_CONFIG_PROP_RESOLUTION (klass, 9, "p9", "p9", "p9", 72.0, 0);
  GIMP_CONFIG_PROP_ENUM (klass, 10, "p10", "p10", "p10", G_TYPE_ENUM, 0, 0);
  GIMP_CONFIG_PROP_STRING (klass, 11, "p11", "p11", "p11", NULL, 0);
  GIMP_CONFIG_PROP_PATH (klass, 12, "p12", "p12", "p12", GIMP_CONFIG_PATH_FILE, NULL, 0);
  GIMP_CONFIG_PROP_MATRIX2 (klass, 13, "p13", "p13", "p13", NULL, 0);
  GIMP_CONFIG_PROP_FONT (klass, 14, "p14", "p14", "p14", 0);
  GIMP_CONFIG_PROP_OBJECT (klass, 15, "p15", "p15", "p15", G_TYPE_OBJECT, 0);
  GIMP_CONFIG_PROP_COLOR (klass, 16, "p16", "p16", "p16", TRUE, NULL, 0);
  GIMP_CONFIG_PROP_BOXED (klass, 17, "p17", "p17", "p17", G_TYPE_BYTES, 0);
  GIMP_CONFIG_PROP_POINTER (klass, 18, "p18", "p18", "p18", 0);
}
