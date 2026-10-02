# Full old MyPaint session pixel oracle

This package records real pinned `GimpMypaintCore`, image/drawable Surface and
native Undo/Redo from `afa43fae3e920210146abed514f136fd49f671b5`, with deterministic
synthetic stimuli. The old side uses its own `.myb` loader, options, evaluator,
resource transforms, tile renderer and Undo. No new renderer participates in it.

The successful comparison contains **32 scenarios, 96 complete 130×96 RGBA
snapshots and 129 records / 9,587,537 bytes**. All records are exactly equal,
including finish/Undo/Redo bytes and Undo flags/depth. Scenarios vary:

- incremental/nonincremental accumulation
- ellipse/asymmetric ordinary bitmap brush
- absent/present RGB paper, using the original first channel
- absent/present selection with a nonzero drawable offset
- ordinary paint at stroke opacity 1 / smudge at stroke opacity .37

A stationary pressure rise precedes moving samples that cross 64-pixel tile
boundaries. Colors use explicitly specified nonlinear double RGB values on both
sides, on unprofiled nonlinear-u8 images. This establishes this specified contract,
not arbitrary ICC/color-space, gray/high-precision, brush-pipe, GUI/tablet or
platform parity. It is a small synthetic scenario matrix, not all177 rendered
brushes or historical user artwork.

## Warm-up and cold negative evidence

The pinned old nonincremental `begin_session` allocates floating tiles before its
GimpImageFeature refresh initializes `drawable_item`. The original tool constructs
GimpMypaintCore and calls stroke_to without an intervening explicit Surface
refresh. A separate valid-loader cold nonincremental attempt exits139. A second
otherwise equivalent harness with a diagnostic-only signal backtrace also exits139,
pointing to gimp_item_get_height → Surface begin_session → Core stroke_to. Both raw
logs, exact harness sources, link plans and executable hashes are retained. They
are negative observations, never passing compatibility evidence. gdb was not
available; the backtrace comes from the recorded signal-handler harness.

For the positive reference, every scenario first uses actual incremental stroke
calls on the same old core at (50,35), pressure0/.85 with .01/.1-second intervals,
then split_stroke, Undo and exact initial-buffer verification. Some selected
scenarios clip that warm-up, but the drawable feature is refreshed by the real
stroke path. The Undo stacks are cleared, reset_brush is called, the accumulation
switch is changed, and non-incremental notification re-copies old mappings. Only
then are the recorded events submitted. No old feature source/archive is patched.

The new controller starts cold and safely produces the same *warmed-reference*
pixels. This is not proof of equivalence to the crashing old cold-start path.

## Failures and provenance

The first new comparator failed to compile because its scratch harness omitted
`core/gimplayer-new.h`; its attempted run had no executable. Those failures are
kept distinctly. The first runnable comparator used GEGL `rgb()` color strings,
which produce different nonlinear values from the intended old GimpRGB stimuli:
alpha matched but RGB differed. Explicit R'G'B'A double color pixels fixed the
harness input and yielded exact equality without a production rendering change.

Raw old stderr includes startup GValue/pagecurl diagnostics and one GParamSpec
unref diagnostic. They are preserved, not suppressed. Fixture equality is the
specified pixel/Undo result, not an assertion that the entire old app is clean.

`comparison.json` records exact source/archive/binary/value hashes and scope.
`manifest.json` seals all files. Old sources include their original absolute input
path as actually compiled; a reproduction runner may substitute only that path
in a temporary harness copy. No inherited environment is stored in these files.
Run the native Meson target `painter-mypaint-legacy-session` to compare the port.
