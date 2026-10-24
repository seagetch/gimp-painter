/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYPAINT_PAINT_CORE_HPP
#define GIMP_PAINTER_MYPAINT_PAINT_CORE_HPP
#include "../../painter/gimp-painter-visibility.h"
#include "gimp-resources.hpp"
#include <stdexcept>
#include "../painter-mypaint/engine.hpp"
typedef struct _GimpPaintOptions GimpPaintOptions;
typedef struct _GimpDrawable GimpDrawable;
namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint {
/* Extended engine/controller with a real current GimpPaintCore transaction.
 * One controller persists brush states across logical strokes, as the old core.
 * This is not the standard MyPaint engine or an alternate GObject framework. */
class TransactionConflict final : public std::runtime_error {
public:
  static const char* message() noexcept {return "Paint transaction conflicts with externally changed drawable; rollback was not performed";}
  TransactionConflict():std::runtime_error(message()){}
};
class PaintCore
{
public:
  PaintCore (GimpPaintOptions *options, const Resource& resource);
  ~PaintCore ();
  PaintCore (const PaintCore&) = delete;
  PaintCore& operator= (const PaintCore&) = delete;
  void configure (const Resource& resource);
  bool stroke_to (GimpDrawable *drawable, double seconds, const GimpCoords& coords);
  // Advance the same evaluator with zero pressure and real color sampling,
  // without a new native transaction. Positive-pressure interpolation may finish
  // an existing press's tail; settled hover only samples. Logical splits apply.
  bool hover_to (GimpDrawable *drawable, double seconds, const GimpCoords& coords);
  // Direct external buffer/geometry changes terminate this controller with
  // TransactionConflict. No rollback is performed; dispose it before reuse.
  // Baseline/checkpoint remain retained until disposal; Save is not blocked.
  void finish ();
  void cancel ();
  // Generic operations use one native snapshot while logical evaluator/Surface
  // segments remain separate. Interactive callers keep the default behavior.
  void begin_batch (bool push_undo, const char *undo_description = nullptr);
  void next_segment ();
  void end_batch (bool commit);
  bool active () const;
  // Read-only history/continuation observability; no native owner escapes.
  GimpCoords continuation_coords () const;
  Resource resource_snapshot () const;
  Engine::Checkpoint evaluator_checkpoint () const;
  ResourceResolution resolution () const;
  std::size_t bytes_read () const;
  std::size_t bytes_written () const;
private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};
} }
#endif
