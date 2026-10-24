# Immutable native preset descriptor catalog (bounded source migration)

See `migration/contracts/preset-native-catalog.md` for original requirement,
startup boundary, metadata-only process catalog and per-owner factory pairing.
The two original parent tasks remain open for explicitly unmatched old virtual/
binary ABI and lifecycle details; this is not blanket legacy GUI parity.

Final native tests: catalog26 + actual GUI5 + original GUI-entry6 =37.
Selected ASan/UBSan/float-cast-overflow instrumentation:11 C/C++ units, same37.
C++ vptr and leak detection are disabled; dependencies/whole app are not instrumented.
Two private causal implementation mutants reject false-success replacement and
missing preparation generation validation. The absent public entry compile red
is separate, not a runtime oracle. Existing normal regressions additionally cover
layer presets12, UI18, lifecycle14 and public C/C++ header compilation/routes.

The original shallow-array metadata/mutation test is preserved unchanged. The
first candidate's element clear callback was a real invalid-free regression;
its receipt remains in historical attempts, not treated as a harness failure.
Final legacy arrays borrow metadata through the catalog epoch; only exposed
immutable descriptors are deduplicated/retained and released at last owner close.
The stress case accounts descriptor bytes/counts and32 actual descriptor frees.
New owning read-only snapshots survive close, with separate last-reference tests.
No global owner map, forever-interned strings, or unbounded cross-epoch registry.

Five model coexistence groups share only the native global unit service through
an explicit test guardian; catalog/factory/Gimp ownership is not mocked. The
original unit singleton assertion and wrapper trace setup correction are retained.
Other single-owner cases perform genuine initialize and selected alternate path
JSON load/save/reopen. Guardian coexistence is not a full multi-app startup claim.

The portrait PNG is actual rendered native Preferences navigation to a replacement
folder page bound to existing tool-preset paths, with oncanvas UI enabled/visible.
Authenticated Xvfb is1080×1920×24. Tests drive the native tree cursor and assert
actual dialog/action metadata; no physical input, browser/help document or image
editing claim is made. Stable page identity remains separate from selected Help.

`source-manifest.json` records complete actual source bytes, including named prior
Filter/RedEye/Exchange overlays; it is not an all-HEAD claim. The tested-input
archive owns only the named provider/header/fixture scope. Ten held provider
snapshots are separately pinned; their paused diagnostics were not rerun.
Actual linked native input hashes are unchanged across sanitizer construction/run.
Historical receipts remain historical; no old metadata/helper proof was resealed.

Persistence audit: frozen committed families validate before the source gate
rejects the new `app/core/gimp.c` digest. It needs explicit exact source-successor
admission after commit, not field-table rewriting. Gimp property/config-loading
bodies are unchanged; config740 assertions pass. Dynamic factory property names
are validated against existing read/write directory-list schema; no new saved
field is added or concealed. The mixed dirty-worktree diagnostic is not the final
gate and is not described as a catalog regression.

Run `python3 migration/tests/preset-catalog/verify.py --check-owned-source` and
`python3 migration/tests/preset-catalog/test_verifier.py`. The latter changes
artifact hashes along with five semantic corruptions, so it tests assertions
rather than only checksum rejection. Runtime receipt reproduction uses the
recorded Meson targets/build commands; never compile paused worktree providers.

Publication cleanup successor: the old 34-case package is retained verbatim in
`historical-34-package.tar.gz`, not resealed as final evidence. A causal red
proved prior autosave was lost on rejected Preferences publication. Three new
cases preserve both prior states, release both config copies and the widget
exactly once, remove only the two owned theme observers, safely retry, and
reject a genuinely closed owner. Config data is attached before releasing the
lease; retained original config endpoints allow cleanup without reading Gimp
thereafter. Native borrowed-application lifetime is unchanged. Default metadata
uses N_ and the source is included in POTFILES; no installed translation or Help
document is claimed.
