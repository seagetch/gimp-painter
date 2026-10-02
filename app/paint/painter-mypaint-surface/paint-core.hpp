/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYPAINT_PAINT_CORE_HPP
#define GIMP_PAINTER_MYPAINT_PAINT_CORE_HPP
#include "gimp-resources.hpp"
typedef struct _GimpPaintOptions GimpPaintOptions;
typedef struct _GimpDrawable GimpDrawable;
namespace GimpPainter { namespace MyPaint {
/* Extended engine/controller with a real current GimpPaintCore transaction.
 * One controller persists brush states across logical strokes, as the old core.
 * This is not the standard MyPaint engine or an alternate GObject framework. */
class PaintCore
{
public:
  PaintCore (GimpPaintOptions *options, const Resource& resource);
  ~PaintCore ();
  PaintCore (const PaintCore&) = delete;
  PaintCore& operator= (const PaintCore&) = delete;
  void configure (const Resource& resource);
  bool stroke_to (GimpDrawable *drawable, double seconds, const GimpCoords& coords);
  void finish ();
  void cancel ();
  bool active () const;
  ResourceResolution resolution () const;
  std::size_t bytes_read () const;
  std::size_t bytes_written () const;
private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};
} }
#endif
