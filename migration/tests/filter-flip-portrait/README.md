# Filter image flip: actual oncanvas portrait rendering

Original15.005 supporting checkpoint, not closure of its15.004 prerequisite or
all geometry requirements. Production fix is11ea92ef08. One actual GUI test
passed on committed **a84cd0b051e7b648ec9519ede82ae9541c44ea1c** production plus
only the two owned test overlays listed in source-before.json. This is not a
claim that the separate aggregate passed (it had unrelated paired-history
failures), nor that evolving main worktree candidates were built.

Authenticated loopback Xvfb1080×1920×24 accepted the proper authority and rejected
an empty authority. The fixture enables painter-canvas-ui through the existing
GUI harness and asserts the real canvas UI is visible in both operation states.
Both PNGs are actual root-window captures and were visually inspected, not
mockups or image edits. They show the before/after **direct image-flip API** on
the same fixed build, not a pointer gesture, menu activation, or old/new binaries.

The ordinary400×300 red-left/blue-right layer is really flipped. Gaussian Filter
bounds stay140,70/160×160; two exact independent solid-region samples per state
prove red/blue become blue/red in its newly derived buffer. The visible selected
Filter retains its rectangle and blurred color boundary after the source flips.
The rendered viewport/layout can settle differently between captures; asserted
bounds are native image coordinates, not screenshot pixel coordinates.

- `filter-flip-before-oncanvas-portrait.png`: red|blue before API action.
- `filter-flip-after-oncanvas-portrait.png`: blue|red after API action.
- `native-gui.json`: exact selected command,1 passing TAP case, stdout/stderr.
- `source-before.json`, `source-after.json`: selected source/provider hashes;
  no selected input changed while running. Eight previously held providers
  checked against committed base before execution. The additional Clone hold
  was announced while this run was in progress; its unchanged committed hash
  was verified afterward in clone-held-provider-postcheck.json. No Clone
  geometry/resize diagnostic was performed.

This checkpoint does not exercise old Edge/string/watercolor runtimes or any
paused provider lifetime diagnostic. Known headless desktop icon-file and
session-bus warnings are retained verbatim; they did not prevent the asserted
rendering. Initial layer-thumbnail refresh is not certified by this test.
No sanitizer claim is made for this GUI invocation; the separate native flip
model checkpoint carries its selected sanitizer evidence.
