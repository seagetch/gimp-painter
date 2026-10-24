/* Exact old Gaussian body; only the core's history storage is adapted. */
typedef struct { GArray *stroke_buffer; } LegacySmoothingCore;
void legacy_perspective_smooth_coords (LegacySmoothingCore *, GimpPaintOptions *, GimpCoords *);
#define GimpPaintCore LegacySmoothingCore
#define gimp_paint_core_smooth_coords legacy_perspective_smooth_coords
#include "smooth-coords.inc"
#undef gimp_paint_core_smooth_coords
#undef GimpPaintCore
