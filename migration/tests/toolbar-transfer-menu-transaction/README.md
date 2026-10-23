# Native toolbar transfer action transaction — 29.010 child

Scope: a selected live image whose shell temporarily has no valid native
window membership/owner must not be changed through an already-open menu or
public global GAction entry. No-image UI synchronization must not change that
image's state. The broader 29.010 UI migration remains open.

The fixture uses real Gimp image windows, their shared native action manager,
and native GtkCheckMenuItem/GtkRadioMenuItem proxies with Painter oncanvas UI
explicitly enabled. An `unmap` callback observes the precise intermediate
state: old GTK owner still present, but the toolbar action route already
withdrawn. A second phase exercises the genuinely parentless shell without a
main-context iteration, then restores its owner and proves actions resume.

The production guard obtains its application through a weak reference, never
dereferencing the historical non-owning action-group pointer. Group association
precedes `action-added` publication. Radio command delivery precedes proxy
notifications; an owning snapshot and per-member revision cancel obsolete
outer command/notification publication following nested selection, regrouping,
or explicit disposal.

## Regression coverage

- `transfer-global-actions`: old-owner/membership gap; parentless direct toggle,
  direct change-state, open checkbox menu, Close, UI update; unchanged model and
  proxy; resumed action after reattachment.
- `transfer-radio-notification`: native notify observer withdraws shell; model
  and radio state agree; radio menu and direct change-state remain blocked.
- `transfer-action-publication`: action-added observer attempts activation/state
  changes during transfer; action works after restore and after group death.
- `transfer-radio-command-supersession`: first command recursively selects a
  newer radio value; additional positive controls regroup another member and
  explicitly dispose the retained anchor. Obsolete outer callbacks stop.

`expected-red.json` records four real failures against unchanged production.
The earlier GtkContainer remove emission-hook fixture missed the interval and
is explicitly excluded as regression evidence. Regroup/dispose variants were
added as positive controls after the original nested-value red run.

## Candidate validation

The ordinary native target passes all four focused regressions and all 26
registered toolbar cases (90.13 seconds, original 240-second suite and
60-second child budgets, zero skipped cases). `candidate-results.json` omits
Meson's inherited environment; `green-source-binary.json` binds the tested
binary to the exact production/test inputs and reviewed header-route inventory.

The exact `gimpaction.h` route identifies baseline blob
`95b757b65c3bcbcb584376e0ab4fc48d761f54a0`; `gimppaintercanvasui.h` remains
in the original reviewed public target list. Both are actually compiled in C11
and C++14, twice included with their own linkage guards (no outer wrapper).
`header-compile-result.json` proves 407 probes (193 C, 214 C++), and the 17
additional-route rejection controls pass. No historical ABI claim is made.

Three PNGs are actual authenticated Xvfb 1080×1920×24 root captures with
`painter-canvas-ui` asserted true; Painter Canvas Controls is visibly checked.
The native Show Guides checkbox stays checked during blocked operations and
becomes unchecked after owner restoration and successful activation. Frame 02
intentionally has no GTK event-loop iteration during the synchronous transfer:
it retains the previous rendered frame, while native assertions prove that
commands did not mutate model/proxy state. It is not proof of a rendered
ownerless window.

Excluded setup/fixture failures: an interrupted pre-test build; an exact
header-closure rejection resolved by reviewed route registration, not bypass;
and a supersession fixture that disposed the active member while still
asserting the remaining group's state. The latter now validates all members
before cleanup; radio fallback during actual cleanup remains legitimate.
These are not presented as production red evidence.

## Bounded sanitizer result

`asan/results.json`: all 26 registered cases pass in 166.97 seconds within the
original 240-second budget, ASan/UBSan halt-on-error enabled. Exactly nine C
translation units are instrumented (image window, action/action group/action
implementation, toggle/radio action, menu, UI manager, toolbar test); private
replacement archives leave normal build objects untouched. The new included
fixture and both changed headers are source-hashed as well. This is **not** a
whole-application sanitizer or leak result: dependencies/other app objects are
uninstrumented and LSan is disabled. No sanitizer suppressions or increased
timeouts were used. Stderr retains benign missing-icon/dummy-plugin/DBus
messages; no ASan or UBSan diagnostic was found.

Evidence packaging excludes Meson's inherited environment dictionaries. A
credential-pattern scan found no bearer token, GitHub token or private key;
this does not authorize publication. The parent task keeps publication paused
pending the required approval. `evidence-sha256.json` hashes the evidence files
(other than itself); source and binary identities are in separate receipts.
