# Ruler numeric safety

The real GTK ruler now rejects nonfinite/degenerate ranges before dividing,
rejects wholly offscreen markers before converting coordinates to integers,
and widens position differences before taking their absolute values. It retains
ROUND-compatible truncation for ordinary positive and negative coordinates.
Tick traversal stops when floating-point addition cannot advance, and label
formatting avoids narrowing huge finite values to int.

Six registered GTK/Cairo tests exercise both orientations, blank invalid axes,
finite extremes, repeated recovery, offscreen and edge-overlapping markers,
and visible ordinary/reversed ticks and marker pixels. They run through
standard Meson test gimp-ruler; unavailable displays return skip77.

Fresh normal results: ruler6/6, Fill core40/40, Fill tool/options32/32.
The unchanged ruler fails the new zero-range test under standard ASan/UBSan
with signed integer overflow in gimp_ruler_get_pos_rect. Applying only the
production fix makes all six ruler cases pass under the same configuration.
See ../tests/ruler-numeric-safety/validation.json for a portable result summary. Raw execution logs are retained privately.

Leak checking was disabled for these runs; address,
undefined and C++ vptr checking remain enabled. This small fix does not close
WBS08.012 or claim full sanitizer/platform/installed-runtime acceptance.
