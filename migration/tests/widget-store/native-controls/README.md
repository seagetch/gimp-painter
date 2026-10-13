# 08.017 bounded native controls and DnD evidence

Declared public base: `6bad61c635370a5649227a9623caf310628fb1bb`. This is a second, bounded widget-store subset,
following the already published editor-layout/store controls and zero-count
procedure-configuration prerequisite. No work ledger is changed. Original tasks
remain 140/661; 08.017 and every one of its 93 original source duties remain TODO.
No 29.* feature obligation is accepted or rerouted.

## Minimal production correction

Restore the exact original defensive preview check from source duty
`01.002/001526`: test `preview != NULL` before dereferencing its renderer in
`gimp_container_box_get_preview`. This guard and its blank line are the only
production change. The other four source files add test-only integration.

The failure test uses actual GimpDocked dispatch, a valid native layer container
and the real preview factory. Its unsupported context property makes the factory
return NULL. The unguarded baseline terminates with SIGSEGV (-11); the standard
ASan/UBSan baseline aborts (-6) at the exact null GimpView member access at
`app/widgets/gimpcontainerbox.c:268:22`. The guard returns safely and the test
checks the precise deliberate diagnostics. No mock, interposition or global
fatal-mask relaxation is used. `baseline-red.json` retains only portable TAP,
identity hashes and that exact source-specific diagnostic, never raw logs.

## Positive controls, distinct from the regression

Eight native baseline controls already pass without a production correction:
normal preview creation/property notification/finalization; enum/property
controls through compact presentation; enum/integer helpers; generic same-parent
DnD order; cross-parent last-owner retention; self/ancestor rejection; image-aware
group movement and Undo; and cross-image COPY/Undo with bounded identity, pixel,
dimension and opacity checks. These results are preserved as positive controls,
not presented as eight reproduced product bugs.

Actual native widget finalization is observed through qdata destruction. Native
weak-notify callback-release counters have their separate, narrower meaning.
The DnD last-owner control observes the ownership gap after source removal,
checking one retained reference, destination ownership and eventual finalization.
Image-aware tests use the native item/layer-tree overrides. Cross-image behavior
is independent provider integration, not a new duty inferred from generic hunks.

## Verification and boundaries

Both final native and standard ASan/UBSan runs passed the focused guard case and
all five selected registrations: layer UI 63, canvas 49, compact options 18,
foundation 70 and MyPaint editor 14 (214 cases per mode).

Standard Meson ASan/UBSan retains RTTI/vptr; leak detection is disabled and
third-party dependencies are not instrumented. `sanitizer-instrumentation.json`
checks the relevant production and test translation units and standard completed
build markers. Native/ASan reports seal the five candidate source files plus the
runner's other selected sources, and exact test executables; the same final
source/binary identities must occur in the focused and expanded runs. Additional
reviewed-provider hashes in `source-hashes.json` are preparation/freeze checks,
not claimed to be independent before/after runtime seals.

This is five selected registrations on a virtual GTK display, not the full
aggregate, physical drag/tablet input or cross-platform acceptance. Current
helper/consumer counterparts do not prove every old orientation API or C caller.
The old stock-property orientation helper's body did not implement the vertical
behavior suggested by its name; no such new policy is introduced here. DnD
coverage is bounded to singleton source lists and selected copy properties.

`source-contract.json` preserves 41 individual mapped original duties (33
preview/helper rows and eight generic DnD rows), their exact original payloads,
case links and remaining limits. `all-original-duties.json` keeps all 93 original
rows, source-specific responsibilities and sibling obligations visible and open.
Whitespace-only rows are not accepted by runtime tests. These evidence artifacts
neither erase absent legacy APIs nor substitute broad smoke tests for each duty.

## Publication boundary

The publication manifest supplies exact Git blob SHA-1, SHA-256, byte count and
mode for every allowed file. The patch uses `a/` and `b/` repository-root paths,
with `/dev/null` only for new files. The three existing source preimages come
from the parent's frozen current-public baseline; both new `.inc` files have no
preimage. The root publication owner must verify those preimages and absent new
paths against the exact declared public commit before publishing. The freezer
has no remote or publication action, and no raw results/environment files enter
the package.
