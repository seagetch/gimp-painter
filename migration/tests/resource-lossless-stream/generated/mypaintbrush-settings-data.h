/* brushlib - The MyPaint Brush Library
 * Copyright (C) 2007-2011 Martin Renold <martinxyz@gmx.ch>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* DO NOT EDIT - generated from brush-settings.json by generate.py. */
#ifndef GIMP_PAINTER_MYPAINT_SETTINGS_DATA_H
#define GIMP_PAINTER_MYPAINT_SETTINGS_DATA_H

#include <float.h>
#include <stddef.h>
#include "mypaintbrush-enum-settings.h"

/* Read-only metadata, usable from C11 and C++14; no GObject or engine ownership. */
typedef struct {
  const char *name;
  int index;
  float hard_minimum, soft_minimum, normal, soft_maximum, hard_maximum;
  const char *displayed_name, *tooltip;
} GimpPainterMyPaintInput;
typedef struct {
  const char *internal_name;
  int index;
  const char *displayed_name;
  int constant;
  float minimum, default_value, maximum;
  const char *tooltip;
} GimpPainterMyPaintSetting;
typedef struct {
  const char *internal_name;
  int index;
  const char *displayed_name;
  int default_value;
} GimpPainterMyPaintSwitch;
typedef struct {
  const char *internal_name;
  int index;
  const char *displayed_name, *default_value;
} GimpPainterMyPaintText;
typedef struct {
  const char *name;
  int index;
  float constructor_default;
} GimpPainterMyPaintState;

static const GimpPainterMyPaintInput painter_mypaint_inputs[INPUT_COUNT] = {
  {"pressure", INPUT_PRESSURE, 0.0, 0.0, 0.4, 1.0, FLT_MAX, "Pressure", "The pressure reported by the tablet, between 0.0 and 1.0. If you use the mouse, it will be 0.5 when a button is pressed and 0.0 otherwise."},
  {"speed1", INPUT_SPEED1, FLT_MAX, 0.0, 0.5, 4.0, FLT_MAX, "Fine speed", "How fast you currently move. This can change very quickly. Try \"print input values\" from the \"help\" menu to get a feeling for the range; negative values are rare but possible for very low speed."},
  {"speed2", INPUT_SPEED2, FLT_MAX, 0.0, 0.5, 4.0, FLT_MAX, "Gross speed", "Same as fine speed, but changes slower. Also look at the \"gross speed filter\" setting."},
  {"random", INPUT_RANDOM, 0.0, 0.0, 0.5, 1.0, 1.0, "Random", "Fast random noise, changing at each evaluation. Evenly distributed between 0 and 1."},
  {"stroke", INPUT_STROKE, 0.0, 0.0, 0.5, 1.0, 1.0, "Stroke", "This input slowly goes from zero to one while you draw a stroke. It can also be configured to jump back to zero periodically while you move. Look at the \"stroke duration\" and \"stroke hold time\" settings."},
  {"direction", INPUT_DIRECTION, 0.0, 0.0, 0.0, 360.0, 360.0, "Direction", "The angle of the stroke, in degrees. The value will stay between 0.0 and 360.0, effectively ignoring turns of 180 degrees."},
  {"tilt_declination", INPUT_TILT_DECLINATION, 0.0, 0.0, 0.0, 90.0, 90.0, "Declination", "Declination of stylus tilt. 0 when stylus is parallel to tablet and 90.0 when it's perpendicular to tablet."},
  {"tilt_ascension", INPUT_TILT_ASCENSION, -180.0, -180.0, 0.0, 180.0, 180.0, "Ascension", "Right ascension of stylus tilt. 0 when stylus working end points to you, +90 when rotated 90 degrees clockwise, -90 when rotated 90 degrees counterclockwise."},
  {"custom", INPUT_CUSTOM, FLT_MAX, -2.0, 0.0, +2.0, FLT_MAX, "Custom", "This is a user defined input. Look at the \"custom input\" setting for details."},
};

static const GimpPainterMyPaintSetting painter_mypaint_settings[BRUSH_MAPPING_COUNT] = {
  {"opaque", BRUSH_OPAQUE, "Opacity", 0, 0.0, 1.0, 2.0, "0 means brush is transparent, 1 fully visible\n(also known as alpha or opacity)"},
  {"opaque_multiply", BRUSH_OPAQUE_MULTIPLY, "Opacity multiply", 0, 0.0, 0.0, 2.0, "This gets multiplied with opaque. You should only change the pressure input of this setting. Use \"opaque\" instead to make opacity depend on speed.\nThis setting is responsible to stop painting when there is zero pressure. This is just a convention, the behaviour is identical to \"opaque\"."},
  {"opaque_linearize", BRUSH_OPAQUE_LINEARIZE, "Opacity linearize", 1, 0.0, 0.9, 2.0, "Correct the nonlinearity introduced by blending multiple dabs on top of each other. This correction should get you a linear (\"natural\") pressure response when pressure is mapped to opaque_multiply, as it is usually done. 0.9 is good for standard strokes, set it smaller if your brush scatters a lot, or higher if you use dabs_per_second.\n0.0 the opaque value above is for the individual dabs\n1.0 the opaque value above is for the final brush stroke, assuming each pixel gets (dabs_per_radius*2) brushdabs on average during a stroke"},
  {"radius_logarithmic", BRUSH_RADIUS_LOGARITHMIC, "Radius", 0, -2.0, 2.0, 5.0, "Basic brush radius (logarithmic)\n 0.7 means 2 pixels\n 3.0 means 20 pixels"},
  {"hardness", BRUSH_HARDNESS, "Hardness", 0, 0.0, 0.8, 1.0, "Hard brush-circle borders (setting to zero will draw nothing). To reach the maximum hardness, you need to disable Anti-aliasing."},
  {"anti_aliasing", BRUSH_ANTI_ALIASING, "Anti-aliasing", 0, 0.0, 1.0, 5.0, "This setting decreases the hardness when necessary to prevent a pixel staircase effect.\n 0.0 disable (for very strong erasers and pixel brushes)\n 1.0 blur one pixel (good value)\n 5.0 notable blur, thin strokes will disappear"},
  {"dabs_per_basic_radius", BRUSH_DABS_PER_BASIC_RADIUS, "Dabs per basic radius", 1, 0.0, 0.0, 6.0, "How many dabs to draw while the pointer moves a distance of one brush radius (more precise: the base value of the radius)"},
  {"dabs_per_actual_radius", BRUSH_DABS_PER_ACTUAL_RADIUS, "Dabs per actual radius", 1, 0.0, 2.0, 6.0, "Same as above, but the radius actually drawn is used, which can change dynamically"},
  {"dabs_per_second", BRUSH_DABS_PER_SECOND, "Dabs per second", 1, 0.0, 0.0, 80.0, "Dabs to draw each second, no matter how far the pointer moves"},
  {"radius_by_random", BRUSH_RADIUS_BY_RANDOM, "Radius by random", 0, 0.0, 0.0, 1.5, "Alter the radius randomly each dab. You can also do this with the by_random input on the radius setting. If you do it here, there are two differences:\n1) the opaque value will be corrected such that a big-radius dabs is more transparent\n2) it will not change the actual radius seen by dabs_per_actual_radius"},
  {"speed1_slowness", BRUSH_SPEED1_SLOWNESS, "Fine speed filter", 0, 0.0, 0.04, 0.2, "How slow the input fine speed is following the real speed\n0.0 change immediately as your speed changes (not recommended, but try it)"},
  {"speed2_slowness", BRUSH_SPEED2_SLOWNESS, "Gross speed filter", 0, 0.0, 0.8, 3.0, "Same as \"fine speed filter\", but note that the range is different"},
  {"speed1_gamma", BRUSH_SPEED1_GAMMA, "Fine speed gamma", 1, -8.0, 4.0, 8.0, "This changes the reaction of the \"fine speed\" input to extreme physical speed. You will see the difference best if \"fine speed\" is mapped to the radius.\n-8.0 very fast speed does not increase \"fine speed\" much more\n+8.0 very fast speed increases \"fine speed\" a lot\nFor very slow speed the opposite happens."},
  {"speed2_gamma", BRUSH_SPEED2_GAMMA, "Gross speed gamma", 1, -8.0, 4.0, 8.0, "Same as \"fine speed gamma\" for gross speed"},
  {"offset_by_random", BRUSH_OFFSET_BY_RANDOM, "Jitter", 0, 0.0, 0.0, 25.0, "Add a random offset to the position where each dab is drawn\n 0.0 disabled\n 1.0 standard deviation is one basic radius away\n<0.0 negative values produce no jitter"},
  {"offset_by_speed", BRUSH_OFFSET_BY_SPEED, "Offset by speed", 0, -3.0, 0.0, 3.0, "Change position depending on pointer speed\n= 0 disable\n> 0 draw where the pointer moves to\n< 0 draw where the pointer comes from"},
  {"offset_by_speed_slowness", BRUSH_OFFSET_BY_SPEED_SLOWNESS, "Offset by speed filter", 0, 0.0, 1.0, 15.0, "How slow the offset goes back to zero when the cursor stops moving"},
  {"slow_tracking", BRUSH_SLOW_TRACKING, "Slow position tracking", 1, 0.0, 0.0, 10.0, "Slowdown pointer tracking speed. 0 disables it, higher values remove more jitter in cursor movements. Useful for drawing smooth, comic-like outlines."},
  {"slow_tracking_per_dab", BRUSH_SLOW_TRACKING_PER_DAB, "Slow tracking per dab", 0, 0.0, 0.0, 10.0, "Similar as above but at brushdab level (ignoring how much time has past, if brushdabs do not depend on time)"},
  {"tracking_noise", BRUSH_TRACKING_NOISE, "Tracking noise", 1, 0.0, 0.0, 12.0, "Add randomness to the mouse pointer; this usually generates many small lines in random directions; maybe try this together with \"slow tracking\""},
  {"color_h", BRUSH_COLOR_H, "Color hue", 1, 0.0, 0.0, 1.0, "Color hue"},
  {"color_s", BRUSH_COLOR_S, "Color saturation", 1, -0.5, 0.0, 1.5, "Color saturation"},
  {"color_v", BRUSH_COLOR_V, "Color value", 1, -0.5, 0.0, 1.5, "Color value (brightness, intensity)"},
  {"restore_color", BRUSH_RESTORE_COLOR, "Save color", 1, 0.0, 0.0, 1.0, "When selecting a brush, the color can be restored to the color that the brush was saved with.\n 0.0 do not modify the active color when selecting this brush\n 0.5 change active color towards brush color\n 1.0 set the active color to the brush color when selected"},
  {"change_color_h", BRUSH_CHANGE_COLOR_H, "Change color hue", 0, -2.0, 0.0, 2.0, "Change color hue.\n-0.1 small clockwise color hue shift\n 0.0 disable\n 0.5 counterclockwise hue shift by 180 degrees"},
  {"change_color_l", BRUSH_CHANGE_COLOR_L, "Change color lightness (HSL)", 0, -2.0, 0.0, 2.0, "Change the color lightness (luminance) using the HSL color model.\n-1.0 blacker\n 0.0 disable\n 1.0 whiter"},
  {"change_color_hsl_s", BRUSH_CHANGE_COLOR_HSL_S, "Change color satur. (HSL)", 0, -2.0, 0.0, 2.0, "Change the color saturation using the HSL color model.\n-1.0 more grayish\n 0.0 disable\n 1.0 more saturated"},
  {"change_color_v", BRUSH_CHANGE_COLOR_V, "Change color value (HSV)", 0, -2.0, 0.0, 2.0, "Change the color value (brightness, intensity) using the HSV color model. HSV changes are applied before HSL.\n-1.0 darker\n 0.0 disable\n 1.0 brigher"},
  {"change_color_hsv_s", BRUSH_CHANGE_COLOR_HSV_S, "Change color satur. (HSV)", 0, -2.0, 0.0, 2.0, "Change the color saturation using the HSV color model. HSV changes are applied before HSL.\n-1.0 more grayish\n 0.0 disable\n 1.0 more saturated"},
  {"smudge", BRUSH_SMUDGE, "Smudge", 0, 0.0, 0.0, 1.0, "Paint with the smudge color instead of the brush color. The smudge color is slowly changed to the color you are painting on.\n 0.0 do not use the smudge color\n 0.5 mix the smudge color with the brush color\n 1.0 use only the smudge color"},
  {"smudge_length", BRUSH_SMUDGE_LENGTH, "Smudge length", 0, 0.0, 0.5, 1.0, "This controls how fast the smudge color becomes the color you are painting on.\n0.0 immediately update the smudge color (requires more CPU cycles because of the frequent color checks)\n0.5 change the smudge color steadily towards the canvas color\n1.0 never change the smudge color"},
  {"smudge_radius_log", BRUSH_SMUDGE_RADIUS_LOG, "Smudge radius", 0, -1.6, 0.0, 1.6, "This modifies the radius of the circle where color is picked up for smudging.\n 0.0 use the brush radius\n-0.7 half the brush radius (fast, but not always intuitive)\n+0.7 twice the brush radius\n+1.6 five times the brush radius (slow performance)"},
  {"eraser", BRUSH_ERASER, "Eraser", 0, 0.0, 0.0, 1.0, "how much this tool behaves like an eraser\n 0.0 normal painting\n 1.0 standard eraser\n 0.5 pixels go towards 50% transparency"},
  {"stroke_threshold", BRUSH_STROKE_THRESHOLD, "Stroke threshold", 1, 0.0, 0.0, 0.5, "How much pressure is needed to start a stroke. This affects the stroke input only. Mypaint does not need a minimal pressure to start drawing."},
  {"stroke_duration_logarithmic", BRUSH_STROKE_DURATION_LOGARITHMIC, "Stroke duration", 0, -1.0, 4.0, 7.0, "How far you have to move until the stroke input reaches 1.0. This value is logarithmic (negative values will not inverse the process)."},
  {"stroke_holdtime", BRUSH_STROKE_HOLDTIME, "Stroke hold time", 0, 0.0, 0.0, 10.0, "This defines how long the stroke input stays at 1.0. After that it will reset to 0.0 and start growing again, even if the stroke is not yet finished.\n2.0 means twice as long as it takes to go from 0.0 to 1.0\n9.9 and bigger stands for infinite"},
  {"custom_input", BRUSH_CUSTOM_INPUT, "Custom input", 0, -5.0, 0.0, 5.0, "Set the custom input to this value. If it is slowed down, move it towards this value (see below). The idea is that you make this input depend on a mixture of pressure/speed/whatever, and then make other settings depend on this \"custom input\" instead of repeating this combination everywhere you need it.\nIf you make it change \"by random\" you can generate a slow (smooth) random input."},
  {"custom_input_slowness", BRUSH_CUSTOM_INPUT_SLOWNESS, "Custom input filter", 0, 0.0, 0.0, 10.0, "How slow the custom input actually follows the desired value (the one above). This happens at brushdab level (ignoring how much time has past, if brushdabs do not depend on time).\n0.0 no slowdown (changes apply instantly)"},
  {"elliptical_dab_ratio", BRUSH_ELLIPTICAL_DAB_RATIO, "Elliptical dab: ratio", 0, 1.0, 1.0, 10.0, "Aspect ratio of the dabs; must be >= 1.0, where 1.0 means a perfectly round dab. TODO: linearize? start at 0.0 maybe, or log?"},
  {"elliptical_dab_angle", BRUSH_ELLIPTICAL_DAB_ANGLE, "Elliptical dab: angle", 0, 0.0, 90.0, 360.0, "Angle by which elliptical dabs are tilted\n 0.0 horizontal dabs\n 45.0 45 degrees, turned clockwise\n 180.0 horizontal again"},
  {"direction_filter", BRUSH_DIRECTION_FILTER, "Direction filter", 0, 0.0, 2.0, 10.0, "A low value will make the direction input adapt more quickly, a high value will make it smoother"},
  {"lock_alpha", BRUSH_LOCK_ALPHA, "Lock alpha", 0, 0.0, 0.0, 1.0, "Do not modify the alpha channel of the layer (paint only where there is paint already)\n 0.0 normal painting\n 0.5 half of the paint gets applied normally\n 1.0 alpha channel fully locked"},
  {"stroke_opacity", BRUSH_STROKE_OPACITY, "Stroke opacity", 1, 0.0, 1.0, 1.0, "Opacity applied for whole stroke. meaningful only when non-incremental mode is enabled.\n"},
  {"texture_grain", BRUSH_TEXTURE_GRAIN, "Texture grain level", 0, -1.0, 0.0, 1.0, "Grain level applied for texture mapping."},
  {"texture_contrast", BRUSH_TEXTURE_CONTRAST, "Texture contrast level", 0, 0.0, 1.0, 1.0, "Contrast level applied for texture mapping."},
};

static const GimpPainterMyPaintSwitch painter_mypaint_switches[BRUSH_BOOL_COUNT] = {
  {"non_incremental", BRUSH_NON_INCREMENTAL, "Non-incremental painting mode.", 0},
  {"use_gimp_brushmark", BRUSH_USE_GIMP_BRUSHMARK, "Use GIMP brush for brushmark.", 1},
  {"use_gimp_texture", BRUSH_USE_GIMP_TEXTURE, "Use GIMP pattern for texture.", 0},
  {"brushmark_specified", BRUSH_BRUSHMARK_SPECIFIED, "Remember brushmark name as a preset.", 0},
  {"texture_specified", BRUSH_TEXTURE_SPECIFIED, "Remember texture name as a preset.", 0},
};

static const GimpPainterMyPaintText painter_mypaint_texts[BRUSH_TEXT_COUNT] = {
  {"brushmark_name", BRUSH_BRUSHMARK_NAME, "Brushmark name", NULL},
  {"texture_name", BRUSH_TEXTURE_NAME, "Texture name", NULL},
};

static const GimpPainterMyPaintState painter_mypaint_states[STATE_COUNT] = {
  {"x", STATE_X, 0},
  {"y", STATE_Y, 0},
  {"pressure", STATE_PRESSURE, 0},
  {"dist", STATE_DIST, 0},
  {"actual_radius", STATE_ACTUAL_RADIUS, 0},
  {"smudge_ra", STATE_SMUDGE_RA, 0},
  {"smudge_ga", STATE_SMUDGE_GA, 0},
  {"smudge_ba", STATE_SMUDGE_BA, 0},
  {"smudge_a", STATE_SMUDGE_A, 0},
  {"last_getcolor_r", STATE_LAST_GETCOLOR_R, 0},
  {"last_getcolor_g", STATE_LAST_GETCOLOR_G, 0},
  {"last_getcolor_b", STATE_LAST_GETCOLOR_B, 0},
  {"last_getcolor_a", STATE_LAST_GETCOLOR_A, 0},
  {"last_getcolor_recentness", STATE_LAST_GETCOLOR_RECENTNESS, 0},
  {"actual_x", STATE_ACTUAL_X, 0},
  {"actual_y", STATE_ACTUAL_Y, 0},
  {"norm_dx_slow", STATE_NORM_DX_SLOW, 0},
  {"norm_dy_slow", STATE_NORM_DY_SLOW, 0},
  {"norm_speed1_slow", STATE_NORM_SPEED1_SLOW, 0},
  {"norm_speed2_slow", STATE_NORM_SPEED2_SLOW, 0},
  {"stroke", STATE_STROKE, 0},
  {"stroke_started", STATE_STROKE_STARTED, 0},
  {"custom_input", STATE_CUSTOM_INPUT, 0},
  {"rng_seed", STATE_RNG_SEED, 0},
  {"actual_elliptical_dab_ratio", STATE_ACTUAL_ELLIPTICAL_DAB_RATIO, 0},
  {"actual_elliptical_dab_angle", STATE_ACTUAL_ELLIPTICAL_DAB_ANGLE, 0},
  {"direction_dx", STATE_DIRECTION_DX, 0},
  {"direction_dy", STATE_DIRECTION_DY, 0},
  {"declination", STATE_DECLINATION, 0},
  {"ascension", STATE_ASCENSION, 0},
};

#endif
