# Native object-data site review

The existing source gate reports six unaudited fingerprints at seven calls:
two in `app/gui/session.c`, five in `app/tools/gimppaintertexttoolbar.inc` (one
lookup fingerprint occurs twice). They already existed at the initial checkout;
no new legacy bridge API is involved. This is not permission to suppress them.

`review.json` pins the entire source files, exact call tokens, multiplicity, and
native ownership analysis. Session continuation storage is owned by its Gimp and
uses weak GUI-owner references; its destructor retires the idle source and
observers. Text metrics use native C allocation/destruction and weak image/context
references. Their borrowed options are retained by the containing native toolbar;
widget destruction disconnects callbacks before that owner can release options.
No C++ implementation is placed in these native data keys.

Fresh ordinary `painter-compact-options` **62/62** and
`painter-session-restore-barrier` **33/33** registrations pass on authenticated
1080×1920×24 Xvfb, within unchanged180/240-second budgets. The former includes text model-before-view ownership,
unit/image switching, native comparison, and canvas popup teardown; the latter
covers queued/construction/active disposal, deferred save and accepted/canceled
Quit. The original source gate's 69 positive/negative controls pass unchanged.
The six exact fingerprint reviews (seven call sites) now pass the unchanged
full gate:2491 production sources,705 data calls, four explicitly dormant legacy
findings. The69 gate controls also pass after review. No blanket exception,
parser relaxation, disabled diagnostic, production change, fresh sanitizer or
whole-feature completion is claimed. Original05.014 checkpoint counts remain
historical;38.004 and all remaining feature/ABI/lifetime duties stay open.

`validate.py --check-source` verifies current relevant source hashes, exact review
multiplicities and both native TAP results. Raw inherited environments were not
packaged. `baseline-gate-summary.json` preserves the original six missing reviews;
`final-gate-summary.json` records the clean reviewed gate. No new visible UI
behavior was introduced by this source-review-only task.
