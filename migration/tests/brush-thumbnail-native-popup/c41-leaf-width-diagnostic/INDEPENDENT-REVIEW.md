# C41 leaf-width diagnostic: independent review

Review outcome: no implementation blocker found in the frozen local candidate identified below. Suitable as a diagnostic-only candidate for separate native build/execution verification, not a production repair, acceptance pass, publication authorization, or proof that the primary readiness/leak failures are resolved.

Review was read-only for the repository. This report is outside the repository. No native GIMP/GTK GUI, ptrace/debugger, babl diagnostic, GitHub write, or publication was performed. Read tasks.md, the Run57 investigation and independent review, the existing diagnostic contract, and the changed implementation/integration. C40's support-pin correction remains a separate concern.

## Findings resolved during review

- Kept three fixed, typed original-function pointers and thunks. Existing and late copied slots are treated by pointer identity, not the receiver's dynamic type. Unknown overrides stay untouched and explicitly partial.
- Avoided any instrumentation when a plugin-backed descendant is known at setup. A late dynamic class is identified before its class slots are read or written; restoration is declared incomplete and the diagnostic is rejected. Static installed slots are still restored first.
- Added a distinct numeric GimpImageWindow host class role rather than folding it into all other hosts. It is a class grouping, not unique-instance attribution.
- Tightened parser consistency between enabled/coverage/restore flags, family availability, install counts, dynamic observations, and metrics. Invalid combinations cannot be accepted as full capture.
- Added existing and late subclasses for all three families, especially pixbuf renderers, to the real-GObject synthetic proof. This covers the copied-slot mechanism relevant to GimpCellRendererButton and GimpCellRendererPopup, without pretending these synthetic classes are real GTK ABI tests.

## Dispatch and lifecycle assessment

leaf.inc modifies only GtkImage.get_preferred_width, GtkCellRendererText.get_preferred_width, and GtkCellRendererPixbuf.get_size slots currently equal to their saved base original. Every typed thunk calls that exact original once with unchanged arguments. Neither dynamic receiver-type lookup nor a public sizing API chooses or invokes the original. Unexpected overrides, including parent-chain implementations, are left in place. A parent-chain call may contribute a measured base portion, but does not measure the entire overriding function.

No height, height-for-width, or baseline vfunc is assigned. GTK's baseline dispatch compares height-function pointer identities; the implementation avoids that hazard rather than relying on a forwarding wrapper being behaviorally equivalent.

Setup uses already registered type names and class peeks, not class initialization or class references. Breadth-first audits visit at most128 types per family. The original functions belong to static base classes. Directly installed types are restored before the finish traversal, including if the traversal later clips. Late static descendants containing an exact thunk copy are restored to the corresponding base original. Unknown replacement slots are not overwritten. Bounds, dynamic classes, or conflicts reject the capture. This is acceptable for the isolated child that the existing controller terminates; it is not a general-purpose instrumentation system for arbitrary long-lived GTK processes.

Privately cached function copies are not discoverable or restored. A cached thunk still forwards safely after observation becomes inactive because the saved original is retained in static storage; this does not justify a claim that every process-wide function reference was restored. Main-thread-only GTK use and one-shot observation remain required.

## Callback and owner safety

Callbacks use fixed counters, timestamps, a bounded parent walk, and a numeric type check. There is no callback allocation, logging, property/icon lookup, sizing redispatch, widget retention, or post-original widget access. The host is the GtkImage itself or the renderer's supplied widget. Target and harness identity pointers are weak and cleared by finalization. Reparenting changes attribution at the next entry without consulting a stale parent after the original returns.

Roles0..4 mean unknown, target, harness, GimpImageWindow hosts, and other. Role3 includes every matching image-window instance at call time, including an instance created after the phase snapshot. A phase trace with one original image window alone does not establish that all role3 time belongs uniquely to that window.

Per-bucket outer totals can overlap across families/owners. The separate global union accounts for outermost spans across all three families without adding nested spans. All durations remain debugger-inflated wall time, not primary timing or exclusive workload attribution.

## Source review and coverage

Reviewed actual upstream tag3.24.49 files obtained from the official GNOME repository:

- [gtkimage.c](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkimage.c): lines1917–1925 call the CSS gadget; content sizing at1700–1740 calls the icon helper. There is no same-widget public width redispatch in that original.
- [gtkcellrenderertext.c](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkcellrenderertext.c): lines2103–2186 create and measure the layout and font metrics, without redispatching the width vfunc.
- [gtkcellrendererpixbuf.c](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkcellrendererpixbuf.c): lines477–552 measure the helper and assign size/offset outputs directly. The render path at572 calls the static get_size implementation directly, bypassing the instrumented class slot.
- [gtksizerequest.c](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtksizerequest.c): lines87–110 establish the height-identity baseline hazard; lines138–164 perform resize/style work before the preferred-width vfunc.

The reviewed upstream source does not establish byte identity with Debian3.24.49-3 patches or the hosted binary. That source/native verification gap remains explicit.

Coverage excludes construction/show/drain before the first geometry wait, height requests, preceding style/resize work, direct static calls, arbitrary overrides, and uninstrumented classes. Unknown overrides alone may yield captured partial records with coverage_complete:false. A full leaf capture still describes only this declared scope. Low totals cannot rule out CSS, icon loading, or other layout cost.

GType child enumeration allocates a sibling list outside the timer. GTK/GLib exposes no bounded version used here, so only visits, fixed storage, output, and callback work are bounded; the sibling allocation is not claimed bounded.

## Integration and validation

The generator adds leaf setup before the original first-wait timer and leaf finish before owner finish/done. The isolated Meson recipe includes the new input/output. Private identity closure covers the source, parser, and generated leaf include. The collector requires valid owner, phase, and leaf streams for captured status. Its existing exact-child cleanup sends SIGKILL using the verified pidfd before debugger exit and does not resume after completed owner records. Failed restoration is not allowed to become acceptance evidence.

Independent final local runs after the leaf worker froze:

- test_leaf.py:10 tests PASS. Compiles the exact leaf include with C11/Wall/Wextra/Werror and real GObject, using explicitly synthetic method-slot declarations and a fake monotonic clock.
- test_leaf_records.py:13 tests PASS.
- test_owner.py:13 tests PASS.
- test_readiness_owner_integration.py:52 tests PASS, including the nested suites.
- git diff --check:PASS.

The synthetic cases cover all-family existing/late copied slots, one-time exact forwarding and sentinels/NULL outputs, same-family and cross-family nesting, original/late overrides with parent calls, no initialization of dormant classes, weak-root destruction, one-shot cleanup, unchanged height pointers, setup/late traversal bounds, conflicting mutation, real GTypePlugin early/late dynamic classes, counter/owner bounds, backward clock, unmatched state, missing owner type, and producer-to-strict-parser handling.

All26 protected common files independently matched both their pinned SHA256 values and C40 bytes. Primary Brush/Transform includes, compact TU, tests.c, app/tests/meson.build, pins.json, sample_case14.py, run.sh, and original owner/phase probes matched C40 exactly. The original3-second predicate, stable-frame condition, leak policy,96-frame collector, and diagnostic-only status remain unchanged. This is focused review evidence, not a full aggregate or hosted/native pass.

## Reviewed code identities

- `tools/brush-ci/readiness-owner/leaf.inc`: `bc1754ff6dabe264157a7d5a65d36ee60cd6cc06f3c4790af70129c037666055`
- `tools/brush-ci/readiness-owner/synthetic-leaf-prefix.c`: `10d419a73225c9ab4d6d8b654e433de1e7725001ec7dbf9e62f4cd2e3ae5d852`
- `tools/brush-ci/readiness-owner/synthetic-leaf-suffix.c`: `bccf5ca5ca7d97d25ef4006c9ad02a1bd97323dc99ab0d57446aa8b8e74de75b`
- `tools/brush-ci/readiness-owner/test_leaf.py`: `7f0d6a27e5eb50f784659cbc20a1afb17bc6d9401619e99d1420e819c2d145e9`
- `tools/brush-ci/readiness-owner/leaf_records.py`: `0a74293b6630e12a05d0367758d8d231f1be9e7d7035a1ff2ca14af204f16626`
- `tools/brush-ci/readiness-owner/test_leaf_records.py`: `b429f96924e6b0caa8bebaff08f26e839b5ec01c9e68d03e155234c63f109891`
- `tools/brush-ci/readiness-owner/collect.py`: `d37fe4d7c97c168035168ed4db8f6315f0badeaf7c6f0c89b9bf0bc67f39a0ff`
- `tools/brush-ci/readiness-owner/generate.py`: `3c5bbc87d1e25cae937f24f456f8f5db9ddea0a4128d5b4a2d6a0570c063e51f`
- `tools/brush-ci/readiness-owner/identity.py`: `8205159eafce65960ec1a8adf4a6fd37a2f09b34853eae551f30ca875f7f8005`
- `tools/brush-ci/readiness-owner/meson.append`: `971a885734a1bcdf1cfd5f3dbe5e4a0282f2e206648812dbc1da06df4fc53e09`
