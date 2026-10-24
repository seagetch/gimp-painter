# Filter own-center rotate invalidation — original 15.005 checkpoint

Pinned old `afa43fae` delegates rotation to the native parent and then resets derived output unconditionally (see old-source-reference.json and excerpt). The port inherited rotation and observed only geometry/format changes: an ordinary 16×16 own-center 180° rotation changed its cache without scheduling recomputation.

## Causal result
Red native case observed one buffer notification, unchanged bounds/source, an immediate red-to-blue cache change, but generation 2→2, and failed the required generation advance. The narrow override retains owner/image, delegates to the native parent without holding a Filter implementation borrow, rechecks the binding epoch and invalidates. Green observes generation 2→3 and recomputes the original red lower-source result. Flip remains invalidation-only; scale, resize and matrix-transform slots are unchanged.

Green native **4** cases pass: rotate, direct/image flip with Undo/Redo, restore/latest lower source, and actual RUNNING duplication. Selected ASan/UBSan **2** cases pass: rotate and flip; **20 instrumented + 53 RTTI-only** units, remaining dependencies uninstrumented, LSan disabled. This is bounded model/API evidence, not a new rendered UI result or full geometry acceptance.

## Provenance
Clean source base `20b2204b11df079c306a680b41ac623f3be75c76`, explicit Options mode provider overlays, committed running-duplicate fixture, and the four owned rotate provider/test files in owned-source-sha256.json. Source seals and archives bind the actual inputs; green archive contains 118 files. All eight earlier held providers plus the Clone provider retain committed base bytes. No held diagnostic or old Edge runtime was executed. The revised flip fixture now asserts delegated rotate rather than inherited rotate, preserving the other three inherited transform slots; historical receipts are not rewritten.

An Options enum setup command regenerated committed enum source before green input capture: source bytes equal the base, while generated stamp timestamp changed; previous stamp/tmp hashes were not captured. `green/generator-setup-event.json` records that event. No source change occurred during the sealed green run. Source/archive receipts do not claim generated stamp invariance.

Original 15.005 criterion and dependencies remain unchanged and open; matrix-transform and broader geometry coverage are not established here.
