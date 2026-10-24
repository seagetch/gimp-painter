/* Bounded old C circle/center/13px policy. The old functions below are unchanged.
 * Type/window adapters expose only scale and offsets; unused anchor branches
 * refuse execution. No current transform or hit helper is called. */
typedef struct { gdouble scale_x, scale_y, offset_x, offset_y; } LegacyHitShell;
#pragma push_macro("SCALEX")
#pragma push_macro("SCALEY")
#pragma push_macro("GIMP_IS_DISPLAY_SHELL")
#pragma push_macro("GIMP_IS_DRAW_TOOL")
#pragma push_macro("GIMP_IS_DISPLAY")
#undef SCALEX
#undef SCALEY
#undef GIMP_IS_DISPLAY_SHELL
#undef GIMP_IS_DRAW_TOOL
#undef GIMP_IS_DISPLAY
#define SCALEX(s,x) ((gint) floor ((x) * (s)->scale_x + 0.5))
#define SCALEY(s,y) ((gint) floor ((y) * (s)->scale_y + 0.5))
#define GIMP_IS_DISPLAY_SHELL(s) ((s) != NULL)
#define GIMP_IS_DRAW_TOOL(t) ((t) != NULL)
#define GIMP_IS_DISPLAY(d) ((d) != NULL)
#define GimpDisplayShell LegacyHitShell
#define GimpDisplay LegacyHitShell
#define GimpDrawTool int
#define gimp_display_get_shell(d) (d)
#define gimp_display_shell_zoom_xy_f legacy_hit_zoom_xy_f
#define gimp_draw_tool_on_handle legacy_hit_on_handle
static void legacy_hit_center (GimpHandleAnchor anchor, gdouble x, gdouble y,
                               gint width, gint height, gdouble *nx, gdouble *ny)
{ g_assert_cmpint (anchor, ==, GIMP_HANDLE_ANCHOR_CENTER); *nx = x; *ny = y; }
static void legacy_hit_unused_northwest (GimpHandleAnchor anchor, gdouble x, gdouble y,
                                       gint width, gint height, gdouble *nx, gdouble *ny)
{ g_assert_not_reached (); }
#define gimp_canvas_item_shift_to_center legacy_hit_center
#define gimp_canvas_item_shift_to_north_west legacy_hit_unused_northwest
void legacy_hit_zoom_xy_f (const LegacyHitShell *, gdouble, gdouble, gdouble *, gdouble *);
gboolean legacy_hit_on_handle (int *, LegacyHitShell *, gdouble, gdouble, GimpHandleType, gdouble, gdouble, gint, gint, GimpHandleAnchor);
#include "zoom.inc"
#include "on-handle.inc"
#undef gimp_canvas_item_shift_to_center
#undef gimp_canvas_item_shift_to_north_west
#undef gimp_draw_tool_on_handle
#undef gimp_display_shell_zoom_xy_f
#undef gimp_display_get_shell
#undef GimpDrawTool
#undef GimpDisplay
#undef GimpDisplayShell
#pragma pop_macro("GIMP_IS_DISPLAY")
#pragma pop_macro("GIMP_IS_DRAW_TOOL")
#pragma pop_macro("GIMP_IS_DISPLAY_SHELL")
#pragma pop_macro("SCALEY")
#pragma pop_macro("SCALEX")
