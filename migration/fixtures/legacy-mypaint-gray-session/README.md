# Native one-byte Gray session reference

This capture executes the real pinned `GimpMypaintCore` and its original native
one-byte Gray drawable implementation from commit
`afa43fae3e920210146abed514f136fd49f671b5`. Existing old application archives were
linked into a separate harness without rebuilding or replacing them. The report
records their unchanged hashes, the behavior sources and executable hash.

The 48 explicitly synthetic stimuli combine bitmap shape, paper, offset
selection and smudge with normal, eraser and actual layer alpha-lock states.
The target is a 130x96 Gray image at offset (7,3), with full byte output after
finish, Undo and Redo. All 193 records total 3,599,558 bytes, SHA256
`28cc932d137e760f4b6d5200994cbe6d22779443b23875e585c24ea58de6e7dc`.
The captured source specifies every input and setup step.

All scenes are incremental. The warm-up, actual Undo, undo-history clear and
brush reset from the separately sealed RGB harness are retained explicitly;
this does not establish cold-start equivalence. Gray-plus-alpha and Gray
nonincremental are outside this positive reference. The original pixel modes
have no two-byte case, and the floating pixmap iterator addresses alpha at
byte 3, so those paths require a separately defined safe policy rather than
an unsupported parity claim.

The old one-byte blending uses the red component directly, not RGB luminance.
Sampling returns the same gray in R, G and B with alpha one. Native target
bytes must not be silently converted through RGBA to approximate this behavior.

`session-values.tsv.gz` contains all pixel and Undo records once. The separate
runtime log contains only diagnostics. Optional reproduction uses
`migration/tests/capture_mypaint_gray_sessions.py` after sourcing the legacy
build environment and holding `/workspace/shared/gimp-painter-build.lock`.
It substitutes only the harness input path and never rebuilds old archives.

## Separate unsupported-path diagnostic experiments

Two bounded real-old runs per mode preserve distinct output and source hashes.
Gray-alpha emits 55 unsupported two-byte-layer diagnostics and its finish pixels
are unchanged. Gray nonincremental emits 52 diagnostics and does alter pixels.
The repeated record hashes agree on this particular x86 build; repeatability
does not make the unimplemented two-byte blending/byte-3 access a defined
rendering contract. These files are explicitly negative/setup observations,
not additions to the 48-scene positive oracle. Archives remained unchanged.

## Independent port candidate comparison

The privately compiled GEGL Surface candidate accepts native nonlinear one-byte
Gray without converting target storage. Its actual native PaintCore session
output matches every one of the 193 old records byte for byte, including all
finish/Undo/Redo images. `private-comparison.json` records the commands, source
and executable hashes; the private candidate source is retained. This normal
comparison is separate from forthcoming registered-target and sanitizer gates.
