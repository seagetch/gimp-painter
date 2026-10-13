# 08.017 bounded native layer-tree integration

Declared public base: `fd1af82f97e8e3c16851cd3e201b3c010c6121ed`. This subset adds a native layer-tree popup entry,
the direct filter action, their provider and lifetime corrections, and explicit
additive C/C++ header routes. Original tasks remain 140/661. All 93 original
08.017 source duties remain TODO and all three work ledgers remain byte-identical.
No 29.* feature duty, inactive provider duty or original row is accepted here.

## Observed red and fixture provenance

The unchanged public production baseline failed three native assertions: missing
popup column, missing direct filter button, and stale returned drag viewable.
The sanitizer drag-cache red was also an assertion, not an observed ASan memory
diagnostic. The original test revision and unchanged three function bodies are
identified in `baseline-red.json`; later fixture helper changes are disclosed.

On the intermediate popup candidate, refresh reentry failed with a fatal GObject
critical from `g_object_notify_by_pspec`. This is not labelled an ASan memory
report. Retaining the actual adjustment across value-setting callbacks repairs
that production lifetime boundary. Separately, selection removal reproduced a
real ASan heap-use-after-free in `GIMP_IS_VIEWABLE` after native parent selection
dispatch. Requested items/list nodes, view and image now have bounded call leases,
with authoritative-state validation before the UI tail; the native event path
also rejects a destroyed container or detached column after selection.

Fixture-only corrections retain the child used as a signal argument after owner
destruction, detach/ref live range controls before popup destruction, and force
layout before native hit lookup. A focus assertion previously depended on window
manager activation; the final fixture observes the native `grab-focus` action
after its default closure and asserts exactly one callback. These fixture
corrections are not presented as reproduced production defects. All 26 named
layer cases remain; diagnostic-only temporary tests are excluded.

## Tested scope

Both native and standard ASan/UBSan passed all eight selected registrations, 426 cases per mode, and both header gates passed 363 compile probes plus 16 route controls per mode.

The layer cases cover native event and isolated provider paths for clicked-target
controls, selection/reorder/removal, owner/column destruction, popup replacement,
refresh reentry, group pass-through and retired native drag-cache reads. Some
controls use a direct provider signal rather than native input. Scrolled geometry
uses a synthetic full-row provider area. The filter case opens/cancels one
existing filter editor. No multiple eligible filter mutation or Clone popup
editing, physical pointer/tablet, keyboard, old inline filter/argument UI, grouped
mode presentation or historical custom drawing parity is claimed.

Native keeps the project's ordinary `-fno-rtti` policy. Standard ASan/UBSan uses
`-frtti`, retains vptr checking and the ordinary registered timeouts, with runtime
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1`. Leak detection is disabled and
third-party dependencies are uninstrumented. This is eight selected registrations,
not the full aggregate. Suggested mutation controls were not run.

Header evidence separately covers 363 C11/C++14 probes and 16 route-schema/
closure controls. The three reviewed extra headers are additive; historical
inventories and probe identities stay intact. Compilation proves neither link
ABI nor runtime acceptance. The final runtime runner seals 13 executable-source/
route inputs plus surrounding selected sources; the route README is frozen as
documentation, not claimed as an independently before/after runtime input.

## Original duties and publication

`source-contract.json` preserves 18 related original rows, exact payloads,
individual responsibilities and remaining acceptance requirements.
`all-original-duties.json` preserves all 93 original rows and sibling duties,
including those outside this bounded subset. Passing this subset does not turn
whitespace/comment/debug rows or absent old APIs into completed work.

The manifest carries exact Git blob SHA-1, SHA-256, mode and byte length for every
allowlisted payload, with the eight existing-file preimages and six new paths.
Root must verify these preimages and absent paths against the declared public
base before publishing. Reports use a positive allowlist: no raw logs, machine
paths, process identifiers, display endpoints or environment dumps are included.
The freezer has no network/publication operation and cannot advance any ledger.
