# Original WBS 08.006: registered Painter paint core

The legacy GimpMypaintCore is a plain C++ controller, not a GObject subclass.
Its responsibilities now span a native registered GimpPainterPaintGate,
GimpPainterSession and the GEGL-backed MyPaint::PaintCore. The adapter directly
derives from current GimpPaintCore and registers through gimp_paint_init with
GimpPaintInfo and the Painter options type. Generic coordinate/path/boundary
entrypoints dispatch to its owned Session. Interactive input uses the tool's
persistent Session. This is one extended renderer, not a second implementation.

The added native registration test obtains the actual PaintInfo by identifier,
checks its core/options types and native parent/struct sizes, then constructs
the core and options through that registration. Direct low-level start is
intentionally rejected before preview freeze, pixels or Undo change. The public
gimp_paint_core_stroke entrypoint then paints real pixels and produces exactly
one Undo. Real Undo and Redo reproduce the before/after buffers. Repeated dispose
and final unref release the adapter. This proves usable registered dispatch;
it does not claim that every native virtual painting route is supported.

The existing options-init child is satisfied by owned semantics. The old raw
options member was not initialized before its first comparison. A Session's
ObjectRef starts null, retains its construct-only options before signal setup and
initial refresh, and keeps options/core alive locally while disconnecting during
close. The new lifetime test releases the caller's last options reference before
the first stroke; the Session keeps the model valid, paints, finishes and accepts
a settings update. Disposal then releases that ownership. A second case keeps
the caller reference and verifies the settings-changed handler is removed before
later edits. Both cases exercise repeated disposal. The existing closed-options
case separately closes the model during painting and checks actual rollback and
rejection. This is not a claim that borrowed options survive target destruction.
The notify connection uses the same close path; only settings-changed is directly
counted by the new handler-presence assertion.

All ten native Session cases pass with 26 current source seals. Existing cases
cover controller pixels, settings splits, close/cancel during native start,
settings reentry, closed options, last-session-reference notifications and stop
while sampling. The two pinned source files each carry the original type duty
and the existing options-init child, giving four accepted source-specific duties.
No routing correction or new child task is added. The stale early tool contract
now points to implemented generic stroking while preserving its historical
low-level refusal scope. Broader renderer/GUI/path/tablet/platform and sanitizer
gates are not inferred from this bounded native run.
