# Sampling-only inactive MyPaint input

This is an explicit safety correction for the registered tool's inactive input
route. Current GIMP coordinates default to pressure1 for a mouse without a
pressure axis. Neither that default nor a constant-opacity brush may turn hover
into painting. Simply skipping hover would lose evaluator motion, idle, tilt and
smudge state; setting pressure0 alone would still let a constant-opacity brush
write pixels.

`PaintCore::hover_to` and `gimp_painter_session_hover_to` advance the same extended
evaluator with pressure0. A sampling-only Surface forwards actual shape-aware
color sampling to the real drawable but refuses every dab. An already active
transaction remains pending until the evaluator reports its logical split.
There is no unconditional release commit. Without an active transaction, a
transient GEGL/resource adapter samples without native start, preview freeze,
snapshot or Undo creation. A weak target identity preserves logical stroke
initialization without owning an old drawable or image.

Explicit finish/cancel clears logical identity. Resource callbacks cannot
recursively reconfigure or paint the currently executing evaluator. A lifecycle
epoch detects cancel/finish during resource setup and prevents subsequent work.
Transient target/image/resource leases expire before hover returns. A subsequent
hover does not resurrect an ended native transaction; legitimate pending native
transactions retain their normal target/image leases until the legacy split.

Five synthetic native GIMP cases pass normally and with focused ASan/UBSan/
float-cast-overflow: constant opacity at pressure0/default1, incremental and
nonincremental sampling with zero pixel writes and zero Undo; unchanged-position
hover preserving pending Undo until a later evaluator split; old image release
before and after finish; closed-adapter rejection; and recursive resource-setup
configure/motion rejection plus cancellation. The focused runner instruments26
sources, uses private archives and leaves production objects untouched. LSan is
disabled; unlisted GIMP/dependency sources remain uninstrumented.

The nine Surface tests, six session adapter tests and all129 records from the
independent warmed old active-session oracle still pass. That oracle proves its
specified active-stroke scenes, not this intentional hover write suppression.
Actual registered tool events, delayed ruler start and GUI/hardware behavior
remain separate gates.
