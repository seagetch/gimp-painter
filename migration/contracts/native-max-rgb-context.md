# Native Max RGB owner context

Status: bounded RGB/RGBA nonlinear-U8 owner and strict XCF acceptance passed.
Fresh normal and focused ASan/UBSan each passed twenty exact paths; genuine
old-runtime and old-writer comparisons are included. Full aggregate, installed
UI, other operating systems and broad procedure parity remain separate gates.
See `migration/tests/native-max-rgb-context.json` and its sealed log artifacts.

## Admission and existing routes

The literal `plug-in-max-rgb` name claims the nonlinear-U8 route before argument
validation. Exactly four `G_TYPE_INT` slots are required: retained run mode,
image slot, drawable slot and signed INT32 `max_p` mode. Execution reads the
current owned input rather than the retained IDs. Raw bytes and original typed
arguments remain preserved when a definition is rejected.

Only RGB owners are admitted. RGB and RGBA lower sources are projected into
native-space straight RGBA8 transport; Gray and indexed owners are unsupported
on this raw legacy route. All signed INT32 modes are accepted: positive keeps
maximum channels, zero or negative keeps minimum channels, and ties survive.
There is no dialog-range clamp. The raw callback transforms hidden RGB too;
final replacement merge determines the visible and retained hidden result.

Other encodings keep the existing normalized-real point route, including its
ignored saved-context slot types and context-free behavior. Neither the legacy
byte/real point APIs nor Threshold's existing wrapper signatures are changed.
No general PDB descriptor, helper allowlist or wire protocol is added. The
bounded XCF route below is part of this task.

## Projection, raw shadow and final merge

The shared owner context captures the committed drawable cache first, then
copies only the current immediate-parent projection intersection from the lower
input proxy. Top-level clipping uses the image rectangle; child clipping uses
the parent group's actual offset and extent. Old completed/loaded pixels outside
that intersection remain input, including after an owner-only move.

A plain immutable captured region and full signed mode reach
`FilterPointShadowOperation::max_rgb`. The existing point-shadow validator
allows tiny positive ROIs, enforces its INT32 selected-area limit, zeroes private
full output, and filters only the start intersection. Zeroed bytes outside that
intersection must remain zero when the final selection changes. An outside
nonempty start selection carries `no_merge` through final import, even if the
selection subsequently clears.

Current final selection, component activity and the target's own alpha lock
are captured by the existing owner context and then sealed. Ancestor alpha lock
is not substituted for the target's own lock. Raw worker output and native import
staging remain private. Only complete, current-generation results replace the
completed drawable buffer on its owner thread. Thread execution does not imply
process/crash isolation or display-vsync atomicity. No worker modifies a displayed
completed buffer.

## Logical storage reservation

The route reserves native input, final mask and private staging at 9 bytes per
pixel, plus two RGBA vectors or spill files at 8 bytes per pixel. Bounded owner
capture/import and raw wrapper/kernel scratch reserve 304 KiB. The spill route
also charges six 32,768-pixel RGBA transport chunks, with exact logical spill
reservation of 17 bytes per pixel. These are admission calculations, not RSS,
GEGL-cache or latency measurements. The trusted application swap directory is
used; definitions cannot supply a storage path.

## Accepted synthetic and genuine tests

`app/tests/test-filter-max-rgb-context.inc` registers fourteen focused paths:
strict integers and retained definitions; Gray rejection; signed modes, ties,
RGB/RGBA and alpha values; tiny crops; late and sealed contexts; all sixteen RGBA
component masks with soft selection; outside-start `no_merge`; cancel, replacement,
lower notification and close; real partial-import interruption and retained
completed-buffer bytes; native Adobe RGB vector/spill bytes; admission refusal
and exact spill quota; all seventeen non-U8-nonlinear component/TRC combinations
plus real spill; and prior/seeded cache with parent bounds and owner-only movement.

The scalar raw and merge oracles are test-local and source-backed. They are not
old-runtime observations. Genuine base and large captures now cover 220 cases, including 126 live scenes
and 176 output comparisons. Projection history covers 16 scenes / 48 outputs.
These are separate observed bytes, not outputs of the port's scalar oracle.

## Strict XCF acceptance

Four strict stored integer arguments are activated only for the exact literal
68-byte legacy capsule: tags 2/1/10/2/0 and payload sizes 4/0/0/4/0. Framing,
name, lengths, kinds, END position and tails are checked. Signed scalar bits and
the original capsule survive; image/drawable slots become typed current-context
placeholders. Fourteen actual old-writer files passed ordinary Open, duplicate,
Save/reopen, changed/unchanged lower notifications and definition Undo/Redo.
Sixteen malformed capsules remain inert; four min/max vector-running and
spill-importing Save cases retain only completed pixels across reopen.

## Validation boundaries

The final sanitizer run instruments 40 translation units and compiles 36 other
C++ units with RTTI only. It preserves 3,064 source/dependency, 118 ordinary-build,
2 private-executable and 5,537 fixture seals. Remaining GIMP/dependencies are
uninstrumented and LeakSanitizer is disabled. No unexpected runtime diagnostics
were observed. The first attempt's four XCF dynamic-link failures are retained;
registered Meson library paths now prevent loading the extracted system GIMP
library. Fresh final normal and sanitizer runs both pass after 19 runner controls.
The initial old-capture relative-prefix failure is retained with its fix/control.

Installed UI, full aggregate, general precision parity, other OS and whole-port
acceptance remain separate obligations.
