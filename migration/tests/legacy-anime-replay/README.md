# Original13.014: current-core anime edit replay

The test replays the **actual retained old capture actions**, not inferred end
states: load initial old anime XCF; whole-source fillRGBA51,153,204,255 without
Undo; update48x40; clear history; shadow opacity.6 with Undo; ordinary Undo/Redo.
Original capture source lines1053–1058 and hashes are retained. Five independent
old projection buffers each match all7680 RGBA pixels. Original recolor and
opacity each change1920 pixels, so a stale projection cannot pass every state.
Clone type/source/hierarchy, shadow mode and opacity are checked throughout.

Selected native and six-C-unit ASan/UBSan pass on clean baselinea19f05f9c8 plus
parent XCF integration and owned test overlays. Parent XCF build source hashes
are recorded; this is not an unqualified whole-current-HEAD run. The six units
are byte/float operations, float kernel, mode registry, group and C test. Clone
C++, XCF, GEGL/Babl and other dependencies are uninstrumented; LSan is off.
The prior primary model-only probe is labeled separately, not substituted for
this clean result.

This is current-core API replay, not pointer/keyboard interaction or source-paint
Undo. It loads a synthetic old anime composite, not real user artwork. No old
runtime or watercolor/Edge path runs. Earlier actual portrait/oncanvas screenshots
remain in `legacy-anime-current/`; no new UI screenshot is claimed here. The
original13.014 criterion is unchanged; dependency13.013 remains open pending
parent review. Scope labels are not additional invented acceptance requirements.
