# Perspective input and full-motion delivery

This is the dependent production-event slice after model/editor/overlay commit
`1f8376bfed`. It retains the pinned old helper's candidate ordering, inclusive
32-scaled-pixel threshold and dominant-coordinate constraint.

## Production route

The native statusbar has a session-only “Snap to perspective” toggle, initially
false on a new display shell. It is not serialized to XCF or preferences. An
existing empty shell reused for another image keeps its session toggle, while
its pending/locked stroke state and owned references are reset.

A normal left press begins immediately when the toggle is off, the tool opts
out, or there are no valid vanishing points. Otherwise the shell retains the
initiating tool/image/model and saves the complete `GimpCoords` origin. The
legacy code delayed even without a guide and swallowed a motionless click;
bypassing that no-guide delay is an explicit tested defect correction.

Until a sample reaches 32 scaled pixels, no tool press is delivered. At the
activation sample, the tool is initialized and pressed at the saved origin;
pressure, tilt, wheel, distance, rotation and slider are not replaced by the
activation sample's axes. As in the pinned implementation, that threshold
sample itself is not delivered as a motion. Native GIMP velocity/direction
initialization is retained. Short release and Escape cancel the pending gesture.

Subsequent live and historical image-coordinate samples share
`gimp_display_shell_perspective_motion()`. Constraint precedes motion-buffer
filtering/interpolation and paint-tool smoothing/symmetry. Autoscroll uses the
same route while a ruler gesture is pending or locked. Release coordinates are
also constrained, so the final input cannot leave the selected line.

A previous hover curve is flushed and its interpolation history discarded when
a ruler is armed; the latest evaluated dynamics remain available. The buffer is seeded with the
saved origin when the delayed press starts, so a repeated endpoint or a
press arriving without a preceding hover cannot be filtered against a previous
stroke's endpoint. This prevents
an earlier off-line hover trajectory from bending the beginning of a constrained
stroke. Stroke end does not constrain later hovering.

## Lifetime and replacement

Release, Escape cancellation, image disconnect/disposal, active-tool change,
guide replacement/removal and disabling the toggle invalidate ruler state.
GObject references are published/cleared before potentially reentrant unrefs.
The initiating tool is retained around initialization and button delivery,
and a changed recipient is not silently pressed as part of the old gesture.

The motion buffer has an explicit cancel-without-delivery operation. Tool/image
replacement discards delayed and queued samples instead of replaying them into
a replacement recipient. A generation check terminates a device-history batch
when a delivery callback changes the recipient. Delayed-start state never
crosses image/tool identity boundaries. Pressure-only samples with identical
positions/timestamps are preserved using the pinned old pressure comparison.

## Base tool contract

`GimpTool.want_full_motion_tracking` and `GimpTool.disable_lazy_snap` both default
to false. The ruler sets `disable_lazy_snap`. The extended MyPaint tool opts into
full-motion tracking in its own registered-subclass slice. Opted-in tools receive
motion while inactive/hovering, including stationary pressure changes, through
the native tool dispatch guard. Normal tools retain hover/active-stroke separation.

The session toggle does not create a ruler, and there is no invented XCF field.
Existing navigation modifier/button handling is unchanged and is rerun against
the expanded event route.

## Verification scope

The event regression executable creates a real native GTK/GIMP display and
uses actual GDK canvas events with a recording GimpTool. Button grabs use
GDK_CURRENT_TIME, as required by the X11 backend. It verifies short/threshold
activation, release projection, no-guide clicks, statusbar control, cancellation,
image/tool/model replacement, callback-time replacement, full-tracking hover and
stationary/equal-time pressure delivery, real ruler editing bypass, three-point
constraints under zoom/rotation/both flips and isolation from prior hover
interpolation. The shared production history-sample helper is tested with full
axes that a synthetic mouse cannot supply.

This does not claim physical tablet, driver history acquisition, real hardware
latency, every brush engine's smoothing/symmetry composition or an old interactive
GUI replay. Those broader gates remain separate. Focused ASan/UBSan and existing
navigation regression results are recorded with exact final source hashes.
