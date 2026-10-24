# Terminal geometry conflicts: bounded 09.011 successor

Supported public Tool resize/translate cancels the stroke at the native Undo-group dirty signal **before** changing geometry. Direct post-mutation buffer, extent, offset, dimensions, or companion-association changes are different: they cannot be treated as native stroke-owned expansion. The historical surface and this controller clip rather than expand.

The predecessor probes reproduce first-cancel update callbacks with restored evaluator but externally changed pixels/geometry, wrong final pixels after a direct offset, and overwritten independent replacement pixels. They are diagnostic traces, not successful acceptance tests. Installed GEGL is 0.4.62; exact library/module hashes are recorded. No claim depends on upstream-master implementation.

The controller now rejects such conflicts before its commit/cancel writes. It retains its original baseline and evaluator/Resource checkpoint until explicit owner disposal, leaves current pixels/geometry/evaluator/Resource/coordinates/history unchanged, marks itself terminal, and immediately finishes native bookkeeping with `push_undo=FALSE`. This thaws once and removes pending-paint handling without leaving Save blocked. Repeated stop/configure/input requests reject. The typed C++ exception maps to the existing C error domain/EXCEPTION code with an exact stable untranslated message.

The Tool reports that exact error, retires only the same failed session under binding-generation checks, and creates a new session only on the next explicit press. Hover cannot silently restart it. Ordinary public resize/translate and non-conflict error paths are unchanged. No global signal suppression, native geometry rewrite, duplicate history entry, raw Stroke pointer ABI, general OOM atomicity, or recovery/merging of arbitrary external edits is claimed.

## Evidence
- Native model14: layer/mask grow/shrink/offset/replacement/same-pointer extent; companion remove/replace; exact bytes/state/coordinates, repeated errors, retained lifetime, one thaw, Save eligibility and redo preservation.
- Actual Tool8: public grow/shrink/translate, direct offset recovery, independent replacement, next-input replacement recovery.
- Unchanged Surface24, paired18, Tool27: **91 native cases total**.
- Selected **23-unit ASan/UBSan**, model14 + Tool8. Native PaintCore/Drawable/Item/Viewable/Layer bookkeeping is instrumented. No vptr/LSan/dependency-wide instrumentation claim.
- Six private-object causal mutants. The first no-retirement runner expected a later assertion; actual earlier native-control failure is preserved and the final check requires that precise failure.
- C++14/no-RTTI first/double header inclusion; current reviewed source policy PASS2513/718, four preexisting dormant findings. The stale private-policy failure is preserved.
- Nine held providers sealed. Instrumented `gimpviewable.c` is explicitly the fixed committed baseline 7c60612b37ee9f38f4c9293a45e9cf824d5a2c24, not its four-line held-preview worktree candidate.

The two PNGs are actual1040×1800 windows on authenticated1080×1920 Xvfb, oncanvas visibility asserted, after ordinary image flush. They show retained red pixels following rejected direct offset and an extended stroke after the next synthetic native Tool gesture. **The error text is not visible**; no caption claims it is. These are native API-driven Tool events, not physical pointer automation. Canvas viewport is unchanged; oncanvas panels may reflow with active-tool state.

`validate.py --check-source` validates current owned source and the explicitly fixed committed baseline dependency. Immutable predecessor receipts replay without relabeling their original failures. Original09.011/mypaint-undo-stroke remains open for parent acceptance review; this package does not erase unsupported external-conflict recovery or old raw-pointer ABI differences.

## Final setup and retirement successor
Geometry and companion identity are captured before ordinary resource/raster callbacks and rechecked before native start. An actual standard Brush begin-use replacement reproduced the old acceptance bug. Pre-start rejection does not start/freeze/thaw native paint, preserves external pixels/evaluator, and permits safe explicit-batch retry followed by exact Undo. The earlier interactive retry fixture incorrectly assumed one Undo would cover every automatic split; that failed receipt is retained separately, not treated as a product defect.

Tool retirement extracts only the same failed session under BindingStore before clearing input, holds stopping through clear/dispose, and restores it only under the same generation. Actual GUI message-report nested press and disposal cases pass. Instrumentation additionally covers native Brush and GimpResources. The complete earlier frozen package is retained and its artifact seal replayed.

Conflict detection is identity/geometry validation at transaction boundaries, not a general pixel-write journal: arbitrary writes preserving identity/geometry, or transient changes fully reversed between checks, are not claimed detectable. Normal public resize/translate still use early cancellation.

A test-only attached native LayerMask subclass returning a null companion buffer proves graceful pre-start rejection before any extent/format access, no freeze/pixel/evaluator/history mutation, and unblocked Save. Removing this guard causes the exact GEGL extent precondition failure. The preceding 13+8 package remains sealed separately.
