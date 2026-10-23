/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 09.007: a native layer mask is projection state, or a selected paint
 * drawable. It must never become the selection supplied to raw painting. */
extern "C" {
#include "core/gimpchannel.h"
#include "core/gimplayermask.h"
}
#include "paint/painter-mypaint-surface/paint-core.hpp"

static GimpLayerMask *
layer_mask_fixture (Scene &s)
{
  g_assert_true (gimp_channel_is_empty (gimp_image_get_mask (s.image)));
  auto *layer = GIMP_LAYER (s.drawable);
  auto *mask = gimp_layer_create_mask (layer, GIMP_ADD_MASK_BLACK, nullptr);
  GError *error = nullptr;
  g_assert_true (gimp_layer_add_mask (layer, mask, FALSE, &error) == mask);
  g_assert_no_error (error);
  gimp_layer_set_edit_mask (layer, FALSE);
  gimp_layer_set_show_mask (layer, FALSE, FALSE);
  gimp_image_undo_free (s.image);
  return mask;
}

static void
layer_mask_raw_paint_apply_parity ()
{
  Scene s;
  auto *mask = layer_mask_fixture (s);
  auto *layer = GIMP_LAYER (s.drawable);
  auto *color = gegl_color_new ("rgba(0.2,0.4,0.6,0.8)");
  gegl_buffer_set_color (gimp_drawable_get_buffer (s.drawable), nullptr, color);
  g_object_unref (color);
  const auto initial = native_pixels (s.drawable);
  const auto mask_initial = native_pixels (GIMP_DRAWABLE (mask));
  std::vector<guchar> painted;
  std::array<float, 4> sampled = {};
  std::size_t reads = 0, writes = 0;
  for (bool apply : {false, true})
    {
      gimp_layer_set_apply_mask (layer, apply, FALSE);
      MyPaint::GeglSurface raw (gimp_drawable_get_buffer (s.drawable));
      std::array<float, 4> sample = {};
      raw.get_color (32, 32, 4, &sample[0], &sample[1], &sample[2], &sample[3]);
      g_assert_cmpfloat (sample[3], >, .5f);
      if (apply) g_assert_true (sample == sampled);
      else sampled = sample;
      MyPaint::Resource resource;
      resource.set_base_value (BRUSH_DABS_PER_SECOND, 40);
      resource.set_base_value (BRUSH_RADIUS_LOGARITHMIC, 1.5);
      MyPaint::PaintCore core (GIMP_PAINT_OPTIONS (s.options), resource);
      GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
      coords.x = 32; coords.y = 32; coords.pressure = 0;
      core.stroke_to (s.drawable, .01, coords);
      coords.pressure = 1;
      for (int i = 0; i < 5; ++i)
        {
          coords.x += 3;
          core.stroke_to (s.drawable, .05, coords);
        }
      core.finish ();
      const auto actual = native_pixels (s.drawable);
      g_assert_true (actual != initial);
      g_assert_cmpuint (core.bytes_written (), >, 0);
      if (apply)
        {
          g_assert_true (actual == painted);
          g_assert_cmpuint (core.bytes_read (), ==, reads);
          g_assert_cmpuint (core.bytes_written (), ==, writes);
        }
      else
        {
          painted = actual;
          reads = core.bytes_read (); writes = core.bytes_written ();
        }
      g_assert_true (native_pixels (GIMP_DRAWABLE (mask)) == mask_initial);
      g_assert_cmpint (s.depth (), ==, 1);
      g_assert_true (gimp_image_undo (s.image));
      g_assert_true (native_pixels (s.drawable) == initial);
      gimp_image_undo_free (s.image);
    }
}

static std::array<guchar, 4>
layer_mask_projection_pixel (Scene &s)
{
  gimp_drawable_update (s.drawable, 0, 0, 128, 128);
  gimp_image_flush (s.image);
  gimp_test_run_mainloop_until_idle ();
  gimp_pickable_flush (GIMP_PICKABLE (s.image));
  std::array<guchar, 4> pixel = {};
  gegl_buffer_get (gimp_pickable_get_buffer (GIMP_PICKABLE (s.image)),
                   GEGL_RECTANGLE (32, 32, 1, 1), 1, babl_format ("R'G'B'A u8"),
                   pixel.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  return pixel;
}

static void
layer_mask_projection_flags ()
{
  Scene s;
  auto *mask = layer_mask_fixture (s);
  auto *layer = GIMP_LAYER (s.drawable);
  auto *red = gegl_color_new ("red");
  gegl_buffer_set_color (gimp_drawable_get_buffer (s.drawable), nullptr, red);
  g_object_unref (red);
  const auto raw = native_pixels (s.drawable);
  gimp_layer_set_apply_mask (layer, TRUE, FALSE);
  g_assert_cmpint (layer_mask_projection_pixel (s)[3], ==, 0);
  gimp_layer_set_apply_mask (layer, FALSE, FALSE);
  const auto unmasked = layer_mask_projection_pixel (s);
  g_assert_cmpint (unmasked[0], ==, 255);
  g_assert_cmpint (unmasked[1], ==, 0);
  g_assert_cmpint (unmasked[2], ==, 0);
  g_assert_cmpint (unmasked[3], ==, 255);
  for (bool apply : {false, true})
    {
      gimp_layer_set_apply_mask (layer, apply, FALSE);
      gimp_layer_set_show_mask (layer, TRUE, FALSE);
      const auto shown = layer_mask_projection_pixel (s);
      g_assert_cmpint (shown[0], ==, 0);
      g_assert_cmpint (shown[1], ==, 0);
      g_assert_cmpint (shown[2], ==, 0);
      g_assert_cmpint (shown[3], ==, 255);
    }
  // Clearing show must reconnect exactly the current apply state, including
  // an apply change made while the mask itself is being displayed.
  for (bool apply : {false, true})
    {
      gimp_layer_set_show_mask (layer, TRUE, FALSE);
      gimp_layer_set_apply_mask (layer, apply, FALSE);
      gimp_layer_set_show_mask (layer, FALSE, FALSE);
      const auto restored = layer_mask_projection_pixel (s);
      if (apply) g_assert_cmpint (restored[3], ==, 0);
      else g_assert_true (restored == unmasked);
    }
  // Set and read an explicit linear Y value through babl rather than assuming
  // the native mask is nonlinear byte storage. A second application gives
  // approximately 64 alpha instead of 128 and must fail this assertion.
  std::vector<float> half_mask (128 * 128, .5f);
  auto *mask_buffer = gimp_drawable_get_buffer (GIMP_DRAWABLE (mask));
  gegl_buffer_set (mask_buffer, GEGL_RECTANGLE (0, 0, 128, 128), 0,
                   babl_format ("Y float"), half_mask.data (), GEGL_AUTO_ROWSTRIDE);
  float stored = 0;
  gegl_buffer_sample (mask_buffer, 32, 32, nullptr, &stored,
                      babl_format ("Y float"), GEGL_SAMPLER_NEAREST, GEGL_ABYSS_NONE);
  g_test_message ("layer mask native format=%s; set Y float=0.5, read=%.9g",
                  babl_get_name (gegl_buffer_get_format (mask_buffer)), stored);
  g_assert_cmpfloat_with_epsilon (stored, .5f, 1. / 255.);
  gimp_drawable_update (GIMP_DRAWABLE (mask), 0, 0, 128, 128);
  gimp_layer_set_apply_mask (layer, TRUE, FALSE);
  const auto partial = layer_mask_projection_pixel (s);
  g_assert_cmpint (partial[0], ==, 255);
  g_assert_cmpint (partial[1], ==, 0);
  g_assert_cmpint (partial[2], ==, 0);
  g_assert_cmpfloat_with_epsilon (partial[3], stored * 255., 1.);
  gimp_layer_set_apply_mask (layer, FALSE, FALSE);
  g_assert_true (layer_mask_projection_pixel (s) == unmasked);
  gimp_layer_set_apply_mask (layer, TRUE, FALSE);
  g_assert_true (layer_mask_projection_pixel (s) == partial);
  g_assert_true (native_pixels (s.drawable) == raw);
}

static void
layer_mask_tool_target_undo_cancel ()
{
  for (bool apply : {false, true})
    {
      Scene s;
      gimp_item_set_offset (GIMP_ITEM (s.drawable), 23, 17);
      auto *mask = layer_mask_fixture (s);
      auto *target = GIMP_DRAWABLE (mask);
      auto *layer = GIMP_LAYER (s.drawable);
      gimp_layer_set_apply_mask (layer, apply, FALSE);
      gimp_layer_set_edit_mask (layer, TRUE);
      auto *selected = gimp_image_get_selected_drawables (s.image);
      g_assert_cmpuint (g_list_length (selected), ==, 1);
      g_assert_true (selected->data == target);
      g_list_free (selected);
      gint x = 0, y = 0;
      gimp_item_get_offset (GIMP_ITEM (mask), &x, &y);
      g_assert_cmpint (x, ==, 23); g_assert_cmpint (y, ==, 17);
      const auto *format = gimp_drawable_get_format (target);
      g_assert_true (format == gimp_image_get_mask_format (s.image));
      g_assert_false (babl_format_has_alpha (format));
      const auto layer_before = native_pixels (s.drawable);
      const auto mask_before = native_pixels (target);
      auto *white = gegl_color_new ("white");
      gimp_context_set_foreground (GIMP_CONTEXT (s.options), white);
      g_object_unref (white);
      auto paint = [&] () {
        s.coords.x = 32 + x; s.coords.y = 32 + y;
        s.motion (false); s.press ();
        for (int i = 0; i < 4; ++i) { s.coords.x += 3; s.motion (true, 75); }
        g_assert_true (native_pixels (target) != mask_before);
        g_assert_true (native_pixels (s.drawable) == layer_before);
      };
      paint ();
      s.release ();
      gimp_tool_control (s.tool, GIMP_TOOL_ACTION_COMMIT, s.display);
      const auto mask_painted = native_pixels (target);
      g_assert_cmpint (s.depth (), ==, 1);
      // Image-space input must land at local y=32, not unadjusted y=49.
      guchar local = 0, untranslated = 0;
      gegl_buffer_sample (gimp_drawable_get_buffer (target), 38, 32, nullptr,
                          &local, babl_format ("Y u8"), GEGL_SAMPLER_NEAREST,
                          GEGL_ABYSS_NONE);
      gegl_buffer_sample (gimp_drawable_get_buffer (target), 61, 49, nullptr,
                          &untranslated, babl_format ("Y u8"), GEGL_SAMPLER_NEAREST,
                          GEGL_ABYSS_NONE);
      g_assert_cmpint (local, >, 0); g_assert_cmpint (untranslated, ==, 0);
      g_assert_true (gimp_image_undo (s.image));
      g_assert_true (native_pixels (target) == mask_before);
      g_assert_true (native_pixels (s.drawable) == layer_before);
      g_assert_true (gimp_image_redo (s.image));
      g_assert_true (native_pixels (target) == mask_painted);
      g_assert_true (gimp_image_undo (s.image));
      gimp_image_undo_free (s.image);
      paint ();
      gimp_tool_button_release (s.tool, &s.coords, s.time + 20,
                               GDK_BUTTON3_MASK, s.display);
      g_assert_true (native_pixels (target) == mask_before);
      g_assert_true (native_pixels (s.drawable) == layer_before);
      g_assert_true (gimp_drawable_get_format (target) == format);
      g_assert_cmpint (s.depth (), ==, 0);
      g_assert_false (gimp_painter_mybrush_tool_has_pending_stroke (
                       GIMP_PAINTER_MYBRUSH_TOOL (s.tool)));
      gimp_layer_set_edit_mask (layer, FALSE);
      selected = gimp_image_get_selected_drawables (s.image);
      g_assert_cmpuint (g_list_length (selected), ==, 1);
      g_assert_true (selected->data == s.drawable);
      g_list_free (selected);
    }
}
