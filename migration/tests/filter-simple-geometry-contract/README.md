# Translate/scale/resize scheduling parity — original 15.005 checkpoint

Pinned old `afa43fae` Filter wrappers call the native parent and then unconditionally reset/invalidate. Read-only audit pins old bytes and the current e68ce702a0 matrix checkpoint. Current inherited native geometry handles ordinary changed bounds correctly, but same-size/zero-displacement calls do not emit the property signals on which the port relied.

## Observed red and bounded correction
Six independent processes were run. Changed translate/scale/resize all **3 PASS** before the fix. Zero translate, same-size scale and no-op resize each failed only the old scheduling requirement: generation2→2; buffer notifications respectively0,1,0. **These are scheduling-parity mismatches, not demonstrated wrong-pixel defects.** Native resize explicitly returns early in that no-op; scale still replaces its buffer. All original red outcomes are preserved.

Three narrow overrides retain owner/image, delegate using current ABI, recheck binding epoch without a borrowed FilterImpl spanning native callbacks, and invalidate afterward. Original native rounding, fill type, Undo entry points and geometry work are retained. Changed source/geometry callbacks may also invalidate, as before; no generic buffer observer is introduced.

Green **native11 PASS**: six geometry cases plus matrix, rotate, flip (including whole-image Undo/Redo), restore/latest-source and real RUNNING duplication. **Selected ASan/UBSan9 PASS**: six geometry cases plus matrix, rotate and flip;20 instrumented+53 RTTI-only units, remaining dependencies uninstrumented, LSan off. Each new case checks4096 original lower-source bytes unchanged, source/model bounds, actual native buffer behavior, scheduling completion and independent red/blue endpoint pixels. New no-ops advance2→3; changed cases retain proper output coordinates and colors.

## Provenance and limits
Base20b2204b11df079c306a680b41ac623f3be75c76 plus explicit previously sealed Filter checkpoints and final Options enum/Meson integration. `enum-baseline-overlay.json` includes all seven providers including generated pdb/enums.pl, two enum fixtures and two isolated Meson files. Every declared baseline hash was checked before the test-only red overlay. Green127-file archive, source seals and owned hashes bind the actual tested successor; no drift through compilation/run. Eight held providers and Clone remain committed baseline bytes. No default Edge execution, held diagnostic, arbitrary or extreme geometry, or Clone geometry claim.

The flip fixture now expects the five parent-delegating wrappers rather than inherited slots; separate behavioral cases prove delegation, while historical receipts stay immutable. This is model/API evidence, not a fresh rendered UI result or whole-HEAD aggregate. Original15.005 dependencies and criterion remain unchanged; bounded cases do not justify closing the entire prerequisite chain.
