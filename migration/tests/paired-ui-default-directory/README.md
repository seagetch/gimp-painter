# Plain Meson paired UI capture

The exact a84cd aggregate found that `paired-stroke-ui` assumed an externally
provided GIMP_TEST_PAIRED_SCREENSHOTS. A plain registered Meson run failed before
capture. The fixture now allocates one unique temporary directory before GTK
initialization when the variable is absent. Explicit evidence runners still
control their destination. No capture, viewport, oncanvas or pixel assertions
were removed or relaxed. The directory is retained and reported for inspection.

Verified with the variable explicitly unset: authenticated Xvfb1080x1920x24,
actual1040x1800 app window, oncanvas visible, four PNGs of the same217,748510x382
image interior. Painted differs, Undo equals baseline, Redo equals painted.
Native pixels and full evaluator/resource comparisons also remain. Direct
controller API plus normal flush, not synthetic mouse gestures or composites.

This receipt tests the fixture alongside the separate save-pending integration
correction in the private clean-provider build; it is not a claim that the
entire committed port passed. The prior failed aggregate is preserved under
migration/tests/exact-aggregate-a84cd. The broader paired child stays DOING
until its separate production save correction is verified and committed.
