/*
# brushlib - The MyPaint Brush Library
# Copyright (C) 2007-2011 Martin Renold <martinxyz@gmx.ch>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <glib-object.h>
#include <cairo.h>
#include <float.h>
#include "mypaintbrush-brushsettings.h"
#include "mypaintbrush-enum-settings.h"

#define None  FLT_MAX
#define False FALSE
#define True  TRUE
#define _(x)  x

static MyPaintBrushInputSettings inputs_list[] = {
  /* # name, hard minimum, soft minimum, normal[1], soft maximum, hard maximum, displayed name, tooltip */
  {"pressure", INPUT_PRESSURE, 0.0,  0.0,  0.4,  1.0, None,  _("Pressure"), _("The pressure reported by the tablet, between 0.0 and 1.0. If you use the mouse, it will be 0.5 when a button is pressed and 0.0 otherwise.")},
  {"speed1",  INPUT_SPEED1, None, 0.0,  0.5,  4.0, None, _("Fine speed"), _("How fast you currently move. This can change very quickly. Try \"print input values\" from the \"help\" menu to get a feeling for the range; negative values are rare but possible for very low speed.")},
  {"speed2",  INPUT_SPEED2, None, 0.0,  0.5,  4.0, None, _("Gross speed"), _("Same as fine speed, but changes slower. Also look at the \"gross speed filter\" setting.")},
  {"random",  INPUT_RANDOM, 0.0,  0.0,  0.5,  1.0, 1.0, _("Random"), _("Fast random noise, changing at each evaluation. Evenly distributed between 0 and 1.")},
  {"stroke",  INPUT_STROKE,  0.0,  0.0,  0.5,  1.0, 1.0, _("Stroke"), _("This input slowly goes from zero to one while you draw a stroke. It can also be configured to jump back to zero periodically while you move. Look at the \"stroke duration\" and \"stroke hold time\" settings.")},
  {"direction", INPUT_DIRECTION, 0.0,  0.0,  0.0,  360.0, 360.0, _("Direction"), _("The angle of the stroke, in degrees. The value will stay between 0.0 and 360.0, effectively ignoring turns of 180 degrees.")},
  {"tilt_declination", INPUT_TILT_DECLINATION, 0.0,  0.0,  0.0,  90.0, 90.0,  _("Declination"), _("Declination of stylus tilt. 0 when stylus is parallel to tablet and 90.0 when it's perpendicular to tablet.")},
  {"tilt_ascension", INPUT_TILT_ASCENSION, -180.0,  -180.0,  0.0,  180.0, 180.0, _("Ascension"),  _("Right ascension of stylus tilt. 0 when stylus working end points to you, +90 when rotated 90 degrees clockwise, -90 when rotated 90 degrees counterclockwise.")},
  //{"motion_strength", INPUT_MOTION_STRENGTH, 0.0,0.0,  0.0,  1.0, 1.0,  "[EXPERIMENTAL} Same as angle, but wraps at 180 degrees. The dynamics are shared with BRUSH_OFFSET_BY_SPEED_FILTER (FIXME: which is a bad thing)."},
  {"custom",    INPUT_CUSTOM, None,-2.0,  0.0, +2.0, None, _("Custom"), _("This is a user defined input. Look at the \"custom input\" setting for details.")},
  {NULL}
};
    /*
    # [1] If, for example, the user increases the "by pressure" slider
    # in the "radius" control, then this should change the reaction to
    # pressure and not the "normal" radius. To implement this, we need
    # a guess what the user considers to be normal pressure.
    */


static MyPaintBrushSettings settings_list[] = {
/*    # internal name, displayed name, constant, minimum, default, maximum, tooltip */
  {"opaque", BRUSH_OPAQUE,  _("Opacity"), False, 0.0, 1.0, 2.0, _("0 means brush is transparent, 1 fully visible\n(also known as alpha or opacity)")},
  {"opaque_multiply", BRUSH_OPAQUE_MULTIPLY, _("Opacity multiply"), False, 0.0, 0.0, 2.0, _("This gets multiplied with opaque. You should only change the pressure input of this setting. Use \"opaque\" instead to make opacity depend on speed.\nThis setting is responsible to stop painting when there is zero pressure. This is just a convention, the behaviour is identical to \"opaque\".")},
  {"opaque_linearize", BRUSH_OPAQUE_LINEARIZE, _("Opacity linearize"), True, 0.0, 0.9, 2.0, _("Correct the nonlinearity introduced by blending multiple dabs on top of each other. This correction should get you a linear (\"natural\") pressure response when pressure is mapped to opaque_multiply, as it is usually done. 0.9 is good for standard strokes, set it smaller if your brush scatters a lot, or higher if you use dabs_per_second.\n0.0 the opaque value above is for the individual dabs\n1.0 the opaque value above is for the final brush stroke, assuming each pixel gets (dabs_per_radius*2) brushdabs on average during a stroke")},
  {"radius_logarithmic", BRUSH_RADIUS_LOGARITHMIC, _("Radius"), False, -2.0, 2.0, 5.0, _("Basic brush radius (logarithmic)\n 0.7 means 2 pixels\n 3.0 means 20 pixels")},
  {"hardness", BRUSH_HARDNESS, _("Hardness"), False, 0.0, 0.8, 1.0, _("Hard brush-circle borders (setting to zero will draw nothing). To reach the maximum hardness, you need to disable Anti-aliasing.")},
  {"anti_aliasing", BRUSH_ANTI_ALIASING, _("Anti-aliasing"), False, 0.0, 1.0, 5.0, _("This setting decreases the hardness when necessary to prevent a pixel staircase effect.\n 0.0 disable (for very strong erasers and pixel brushes)\n 1.0 blur one pixel (good value)\n 5.0 notable blur, thin strokes will disappear")},
  {"dabs_per_basic_radius", BRUSH_DABS_PER_BASIC_RADIUS, _("Dabs per basic radius"), True, 0.0, 0.0, 6.0, _("How many dabs to draw while the pointer moves a distance of one brush radius (more precise: the base value of the radius)")},
  {"dabs_per_actual_radius", BRUSH_DABS_PER_ACTUAL_RADIUS, _("Dabs per actual radius"), True, 0.0, 2.0, 6.0, _("Same as above, but the radius actually drawn is used, which can change dynamically")},
  {"dabs_per_second", BRUSH_DABS_PER_SECOND, _("Dabs per second"), True, 0.0, 0.0, 80.0, _("Dabs to draw each second, no matter how far the pointer moves")},
  {"radius_by_random", BRUSH_RADIUS_BY_RANDOM, _("Radius by random"), False, 0.0, 0.0, 1.5, _("Alter the radius randomly each dab. You can also do this with the by_random input on the radius setting. If you do it here, there are two differences:\n1) the opaque value will be corrected such that a big-radius dabs is more transparent\n2) it will not change the actual radius seen by dabs_per_actual_radius")},
  {"speed1_slowness", BRUSH_SPEED1_SLOWNESS, _("Fine speed filter"), False, 0.0, 0.04, 0.2, _("How slow the input fine speed is following the real speed\n0.0 change immediately as your speed changes (not recommended, but try it)")},
  {"speed2_slowness", BRUSH_SPEED2_SLOWNESS, _("Gross speed filter"), False, 0.0, 0.8, 3.0, _("Same as \"fine speed filter\", but note that the range is different")},
  {"speed1_gamma", BRUSH_SPEED1_GAMMA, _("Fine speed gamma"), True, -8.0, 4.0, 8.0, _("This changes the reaction of the \"fine speed\" input to extreme physical speed. You will see the difference best if \"fine speed\" is mapped to the radius.\n-8.0 very fast speed does not increase \"fine speed\" much more\n+8.0 very fast speed increases \"fine speed\" a lot\nFor very slow speed the opposite happens.")},
  {"speed2_gamma", BRUSH_SPEED2_GAMMA, _("Gross speed gamma"), True, -8.0, 4.0, 8.0, _("Same as \"fine speed gamma\" for gross speed")},
  {"offset_by_random", BRUSH_OFFSET_BY_RANDOM, _("Jitter"), False, 0.0, 0.0, 25.0, _("Add a random offset to the position where each dab is drawn\n 0.0 disabled\n 1.0 standard deviation is one basic radius away\n<0.0 negative values produce no jitter")},
  {"offset_by_speed", BRUSH_OFFSET_BY_SPEED,  _("Offset by speed"), False, -3.0, 0.0, 3.0, _("Change position depending on pointer speed\n= 0 disable\n> 0 draw where the pointer moves to\n< 0 draw where the pointer comes from")},
  {"offset_by_speed_slowness", BRUSH_OFFSET_BY_SPEED_SLOWNESS, _("Offset by speed filter"), False, 0.0, 1.0, 15.0, _("How slow the offset goes back to zero when the cursor stops moving")},
  {"slow_tracking", BRUSH_SLOW_TRACKING, _("Slow position tracking"), True, 0.0, 0.0, 10.0, _("Slowdown pointer tracking speed. 0 disables it, higher values remove more jitter in cursor movements. Useful for drawing smooth, comic-like outlines.")},
  {"slow_tracking_per_dab", BRUSH_SLOW_TRACKING_PER_DAB, _("Slow tracking per dab"), False, 0.0, 0.0, 10.0, _("Similar as above but at brushdab level (ignoring how much time has past, if brushdabs do not depend on time)")},
  {"tracking_noise", BRUSH_TRACKING_NOISE, _("Tracking noise"), True, 0.0, 0.0, 12.0, _("Add randomness to the mouse pointer; this usually generates many small lines in random directions; maybe try this together with \"slow tracking\"")},

  {"color_h", BRUSH_COLOR_H, _("Color hue"), True, 0.0, 0.0, 1.0, _("Color hue")},
  {"color_s", BRUSH_COLOR_S, _("Color saturation"), True, -0.5, 0.0, 1.5, _("Color saturation")},
  {"color_v", BRUSH_COLOR_V, _("Color value"), True, -0.5, 0.0, 1.5, _("Color value (brightness, intensity)")},
  {"restore_color", BRUSH_RESTORE_COLOR, _("Save color"), True, 0.0, 0.0, 1.0, _("When selecting a brush, the color can be restored to the color that the brush was saved with.\n 0.0 do not modify the active color when selecting this brush\n 0.5 change active color towards brush color\n 1.0 set the active color to the brush color when selected")},
  {"change_color_h", BRUSH_CHANGE_COLOR_H, _("Change color hue"), False, -2.0, 0.0, 2.0, _("Change color hue.\n-0.1 small clockwise color hue shift\n 0.0 disable\n 0.5 counterclockwise hue shift by 180 degrees")},
  {"change_color_l", BRUSH_CHANGE_COLOR_L, _("Change color lightness (HSL)"), False, -2.0, 0.0, 2.0, _("Change the color lightness (luminance) using the HSL color model.\n-1.0 blacker\n 0.0 disable\n 1.0 whiter")},
  {"change_color_hsl_s", BRUSH_CHANGE_COLOR_HSL_S, _("Change color satur. (HSL)"), False, -2.0, 0.0, 2.0, _("Change the color saturation using the HSL color model.\n-1.0 more grayish\n 0.0 disable\n 1.0 more saturated")},
  {"change_color_v", BRUSH_CHANGE_COLOR_V, _("Change color value (HSV)"), False, -2.0, 0.0, 2.0, _("Change the color value (brightness, intensity) using the HSV color model. HSV changes are applied before HSL.\n-1.0 darker\n 0.0 disable\n 1.0 brigher")},
  {"change_color_hsv_s", BRUSH_CHANGE_COLOR_HSV_S, _("Change color satur. (HSV)"), False, -2.0, 0.0, 2.0, _("Change the color saturation using the HSV color model. HSV changes are applied before HSL.\n-1.0 more grayish\n 0.0 disable\n 1.0 more saturated")},
  {"smudge", BRUSH_SMUDGE, _("Smudge"), False, 0.0, 0.0, 1.0, _("Paint with the smudge color instead of the brush color. The smudge color is slowly changed to the color you are painting on.\n 0.0 do not use the smudge color\n 0.5 mix the smudge color with the brush color\n 1.0 use only the smudge color")},
  {"smudge_length", BRUSH_SMUDGE_LENGTH, _("Smudge length"), False, 0.0, 0.5, 1.0, _("This controls how fast the smudge color becomes the color you are painting on.\n0.0 immediately update the smudge color (requires more CPU cycles because of the frequent color checks)\n0.5 change the smudge color steadily towards the canvas color\n1.0 never change the smudge color")},
  {"smudge_radius_log", BRUSH_SMUDGE_RADIUS_LOG, _("Smudge radius"), False, -1.6, 0.0, 1.6, _("This modifies the radius of the circle where color is picked up for smudging.\n 0.0 use the brush radius\n-0.7 half the brush radius (fast, but not always intuitive)\n+0.7 twice the brush radius\n+1.6 five times the brush radius (slow performance)")},
  {"eraser", BRUSH_ERASER, _("Eraser"), False, 0.0, 0.0, 1.0, _("how much this tool behaves like an eraser\n 0.0 normal painting\n 1.0 standard eraser\n 0.5 pixels go towards 50% transparency")},

  {"stroke_threshold", BRUSH_STROKE_THRESHOLD, _("Stroke threshold"), True, 0.0, 0.0, 0.5, _("How much pressure is needed to start a stroke. This affects the stroke input only. Mypaint does not need a minimal pressure to start drawing.")},
  {"stroke_duration_logarithmic", BRUSH_STROKE_DURATION_LOGARITHMIC, _("Stroke duration"), False, -1.0, 4.0, 7.0, _("How far you have to move until the stroke input reaches 1.0. This value is logarithmic (negative values will not inverse the process).")},
  {"stroke_holdtime", BRUSH_STROKE_HOLDTIME, _("Stroke hold time"), False, 0.0, 0.0, 10.0, _("This defines how long the stroke input stays at 1.0. After that it will reset to 0.0 and start growing again, even if the stroke is not yet finished.\n2.0 means twice as long as it takes to go from 0.0 to 1.0\n9.9 and bigger stands for infinite")},
  {"custom_input", BRUSH_CUSTOM_INPUT, _("Custom input"), False, -5.0, 0.0, 5.0, _("Set the custom input to this value. If it is slowed down, move it towards this value (see below). The idea is that you make this input depend on a mixture of pressure/speed/whatever, and then make other settings depend on this \"custom input\" instead of repeating this combination everywhere you need it.\nIf you make it change \"by random\" you can generate a slow (smooth) random input.")},
  {"custom_input_slowness", BRUSH_CUSTOM_INPUT_SLOWNESS, _("Custom input filter"), False, 0.0, 0.0, 10.0, _("How slow the custom input actually follows the desired value (the one above). This happens at brushdab level (ignoring how much time has past, if brushdabs do not depend on time).\n0.0 no slowdown (changes apply instantly)")},

  {"elliptical_dab_ratio", BRUSH_ELLIPTICAL_DAB_RATIO, _("Elliptical dab: ratio"), False, 1.0, 1.0, 10.0, _("Aspect ratio of the dabs; must be >= 1.0, where 1.0 means a perfectly round dab. TODO: linearize? start at 0.0 maybe, or log?")},
  {"elliptical_dab_angle", BRUSH_ELLIPTICAL_DAB_ANGLE, _("Elliptical dab: angle"), False, 0.0, 90.0, 360.0, _("Angle by which elliptical dabs are tilted\n 0.0 horizontal dabs\n 45.0 45 degrees, turned clockwise\n 180.0 horizontal again")},
  {"direction_filter", BRUSH_DIRECTION_FILTER, _("Direction filter"), False, 0.0, 2.0, 10.0, _("A low value will make the direction input adapt more quickly, a high value will make it smoother")},

  {"lock_alpha", BRUSH_LOCK_ALPHA, _("Lock alpha"), False, 0.0, 0.0, 1.0, _("Do not modify the alpha channel of the layer (paint only where there is paint already)\n 0.0 normal painting\n 0.5 half of the paint gets applied normally\n 1.0 alpha channel fully locked")},
  {"stroke_opacity", BRUSH_STROKE_OPACITY, _("Stroke opacity"), True, 0.0, 1.0, 1.0, _("Opacity applied for whole stroke. meaningful only when non-incremental mode is enabled.\n") },
  {"texture_grain", BRUSH_TEXTURE_GRAIN, _("Texture grain level"), False, -1.0, 0.0, 1.0, _("Grain level applied for texture mapping.")},
  {"texture_contrast", BRUSH_TEXTURE_CONTRAST, _("Texture contrast level"), False, 0.0, 1.0, 1.0, _("Contrast level applied for texture mapping.")},
  {NULL}
};

static MyPaintBrushSwitchSettings switches_list[] = {
  {"non_incremental", BRUSH_NON_INCREMENTAL, _("Non-incremental painting mode."), FALSE },
  {"use_gimp_brushmark", BRUSH_USE_GIMP_BRUSHMARK, _("Use GIMP brush for brushmark."), TRUE },
  {"use_gimp_texture", BRUSH_USE_GIMP_TEXTURE, _("Use GIMP pattern for texture."), FALSE },
  {"brushmark_specified", BRUSH_BRUSHMARK_SPECIFIED, _("Remember brushmark name as a preset."), FALSE },
  {"texture_specified", BRUSH_TEXTURE_SPECIFIED, _("Remember texture name as a preset."), FALSE },
  {NULL}
};

static MyPaintBrushTextSettings text_list[] = {
  {"brushmark_name", BRUSH_BRUSHMARK_NAME, _("Brushmark name"), NULL},
  {"texture_name", BRUSH_TEXTURE_NAME, _("Texture name"), NULL},
  {NULL}
};

struct _MypaintBrushSettingGroupInternal {
  gchar                          *name;
  MypaintBrushSettingGroupID  index;
  gchar                          *display_name;
  gchar                         **setting_list;
};
typedef struct _MypaintBrushSettingGroupInternal MypaintBrushSettingGroupInternal;

static gchar* basic_group_list[] = {
    "radius_logarithmic", 
    "radius_by_random", 
    "hardness", 
    "anti_aliasing", 
    "eraser", 
    "offset_by_random", 
    "elliptical_dab_angle", 
    "elliptical_dab_ratio", 
    "direction_filter",
    NULL};
static gchar* opacity_group_list[] = { 
    "opaque", 
    "opaque_multiply", 
    "opaque_linearize", 
    "lock_alpha",
    NULL};
static gchar* dabs_group_list[] = {
    "dabs_per_basic_radius",
    "dabs_per_actual_radius",
    "dabs_per_second",
    "use_gimp_brushmark",
    "brushmark_specified",
    "brushmark_name",
    NULL};
static gchar* smudge_group_list[] = {
    "smudge", 
    "smudge_length", 
    "smudge_radius_log",
    NULL};
static gchar* speed_group_list[] = { 
    "speed1_slowness", 
    "speed2_slowness", 
    "speed1_gamma", 
    "speed2_gamma", 
    "offset_by_speed", 
    "offset_by_speed_slowness",
    NULL};
static gchar* tracking_group_list[] = {
    "slow_tracking", 
    "slow_tracking_per_dab",
    "tracking_noise" };
static gchar* stroke_group_list[] = {
    "stroke_threshold", 
    "stroke_duration_logarithmic", 
    "stroke_holdtime", 
    "non_incremental",
    "stroke_opacity",
    NULL};
static gchar* texture_group_list[] = {
    "use_gimp_texture",
    "texture_specified",
    "texture_name",
    "texture_grain",
    "texture_contrast",
    NULL};
static gchar* color_group_list[] = {
    "change_color_h", 
    "change_color_l",
    "change_color_hsl_s",
    "change_color_v",
    "change_color_hsv_s",
    "restore_color",
    "colorize",
    NULL};
static gchar* custom_group_list[] = {
    "custom_input",
    "custom_input_slowness",
    NULL};


static MypaintBrushSettingGroupInternal group_list[] = {
  {"basic", BRUSH_SETTING_GROUP_BASIC, _("Basic"), basic_group_list},
  {"opacity", BRUSH_SETTING_GROUP_OPACITY, _("Opacity"), opacity_group_list},
  {"dabs",  BRUSH_SETTING_GROUP_DABS, _("Dabs"), dabs_group_list},
  {"smudge", BRUSH_SETTING_GROUP_SMUDGE, _("Smudge"), smudge_group_list},
  {"speed", BRUSH_SETTING_GROUP_SPEED, _("Speed"), speed_group_list},
  {"tracking", BRUSH_SETTING_GROUP_TRACKING, _("Tracking"), tracking_group_list},
  {"stroke",  BRUSH_SETTING_GROUP_TRACKING, _("Stroke"), stroke_group_list},
  {"color", BRUSH_SETTING_GROUP_COLOR, _("Color"), color_group_list},
  {"texture", BRUSH_SETTING_GROUP_TEXTURE, _("Texture"), texture_group_list},
  {"custom", BRUSH_SETTING_GROUP_CUSTOM, _("Custom"), custom_group_list},
  {NULL}
};

static gfloat transform_change_color_h (gfloat y) { return y*64.0/360.0; }
static gfloat transform_change_color_hsv_s (gfloat y) { return y*128.0/256.0; }
static gfloat transform_change_color_v (gfloat y) { return y*128.0/256.0; }

MyPaintBrushSettingMigrate migration[] = {
  {"color_hue", "change_color_h", transform_change_color_h},
  {"color_saturation", "change_color_hsv_s", transform_change_color_hsv_s},
  {"color_value", "change_color_v", transform_change_color_v},
  {"speed_slowness", "speed1_slowness", NULL},
  {"change_color_s", "change_color_hsv_s", NULL},
  {"stroke_treshold", "stroke_threshold", NULL},
  {NULL}
};

#if 0
settings_hidden = "color_h color_s color_v".split()

// the states are not (yet?) exposed to the user
// WARNING: only append to this list, for compatibility of replay files (brush.get_state() in stroke.py)
gchar* states_list[] = {
// lowlevel
"x", "y",
"pressure",
"dist",              // "distance" moved since last dab, a new dab is drawn at 1.0
"actual_radius",     // used by count_dabs_to, thus a state!

"smudge_ra", "smudge_ga", "smudge_ba", "smudge_a",  // smudge color stored with premultiplied alpha (low-pass filtered)
"last_getcolor_r", "last_getcolor_g", "last_getcolor_b", "last_getcolor_a", // cached result of last call to get_color()
"last_getcolor_recentness",

"actual_x", "actual_y",  // for slow position
"norm_dx_slow", "norm_dy_slow", // note: now this is dx/dt * (1/radius)

"norm_speed1_slow", "norm_speed2_slow",

"stroke", "stroke_started", // stroke_started is used as boolean

"custom_input",
"rng_seed",

"actual_elliptical_dab_ratio", "actual_elliptical_dab_angle", // used by count_dabs_to

"direction_dx", "direction_dy",
"declination", "ascension"
};

settings_visible = [s for s in settings if s.cname not in settings_hidden}

states = []
for line in states_list.split("\n"):
    line = line.split("#")[0}
    for cname in line.split(","):
        cname = cname.strip()
        if not cname: continue
        st = BrushState()
        st.cname = cname
        st.index = len(states)
        states.append(st)
#endif


static GHashTable* brush_input_settings_dict    = NULL;
static GHashTable* brush_settings_dict          = NULL;
static GHashTable* brush_setting_migrate_dict   = NULL;
static GHashTable* brush_setting_group_dict     = NULL;
static GHashTable* brush_switch_settings_dict   = NULL;
static GHashTable* brush_text_settings_dict     = NULL;

static GHashTable* index_to_name_dict           = NULL;

GList *
mypaint_brush_get_input_settings (void)
{
  GList *result = NULL;
  MyPaintBrushInputSettings *input = inputs_list;
  for (; input->name; input ++)
    {
      result = g_list_append (result, input);
    }
  return result;
}

GHashTable *mypaint_brush_get_input_settings_dict (void)
{
  if (!brush_input_settings_dict) {
    MyPaintBrushInputSettings *setting = inputs_list;
    brush_input_settings_dict = g_hash_table_new (g_str_hash, g_str_equal);
    for (; setting->name; setting ++)
    {
      g_hash_table_insert (brush_input_settings_dict, setting->name, setting);
    }
  }
  g_hash_table_ref(brush_input_settings_dict);
  return brush_input_settings_dict;
}

GList *
mypaint_brush_get_brush_settings (void)
{
  GList *result = NULL;
  MyPaintBrushSettings *setting = settings_list;
  for (; setting->internal_name; setting ++)
    {
      result = g_list_append (result, setting);
    }
  return result;
}

GHashTable *mypaint_brush_get_brush_settings_dict (void)
{
  if (!brush_settings_dict) {
    MyPaintBrushSettings *setting = settings_list;
    brush_settings_dict = g_hash_table_new (g_str_hash, g_str_equal);
    for (; setting->internal_name; setting ++)
    {
      g_hash_table_insert (brush_settings_dict, setting->internal_name, setting);
    }
    g_hash_table_ref (brush_settings_dict);
  }
  g_hash_table_ref (brush_settings_dict);
  
  return brush_settings_dict;
}

GList *
mypaint_brush_get_brush_switch_settings (void)
{
  GList *result = NULL;
  MyPaintBrushSwitchSettings *setting = switches_list;
  for (; setting->internal_name; setting ++)
    {
      result = g_list_append (result, setting);
    }
  return result;
}

GHashTable *mypaint_brush_get_brush_switch_settings_dict (void)
{
  if (!brush_switch_settings_dict) {
    MyPaintBrushSwitchSettings *setting = switches_list;
    brush_switch_settings_dict = g_hash_table_new (g_str_hash, g_str_equal);
    for (; setting->internal_name; setting ++)
    {
      g_hash_table_insert (brush_switch_settings_dict, setting->internal_name, setting);
    }
    g_hash_table_ref (brush_switch_settings_dict);
  }
  g_hash_table_ref (brush_switch_settings_dict);
  
  return brush_switch_settings_dict;
}


GList *
mypaint_brush_get_brush_text_settings (void)
{
  GList *result = NULL;
  MyPaintBrushTextSettings *setting = text_list;
  for (; setting->internal_name; setting ++)
    {
      result = g_list_append (result, setting);
    }
  return result;
}

GHashTable *mypaint_brush_get_brush_text_settings_dict (void)
{
  if (!brush_text_settings_dict) {
    MyPaintBrushTextSettings *setting = text_list;
    brush_text_settings_dict = g_hash_table_new (g_str_hash, g_str_equal);
    for (; setting->internal_name; setting ++)
    {
      g_hash_table_insert (brush_text_settings_dict, setting->internal_name, setting);
    }
    g_hash_table_ref (brush_text_settings_dict);
  }
  g_hash_table_ref (brush_text_settings_dict);
  
  return brush_text_settings_dict;
}


GHashTable *mypaint_brush_get_setting_migrate_dict (void)
{
  if (!brush_setting_migrate_dict) {
    MyPaintBrushSettingMigrate *setting = migration;
    brush_setting_migrate_dict = g_hash_table_new (g_str_hash, g_str_equal);
    for (; setting->old_name; setting ++)
    {
      g_hash_table_insert (brush_setting_migrate_dict, setting->old_name, setting);
    }
    g_hash_table_ref (brush_setting_migrate_dict);
  }
  g_hash_table_ref (brush_setting_migrate_dict);
  return brush_setting_migrate_dict;
}

GHashTable *mypaint_brush_get_setting_group_dict (void)
{
  gchar** name;
  if (!brush_setting_group_dict) {
    MypaintBrushSettingGroupInternal* group = group_list;
    GHashTable* settings_dict = mypaint_brush_get_brush_settings_dict ();
    GHashTable* switches_dict = mypaint_brush_get_brush_switch_settings_dict();
    GHashTable* text_dict     = mypaint_brush_get_brush_text_settings_dict ();
    brush_setting_group_dict = g_hash_table_new (g_str_hash, g_str_equal);
    for (; group->name; group ++)
    {
      MypaintBrushSettingGroup* group_entry = g_new0(MypaintBrushSettingGroup, 1);
      group_entry->name = group->name;
      group_entry->index = group->index;
      group_entry->display_name = group->display_name;
      group_entry->setting_list = NULL;

      for (name = group->setting_list; *name; name ++) {
        MyPaintBrushSettingEntry* entry;
        gpointer setting;
        entry = g_new0(MyPaintBrushSettingEntry, 1);
        setting = g_hash_table_lookup (settings_dict, *name);
        if (!setting)
          setting = g_hash_table_lookup (switches_dict, *name);
        if (!setting)
            setting = g_hash_table_lookup (text_dict, *name);
        
        if (setting) {
          entry->ptr  = setting;
          entry->type = mypaint_brush_get_prop_type(*name);
          group_entry->setting_list = g_list_append (group_entry->setting_list, entry);
        } else {
          g_print("BrushSettingGroup::NOT_FOUND::%s\n", *name);
          }
        }

      g_hash_table_insert (brush_setting_group_dict, group->name, group_entry);
    }
    g_hash_table_unref(settings_dict);
    g_hash_table_unref(switches_dict);
    g_hash_table_unref(text_dict);  

    g_hash_table_ref (brush_setting_group_dict);
  }

  g_hash_table_ref (brush_setting_group_dict);

  return brush_setting_group_dict;
}

GList *mypaint_brush_get_setting_group_list (void)
{
  MypaintBrushSettingGroupInternal* group = group_list;
  GHashTable* brush_setting_group_dict = mypaint_brush_get_setting_group_dict();
  GList* result = NULL;
  for (; group->name; group ++)
  {
    MypaintBrushSettingGroup* setting_list = 
      (MypaintBrushSettingGroup*)g_hash_table_lookup(brush_setting_group_dict, group->name);
    result = g_list_append (result, setting_list);
  }
  g_hash_table_unref(brush_setting_group_dict);

  return result;
}

gchar* mypaint_brush_internal_name_to_signal_name (const gchar* name)
{
  gchar* result = g_strdup(name);
  g_strcanon(result,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-",'-');
  return result;
}

gchar* mypaint_brush_signal_name_to_internal_name (const gchar* name)
{
  gchar* result = g_strdup(name);
  g_strcanon(result,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_",'_');
  return result;
}

GType mypaint_brush_get_prop_type (const gchar* name)
{
  GType       result      = G_TYPE_NONE;
  GHashTable* switch_dict = mypaint_brush_get_brush_switch_settings_dict();
  GHashTable* text_dict   = mypaint_brush_get_brush_text_settings_dict();
  GHashTable* float_dict  = mypaint_brush_get_brush_settings_dict();

  if (g_hash_table_lookup(switch_dict, name)) {
    result = G_TYPE_BOOLEAN;
  } else if (g_hash_table_lookup(text_dict, name)) {
    result = G_TYPE_STRING;
  } else if (g_hash_table_lookup(float_dict, name)) {
    result = G_TYPE_DOUBLE;
  }
  g_hash_table_unref(switch_dict);
  g_hash_table_unref(text_dict);
  g_hash_table_unref(float_dict);
  return result;
}

gchar* mypaint_brush_settings_index_to_internal_name(int index)
{
  if (!index_to_name_dict) {
    MyPaintBrushSettings* s1;
    MyPaintBrushSwitchSettings* s2;
    MyPaintBrushTextSettings* s3;
    index_to_name_dict = g_hash_table_new(g_direct_hash, g_direct_equal);
    for (s1 = settings_list; s1->internal_name; s1 ++) {
      g_hash_table_insert(index_to_name_dict, (gpointer)(s1->index), s1->internal_name);
    }
    for (s2 = switches_list; s2->internal_name; s2 ++) {
      g_hash_table_insert(index_to_name_dict, (gpointer)(s2->index), s2->internal_name);
    }
    for (s3 = text_list; s3->internal_name; s3 ++) {
      g_hash_table_insert(index_to_name_dict, (gpointer)(s3->index), s3->internal_name);
    }
  }
  return (gchar*)g_hash_table_lookup(index_to_name_dict, (gpointer)index);
}
