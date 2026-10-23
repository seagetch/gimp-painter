# Native preset gestures on a portrait oncanvas desktop

This is a test-fixture correction, not a change to preset, popup, preview, or
canvas-timeout implementations. A fresh committed-source application build
uses authenticated Xvfb1080x1920x24, O2/no debug symbols, assertions enabled,
HTTP enabled, its own profile/prefix and no candidate object reuse.

The fixture now explicitly enables painter-canvas-ui. Its initial native-click
failure occurred **before dispatch**: a target row rectangle at y=-25,height24
was above the compact256x46 viewport despite a minimal-scroll request. Merely
waiting for positive cell allocation was not sufficient. The fixture requests
native centered scrolling, waits for the visible cell/viewport intersection,
and requires the exact native row and column hit-test within the original2s.
The original dispatch, selection, operation and Undo assertions remain.

The blank-release test also assumed bottom-2 was blank, but this compact viewport
contained real rows there. Only that case now attaches a one-row synthetic
container to the view, establishing actual in-window blank pixels. The real
application factory/container is never modified, including on assertion abort;
the original view container is restored on normal completion. No invented
out-of-window click, skipped case or production behavior change is used.

A conditional screenshot is taken after actual queued native primary press and
release, exactly one generated clone and one Undo step have been verified. Only
then is the real image window resized1040x1800 for evidence presentation; original
window size is restored afterward. This is not resizing to conceal the hit-test
failure. Actual visible oncanvas UI is asserted. The screenshot is a rendered
application state, not a mockup or a direct model-API-only image.

Initial failures, including the failed scroll-readiness and missing blank-region
attempts, remain explicit evidence. Separate existing perspective UI regression
runs on the same clean source. No legacy runtime, full application sanitizer,
full port completion or paused candidate UI claim is made.
