/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYPAINT_GEGL_SURFACE_HPP
#define GIMP_PAINTER_MYPAINT_GEGL_SURFACE_HPP
#include "../../painter/gimp-painter-visibility.h"
#include <gegl.h>
#include "paint/painter-mypaint/surface.hpp"
#include <functional>
#include <memory>
#include <vector>
namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint {
struct ShapeMask {
  int width = 0, height = 0;
  std::vector<unsigned char> pixels;
};
struct Texture {
  int width = 0, height = 0;
  std::vector<unsigned char> values; // exact first channel of old pattern bytes
};
/* Exact historical nonlinear-byte rendering plus a native-TRC double extension.
 * Native storage is retained; evaluator colors use drawable-space nonlinear RGB.
 * Shape transformation/resource selection is injected and separately tested. */
class GeglSurface : public Surface
{
public:
  using ShapeProvider = std::function<ShapeMask (float radius, float hardness, float aspect, float angle)>;
  explicit GeglSurface (GeglBuffer *target);
  ~GeglSurface () override;
  GeglSurface (const GeglSurface&) = delete;
  GeglSurface& operator= (const GeglSurface&) = delete;
  void set_shape_provider (ShapeProvider provider);
  using TextureProvider = std::function<std::shared_ptr<const Texture> ()>;
  void set_texture (Texture texture);
  void set_texture_provider (TextureProvider provider);
  void set_selection (GeglBuffer *selection, int drawable_offset_x, int drawable_offset_y);
  void set_non_incremental (bool value);
  void set_stroke_opacity (float value);
  void set_background (double r, double g, double b);
  static const Babl* evaluation_format (GeglBuffer *target);
  void set_dirty_callback (std::function<void (const GeglRectangle&)> callback);
  const GeglRectangle& dirty () const;
  std::size_t bytes_read () const;
  std::size_t bytes_written () const;
  bool draw_dab (float x,float y,float radius,float r,float g,float b,
                 float opaque,float hardness=.5,float alpha=1,float aspect=1,float angle=0,
                 float lock_alpha=0,float colorize=0,float grain=0,float contrast=1) override;
  void get_color (float x,float y,float radius,float *r,float *g,float *b,float *a,
                  float hardness=.5,float aspect=1,float angle=0,float grain=0,float contrast=1) override;
  void begin_session () override;
  void begin_session_from (GeglBuffer *initial_snapshot);
  void end_session () override;
  void cancel_session ();
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} }
#endif
