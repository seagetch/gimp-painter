/* SPDX-License-Identifier: ISC */
#include <assert.h>
#include <string.h>
#include "mypaintbrush-settings-data.h"
#include "mypaintbrush-settings-data.h"

/* Array lengths are explicit, so consumers never depend on legacy sentinels. */
int
main (void)
{
  int i;
  assert (INPUT_COUNT == 9 && BRUSH_MAPPING_COUNT == 45);
  assert (BRUSH_BOOL_COUNT == 5 && BRUSH_TEXT_COUNT == 2);
  assert (BRUSH_SETTINGS_COUNT == 52 && STATE_COUNT == 30);
  for (i = 0; i < INPUT_COUNT; i++)
    {
      assert (painter_mypaint_inputs[i].index == i);
      assert (painter_mypaint_inputs[i].name[0] != '\0');
    }
  for (i = 0; i < BRUSH_MAPPING_COUNT; i++)
    {
      assert (painter_mypaint_settings[i].index == i + BRUSH_MAPPING_BASE);
      assert (painter_mypaint_settings[i].internal_name[0] != '\0');
    }
  for (i = 0; i < BRUSH_BOOL_COUNT; i++)
    assert (painter_mypaint_switches[i].index == i + BRUSH_BOOL_BASE);
  for (i = 0; i < BRUSH_TEXT_COUNT; i++)
    {
      assert (painter_mypaint_texts[i].index == i + BRUSH_TEXT_BASE);
      assert (painter_mypaint_texts[i].default_value == NULL);
    }
  for (i = 0; i < STATE_COUNT; i++)
    {
      assert (painter_mypaint_states[i].index == i);
      assert (painter_mypaint_states[i].constructor_default == 0.0f);
      assert (painter_mypaint_states[i].name[0] != '\0');
    }
  assert (painter_mypaint_inputs[INPUT_CUSTOM].hard_minimum == FLT_MAX);
  assert (painter_mypaint_inputs[INPUT_PRESSURE].hard_maximum == FLT_MAX);
  assert (painter_mypaint_settings[BRUSH_STROKE_OPACITY].constant == 1);
  assert (strcmp (painter_mypaint_settings[BRUSH_TEXTURE_GRAIN].internal_name,
                  "texture_grain") == 0);
  assert (painter_mypaint_switches[BRUSH_USE_GIMP_BRUSHMARK - BRUSH_BOOL_BASE].default_value == 1);
  assert (strcmp (painter_mypaint_states[STATE_LAST_GETCOLOR_RECENTNESS].name,
                  "last_getcolor_recentness") == 0);
  return 0;
}
