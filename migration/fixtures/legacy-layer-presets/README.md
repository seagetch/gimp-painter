# Native pinned layer-preset construction oracle

`capture.cpp` calls the real unchanged old `ILayerPresetApplier` from pinned
`afa43fae3e920210146abed514f136fd49f671b5` for all eight bundled JSON files.
Each uses a 100×80 RGB image, source rectangle (7,9,40,30), source screen mode,
opacity .6 and rectangular selection (12,15,20,17). `test.json` and `test2.json`
have source FilterLayers; the rest use normal alpha sources. TSV records ordered
paths, actual runtime native types/names/modes/opacity/rectangle, original source
identity, clone source identity, filter procedure and typed positional values.

`runtime.json` records source/archive/binary identities and compile/link commands.
`runtime-capture.log.gz` and stderr retain old diagnostics. No application source
was patched for this capture. The existing old-build compatibility environment
is inherited and is not represented as an unmodified system dependency stack.

The old JsonResource finalizer calls g_object_unref on its JsonNode and crashes.
The harness therefore retains its eight resources until process exit. It still
uses proper `delete` for every old C++ applier. Native port cleanup is separately
tested; this fixture is a construction oracle, not old memory-safety evidence.
The recorded old parameter types GimpInt32, GimpImageID and GimpDrawableID derive
from signed int. The port comparison explicitly maps those ABI types; float
JSON values become gdouble after the old runner's conversion, as recorded.

The synthetic harness is GPL-3.0-or-later. No private artwork is used.
