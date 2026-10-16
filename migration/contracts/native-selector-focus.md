# Native selector dismissal focus precondition

The exact-source `58afcbe7` aggregate completed all 156 registrations with 155
passing and one failing native popover-grab assertion in the MyPaint selector
fixture. Every source, tree, ELF and registry integrity check passed. Packaging
correctly rejected the aggregate. The sanitized failed receipt is retained in
`migration/tests/frozen-linux-58af.json`.

The selector test creates two top-level windows before opening the first
window's popover. It previously relied on a single idle drain without establishing
which native window had focus. The existing editor-dropdown fixture already
uses explicit presentation, native focus and a bounded active-window wait for
this kind of modal input test. The selector fixture now uses the same preparation
before its four native dismissal cycles. It adds an active-window assertion;
all original grab, Escape/outside-click dismissal, selected preset, history and
snapshot assertions remain unchanged. No production selector code is changed.

## Evidence and causal limit

A temporary state-only probe on unchanged production passed the entire 29-case
suite and ten repeats. The same three preceding aggregate tests also passed
before the selector. The probe reported inactive parents but a valid popover
grab in all four cycles. The focus-prepared comparison passed the same prefix
and reported the intended active parent and valid grab in every cycle.

The original failure was not reproduced. GTK can remove a modal popover grab
when a later parent focus-out is delivered, but these passing probes do not prove
that caused this aggregate failure. This change strengthens a missing fixture
precondition; it is not presented as a proven production repair.

Final diagnostics are removed. Final normal/HiDPI repeated evidence is recorded
in `migration/tests/native-selector-focus.json`. The failed aggregate is not
waived or reclassified. A later integrated aggregate, sanitizers and other
platforms remain separate gates.
