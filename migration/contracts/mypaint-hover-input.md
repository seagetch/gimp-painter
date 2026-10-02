# Sampling-only inactive MyPaint input

This is an explicit safety correction for the registered tool's inactive input
route. Current GIMP coordinates default to pressure1 for a mouse without a
pressure axis. Neither that default nor a constant-opacity brush may turn hover
into painting. Simply skipping hover would lose evaluator motion, idle, tilt and
smudge state; setting pressure0 alone would still let a constant-opacity brush
write pixels.

`PaintCore::hover_to` and `gimp_painter_session_hover_to` advance the same extended
evaluator with pressure0. A hover Surface forwards actual shape-aware
color sampling to the real drawable. Cold and settled zero-pressure hover refuse
every dab. A real preceding press may retain positive-pressure interpolated
release dabs; those alone are forwarded to the existing active Surface and native
transaction. The actual old pipe oracle established this distinction; see
`mypaint-active-pipe-release.md`. An already active
transaction remains pending until the evaluator reports its logical split.
There is no unconditional release commit. Without an active transaction, a
transient GEGL/resource adapter samples without native start, preview freeze,
snapshot or Undo creation. A weak target identity preserves logical stroke
initialization without owning an old drawable or image.

Explicit finish/cancel clears logical identity. Resource callbacks cannot
recursively reconfigure or paint the currently executing evaluator. A lifecycle
epoch and deferred stop flags detect cancel/finish during resource setup and
prevent subsequent work. Stop requests during an active sample take effect after
the evaluator returns, with cancellation taking priority over finish.
Transient target/image/resource leases expire before hover returns. A subsequent
hover does not resurrect an ended native transaction; legitimate pending native
transactions retain their normal target/image leases until the legacy split.

Seven synthetic native GIMP cases pass normally and with focused ASan/UBSan/
float-cast-overflow: constant opacity at pressure0/default1, incremental and
nonincremental sampling with zero pixel writes and zero Undo; unchanged-position
hover preserving pending Undo until a later evaluator split; old image release
before and after finish; closed-adapter rejection; and recursive resource-setup
configure/motion rejection plus cancellation; and a real ordinary brush selector
receiving zero hover pressure with unchanged tilt; and positive-pressure release
tails with cumulative/nonincremental and constant/pressure-dependent opacity,
including settled hover, finish/cancel and real Undo/Redo. The focused runner instruments26
sources, uses private archives and leaves production objects untouched. LSan is
disabled; unlisted GIMP/dependency sources remain uninstrumented.

The nine Surface tests, eight session adapter tests and all129 records from the
independent warmed old active-session oracle still pass. That oracle proves its
specified active-stroke scenes, not this intentional hover write suppression.
The registered tool, delayed ruler start and synthetic GTK event path are now
covered separately in `mypaint-native-tool.md`; physical hardware remains open.
