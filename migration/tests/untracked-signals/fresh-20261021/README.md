# 07.008 untracked signal owner routes — acceptance assembly

This package accounts for all 60 original `connect_noret`/unretained connection
sites without treating historical visual or API parity as the signal-lifetime
criterion. `route-review.json` preserves every original source-map row and its
remaining acceptance text. Three pinned callbacks are empty, comment-only, or
literally disabled in the original source. The other 57 duties map to actual
native replacement owner routes and 45 distinct registered test identifiers.

All 60 duties now have bounded acceptance: direct editor controls passed normal
3/3 and ASan/UBSan/float-cast-overflow 3/3 with the original 120-second budget.
`validate-evidence.py` checks the independent runtime receipts;
`validate-routes.py` checks every original mapping and actual TAP case result.
Seven negative accounting controls reject lost duties and false acceptance.
The unrelated oncanvas tool-group reset/deserialization failure discovered by
the separate perspective UI suite remains open; this is not full UI acceptance.

Evidence has distinct immutable checkpoints, not an invented single full-app
sanitizer run:

* `filter/`: four native dependency callback lifetime routes, normal and focused
  sanitizers; owner closure, replacement and reentry.
* `popup/`: 34 popup, 57 dropdown and 29 selector cases, normal and bounded
  sanitizers with original watchdogs.
* `editor/`: original 25 and page-memory 3 sanitizer cases. The exact earlier
  harness is preserved, rather than claiming it was rerun after later test-only
  extensions. Normal editor/page receipts are also in the committed
  `migration/tests/editor-page-memory` package.
* `layer-canvas/`: earlier full 70/107 normal and focused sanitizer checkpoint.
  The subsequent initializer fix is separately verified at commit
  `91a4a58881c2575b960f9c232a7e1bc99204c253` by the immutable
  `migration/tests/vfunc-exception-containment/layer-tiles-initializer` package:
  initializer 24, layer 70 and canvas 107 in normal and bounded sanitizers.
* The later dispatch catch-all fix is separately verified at commit
  `d8ac83c5a544e50100b880736f37865809bed014`: four actual GTK signal faults,
  layer 70 and canvas 107 pass normal and bounded sanitizers in
  `migration/tests/vfunc-exception-containment/layer-signal-dispatch`.
* `migration/tests/application-lifecycle-signals`: startup 7, HTTP 18 normal,
  HTTP 18 and HTTP/GTK 6 sanitizer owner-teardown checks.
* `application-exit/`: actual installed application, authenticated 1080×1920
  Xvfb, enabled oncanvas controls, HTTP response 200, accepted native exit hook,
  exit status 0 and closed listener. Its inspected PNG is actual rendered UI.

Private instrumentation is source-enumerated. Dependencies are not all
instrumented and LeakSanitizer is disabled; no whole-application leak or platform
claim follows. Runtime diagnostics are preserved. Inherited environment dumps,
private profiles and ephemeral HTTP tokens are excluded. Failed fixtures and
setup attempts do not count as passing production regressions.
