# Genuine old Gaussian negative-region behavior

This separate corpus contains 112 successful, full-buffer old PDB captures on
fresh 8×8 and 9×9 RGBA/Gray-alpha images, plus four independent input load/export
checks. The exact binary, procedure-source, input, output, script and log hashes
are in `capture-report.json`. It uses the same pinned legacy executable as the
alias corpus and does not invoke the XCF reader.

The canonical procedure and both two-radius aliases accept one nonpositive
radius when the other is positive. Their behavior is more specific than merely
skipping an axis:

1. `gauss()` falls back to RLE, then computes input-bound expansion as
   `1 + ceil(radius)` on each axis
2. A value below -1 can shrink the disabled axis. For a nonempty remaining
   region, untouched shadow pixels outside that region are transparent; the old
   REPLACE shadow merge preserves original RGB while setting their alpha to zero
3. When the remaining region is zero or negative in size, there are no shadow
   writes. These fresh-drawable calls return PDB success with the complete input
   unchanged. The old core logs its missing-shadow assertion. The capture log
   preserves that diagnostic; it is not presented as a clean old run or a crash

All 48 empty-region captures are byte-identical to their inputs and correspond
to 48 retained missing-shadow diagnostics.

The chosen finite radii are -1.1, -2, -5 and -17, with the other radius 3.
Odd/even sizes distinguish a one-pixel remaining region from an empty region.
The procedure reports success for all 112 captures. Only coordinate arithmetic
outside defined old signed-int ranges is rejected by the new kernels; it is not
emulated as overflowing pointer/region arithmetic.

These fixtures cover fresh, whole, unselected drawables with alpha. They do not
establish selected-region behavior, stale externally retained shadow storage,
alpha-less negative-region semantics, or arbitrary PDB compatibility.
