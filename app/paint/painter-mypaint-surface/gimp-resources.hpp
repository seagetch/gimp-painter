/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYPAINT_GIMP_RESOURCES_HPP
#define GIMP_PAINTER_MYPAINT_GIMP_RESOURCES_HPP
#include "gegl-surface.hpp"
#include "paint/painter-mypaint/resource.hpp"
typedef struct _GimpContext GimpContext;
typedef struct _GimpBrush GimpBrush;
typedef struct _GimpPattern GimpPattern;
typedef struct _GimpCoords GimpCoords;
namespace GimpPainter { namespace MyPaint {
struct ResourceResolution {
  bool brush_missing = false, texture_missing = false;
  std::string requested_brush, requested_texture;
  std::string effective_brush, effective_texture;
};
/* Owns typed GIMP resources and balanced brush-use intervals. Surface callbacks
 * retain this state, so it can outlive the controller without dangling refs. */
class GimpResources
{
public:
  enum class Purpose { Stroke, Preview };
  GimpResources (GimpContext *context, const Resource& resource, Purpose purpose = Purpose::Stroke);
  ~GimpResources ();
  GimpResources (const GimpResources&) = delete;
  GimpResources& operator= (const GimpResources&) = delete;
  ResourceResolution resolution () const;
  void set_brush (GimpBrush *brush);
  void set_pattern (GimpPattern *pattern);
  void set_coords (const GimpCoords& coords);
  void attach (GeglSurface& surface);
private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};
} }
#endif
