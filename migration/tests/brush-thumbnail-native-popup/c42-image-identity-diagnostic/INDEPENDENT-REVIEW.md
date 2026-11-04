# C42 image identity diagnostic: independent review

## Outcome and scope

No remaining implementation blocker found in the reviewed local candidate. Suitable for the parent's final local aggregate and subsequent separately authorized native build/execution verification. This is diagnostic-only review, not a production repair, primary performance result, leak clearance, acceptance pass, or publication authorization.

Baseline: frozen C41 tree `4983054ddd202bd50f44bf6ea07f9281a3911a6a`. Reviewed `tasks.md`, the readiness-owner contract, C41 independent review, C42 producer and real-GObject fixtures, strict parser, collector, and overlay integration. No implementation files were edited by this reviewer. No GIMP/GTK GUI, ptrace/debugger, babl diagnostic, hosted job, GitHub write, or publication was performed.

## Findings resolved during review

1. Replaced borrowed pending traversal pointers with weak queue slots. A virtual `forall` may finalize or reparent a previously queued sibling. Cleared entries are skipped, parent/root changes are flagged, and all live queue weak registrations are removed before timed observation. Synthetic finalization and reparenting cases now fail closed without stale dereferences.
2. Moved scale-factor lookup after the bounded parent walk. GTK's unrealized scale-factor path walks to the toplevel internally. Failed bounded traversal now returns canonical invalid metadata without invoking that getter. An unrealized cyclic synthetic hierarchy verifies the ordering.
3. Added cross-stream image/leaf entry and exit count checks. Complete but inconsistent streams become inconclusive; the existing child cleanup still runs without another inferior continue.
4. Tightened producer/parser invariants: snapshot host must match its ordinal origin; one-call duration equals its maximum; single-call first/last metadata agree; observed metadata differences require corresponding change counts; nonempty safe records have an outer entry; valid host/ancestor values cannot be zero; and non-icon-size storage requires the -1 logical-size sentinel.
5. Added a mixed known/unknown nested-call case alongside same-image and cross-image nesting. Inclusive overlap and unknown-call semantics are explicit.

## Identity, traversal, and lifetime

The snapshot visits existing GtkImage objects below existing GimpImageWindow roots using `gtk_container_forall`, including internal children. Duplicate callback exposure is deduplicated. Limits are 16 matching roots, 128 examined toplevels, 4096 stored nodes, 256 image identities, depth64 traversal, and depth128 ancestry. Root ordinals and image ordinals are weak, run-local identities; cleared slots are never reused. Late images, replacement objects, images outside the snapshot roots, and late matching roots remain untracked or separately classified, not silently assigned an old ordinal.

`forall` has no early-break API: stored/processed visits and each callback's work are bounded, but remaining sibling callbacks can still occur. The toplevel-list allocation is outside timing and is not claimed allocation-bounded. Setup invokes no event-loop drain, sizing query, icon lookup, rendering, or GUI prewarming. Weak registrations and destroy handlers add diagnostic setup overhead.

Destroy-signal observation and finalization are separate facts. A destroyed object can remain alive through another reference. The call token contains only ordinal/flags/time, never a widget pointer. After forwarding, bookkeeping uses static ordinal storage only, including when the original finalized the widget. Finish disconnects live destroy handlers and removes live weak registrations; repeated begin/finish is inert. The address-reuse proof combines real weak finalization with a synthetic reused argument address and explicitly does not claim allocator reuse.

## Getters and timing interpretation

Reviewed upstream GTK 3.24.49 primary sources:

- [gtkimage.c](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkimage.c): storage-specific logical-size getters receive NULL name/object outputs; pixel size and storage access do not request an image load.
- [gtkiconhelper.c](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkiconhelper.c): logical/pixel size are stored-field reads and storage uses the existing definition.
- [gtkwidget.c](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkwidget.c): state, visibility, realization, mapping, child visibility, and parent getters read existing state. Scale lookup can inspect root/monitor state and does not load an image. The probe bounds ancestry before invoking it.
- [gtkcontainer.c](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkcontainer.c): `forall` dispatch includes internal children; special containers may expose descendants beyond an ordinary direct-child enumeration.

This does not establish Debian patch or hosted-binary byte identity. The synthetic getter declarations are not the GTK ABI.

Snapshot metadata is distinct from first/last call-entry metadata. Fixed semantic ancestor classes and host ordinals describe only sampled ancestry. First means first observed entry during this probe, not first-ever, cold, or uncached. First/repeat durations are inclusive, so nested spans overlap and must not be added as disjoint work. Unknown aggregates and their first/last metadata can combine multiple objects; they are neither unique-image counts nor proof of newly constructed widgets. Debugger pauses and instrumentation overhead remain in wall durations. Leaf totals also include image-observer work, so image/leaf duration equality is neither asserted nor required.

The existing typed original is still called exactly once with unchanged arguments. Removing only the two image-observer lines from `leaf.inc` reproduces the exact reviewed C41 SHA256. All previous family-slot auditing, forwarding, late copied-slot restoration, coverage exclusions, and rejected-restore child termination remain unchanged. Identity coverage is conditional on these observed leaf calls; it does not expand C41 coverage to arbitrary overrides, height queries, pre-vfunc style work, or direct static calls.

## Protocol and integration

Only bounded numeric IMAGE_DIAG rows are exported. Signed -1 is restricted to size sentinels. Ordering, row counts, ranges, canonical missing metadata, totals, first/repeat relationships, nesting, mutation flags, and completion are checked. Missing semantic types permit an explicitly partial capture; other producer faults reject capture. Output bounds fit the existing report budget. Arbitrary strings, names, paths, addresses, console spoofing, malformed target chunks, mixed transports, and incomplete streams are rejected or excluded.

The generator adds the snapshot before the original timer in the separate diagnostic copy; its source/parser/generated include enter private identity closure. The original primary deadline, stable-frame assertions, leak settings, owner/phase probes, workflow, and primary test sources remain unchanged. The collector retains exact-child lifecycle and non-acceptance labeling.

## Independent verification on final code

- `test_image.py`: 12 tests PASS, covering 27 synthetic process modes. Exact producer compiled with C11, Wall/Wextra/Werror and real GObject/GLib; producer output is also fed to the strict parser.
- `test_image_records.py`: 17 tests PASS.
- `test_leaf.py`: 10 tests PASS.
- `test_leaf_records.py`: 13 tests PASS.
- `test_owner.py`: 13 tests PASS.
- `test_readiness_owner_integration.py`: 55 tests PASS, including nested suites. These counts are not additive distinct native cases.
- `git diff --check` against frozen C41: PASS.
- All26 protected common files independently match both pinned SHA256 and frozen C41 bytes. Primary Brush/Transform/compact/tests/Meson, pins, original sample controller, runner, original owner/phase probes, leaf parser, and workflow also match C41.

These are focused normal-execution source/synthetic checks, not ASan/LSan, full native compilation, whole-project aggregate, or hosted readiness acceptance. The actual slow widgets and a production fix remain unproven. Existing same-size guards do not justify adding speculative guards, and possible Dockbook tab recreation is still a hypothesis.

## Reviewed implementation identities

- `tools/brush-ci/readiness-owner/image.inc`: `fd79b1fa80ad0be75d6ef8d9bbdcd251edfeb01dc497f8cb9576face93ee8ede`
- `tools/brush-ci/readiness-owner/synthetic-image-prefix.c`: `c024f73f57c5a86e9fd01990490848e4aba5e557de049207dacfbe6181b76426`
- `tools/brush-ci/readiness-owner/synthetic-image-suffix.c`: `0b4177ceb7fd331048d389199c45e39c561f8c895d80180fee1f897fbb32ba3e`
- `tools/brush-ci/readiness-owner/test_image.py`: `e3b0fedea991cf5de19acebf436dcf3e61e12fb6e36ef3605fca09e458ebd572`
- `tools/brush-ci/readiness-owner/image_records.py`: `3600b473eec13f16dcae7235f6f3d9e38df31790def83723d7b5de36caaefbef`
- `tools/brush-ci/readiness-owner/test_image_records.py`: `f2d0f5d04b2afb82f05088bcc8ab0cb6e59461a891a8fd50a5e0f14c0d7a9946`
- `tools/brush-ci/readiness-owner/leaf.inc`: `451188e12334aff2a5c4db533cf775164383b88cec355d96006a5725607b79b8`
- `tools/brush-ci/readiness-owner/collect.py`: `5e0b71a98cc232870035285d038b15b849f740972775f5456513fa55a7ef2d5f`
- `tools/brush-ci/readiness-owner/generate.py`: `463ea36b0d3e5eed8634587e5430fb3a801c1ecf86a53a3321efa10a25942455`
- `tools/brush-ci/readiness-owner/identity.py`: `8fff62af2b098ee1cf7c7abc71b0a8ce0141896416e8057d82bc14321d15eaf2`
- `tools/brush-ci/readiness-owner/meson.append`: `cde30a8f860c432d52628f5526dfe9628e8ae79108bf2a2a517cd32740d9419c`
