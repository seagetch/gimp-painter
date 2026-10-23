# Original 13.010 compatibility-mode persistence checkpoint

`/painter-layer-modes/modes_xcf_roundtrip` constructs 27 native scenes:
all nine byte-compatibility modes (historical IDs 0,3,23–29) in flat,
isolated Painter-Normal group, and pass-through group layouts. Each scene is
saved and reopened **twice**, for 54 native XCF save/reopen cycles. Every cycle
checks the source layer's exact mode enum, typed capsule `legacy-mode` raw ID,
opacity .5, applied mask's 64 bytes (128), group mode/hierarchy, and all64 RGBA
projection pixels against the genuine old application reference. Existing
normal/precision/group/direct-operation cases run in the same test target.

This uses newly written modern XCF with typed Painter capsules; it is not an
old application runtime, a new historical-file capture or a claim that another
application understands the Painter extensions. The existing reader's old-file
classification and broad XCF receipts remain separate evidence. No Edge/string
runtime or skipped legacy fixtures are executed.

Focused sanitizer instrumentation is deliberately C-only: XCF registration,
load/save/read/write/seek, the Painter byte operation, cached mode dispatch,
GimpGroupLayer and the C test harness (including this new test include).
**C++ capsule/provenance/bridge code and remaining dependencies are not
instrumented.** This report is not equivalent to the separate broad XCF
sanitizer receipt. LSan is disabled. Normal and focused reports omit inherited
process environment, and the manifest records source/include hashes and exact
case counts. This is a file/numerical test, not a UI screenshot claim.

Original predecessor 13.009 remains open; this checkpoint does not redefine
13.010's acceptance, certify arbitrary mask/opacity/group combinations, or
close precision-extension/artwork requirements.
