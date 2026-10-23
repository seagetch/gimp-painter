# Original 13.014: genuine old anime fixture checkpoint

Current GIMP opens the existing genuine-old anime initial and edited XCF files.
Each 96x80 projection exactly matches all 7,680 RGBA pixels of its old capture.
The tests also verify CloneLayer dynamic type and resolved source pointer, group
parent, DST-IN/Multiply modes and initial/edited shadow opacity. These fixtures
are synthetic rectangles from the old anime preset, not real user artwork.
The edited file is opened, not reconstructed by replaying edits in this test.

Native aggregate (three tests including the preceding mode selector) and focused
ASan/UBSan pass. Only the standard mode combo/box and C UI test are instrumented;
XCF, CloneLayer, C++ bridge, compositing and other dependencies are not. LSan is
off. This does not replace separately scoped persistence instrumentation.

Both PNGs are actual current-GIMP full-root captures at authenticated Xvfb
1080x1920x24. Each was captured in a fresh process selected with
`--test-args="-p /painter-mode-ui/anime-initial"` or `anime-edited` and
`GIMP_TEST_ANIME_SCREENSHOT_DIR` pointing here. Oncanvas configuration and public
visibility were asserted, and visible oncanvas tools/color wheel/layer tiles
were visually checked. No pointer/keyboard event or user-edit replay is claimed.
Fresh processes avoid preceding test-window state affecting the illustration;
no canvas lifetime fault test or paused implementation was exercised/changed.

Original 13.014 remains DOING: dependency 13.013 and broader artwork acceptance
are not established by these two fixtures. Watercolor/old Edge runtime remains
SKIPPED and was not executed. Original criterion is unchanged.
