# C36: case14 initial application-frame precondition (review only)

Run53 public C35 ef5f08e68c1f0692b40b938c9ea4c08763db1483 (tree
7ddedb127e9f31b3d960f07147f48ff5ef1674d0) completed5success/3failure. C35's
owner record transport succeeded:62 complete rows,16owners/32probes/12events,
finish breakpoint matched, cleanup confirmed and primary identity unchanged.

## Evidence and its limits

The long observed dispatch enclosed mapped GimpImageWindow owner2's frame-layout
and check-resize. Their nested intervals were8,466,898us and7,917,112us; whole
dispatch8,892,519us. These are debugger observations, not additive CPU costs or
native speed certification. Target-private check-resize/layout probes saw no
emissions. Samples2–8 hit96-frame cap; five include SVG loading under GTK preferred
width traversal and ASan unwind. Exact internal child/widget remains unknown. No
production SVG/widget performance repair is claimed. Native first private wait
separately failed stable0 after8,672,506us, one dispatch8,668,845us; target's first
frame arrived8,671,338us. Prior-run target-frame ordering is not substituted here.

GUI restore creates/presents the empty image window (gui.c), then tests.c enters
g_test_run without a frame barrier. Existing drain() only runs until an idle callback;
future frameclock work can remain pending. Case14 constructs its private fixture
immediately; ordinary Brush fixtures additionally create/resize an image shell and
settle events before their resource setup. Source and owner evidence support an
initial-application-layout overlap, not a proven production invalidation defect.

## Narrow fixture change and explicit budget

Only private-target-release calls the new precondition, as its FIRST statement,
before tool lookup, private options/brushes, registered factory or private window.
Exactly one already-existing GimpImageWindow is required; no arbitrary toplevel,
show/hide/resize, selection mutation, direct asset load, global warmup or popup work.
It must already be mapped/realized/viewable with a frameclock. Actual after-paint
observations must preserve host/native-window/clock identity and geometry for three
successive comparisons, across distinct completed frame counters (four frames).

The new setup phase has an explicit separate10second monotonic deadline. Missing
frames, invalidation, destruction/unmap, replaced window/clock or deadline expiry
FAIL this setup phase; never skip, retry, continue, or extend the private wait.
The check also rejects a single nonpreemptive dispatch that crosses10s even if it
reports enough stable frames. Held refs, temporary handlers and frameclock updating
are released/balanced before any result assertion. Finite elapsed/frame/stability/
invalid/expired/ready fields are logged. A GTK dispatch cannot be preempted by a
polling deadline, so elapsed may overshoot, but overshoot cannot pass.

The existing private test body after this call is byte-identical to C35, including
both opens, weak lifetime assertions and hide/reopen/destruction checks. Its original
first-target readiness3second/stable>=3 helper is byte-identical; every popup-specific
construction and dispatch remains on its original side of that measurement boundary.
All26 protected common files and existing21case acceptance manifest are unchanged.
The10s budget is additional CASE14 SETUP allowance, not a native popup speed repair.
It must be explicitly reviewed as such before publication. This stops measuring
private activation while application startup is unfinished; it does NOT prove
cold-start-plus-private-popup readiness within3s or improve application startup speed.

## Local proof and outstanding hosted proof

New safe tests compile the EXACT precondition core against deterministic ABI-shaped
stubs (not real GTK). Seventeen model modes cover normal/fresh frames, geometry reset,
8second startup, no frames, late callback, enough frames followed by dispatch overrun,
destroy/unmap, unrealize, replaced clock/native window, repeated frame counter, and
missing clock/unmapped/nonviewable admission, invalid geometry and origin failure. All paths check refs, signal cleanup and update
balance. Original private body and readiness-helper hashes are asserted. A separate
compiled copy of the original3second wait LOOP still rejects a3.1second target stall;
this is predicate/model evidence, not a native GTK delay-injection result.
Bootstrap CI runs these tests using its existing C compiler; no new dependency,
job, debugger capability or native acceptance waiver. Primary source and gate seals
must be regenerated for this fixture-only change.

Fresh exact-source native old/new controlled evidence remains pending. Required:
prove startup frame completion precedes private construction and unchanged first
wait, compare fresh baseline/setup-ready processes under unchanged assets/sanitizer,
retain native first-wait timing and all ownership assertions, and verify a native
target-readiness negative still fails. Rendering the real startup window can warm
shared icons incidentally; no isolated cache-effect/performance claim is made.
Remaining aggregate leaks, matched-comparison inconclusiveness and Task308's open
status remain. No local GUI/ptrace/babl execution or GitHub write is implied.
