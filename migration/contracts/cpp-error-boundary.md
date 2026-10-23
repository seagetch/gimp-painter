# Original 05.013: common error-boundary contract

The common bridge uses the `GIMP_PAINTER_ERROR` GError domain and the seven codes
declared in `app/painter/gimp-painter-error.h`. This defines the shared contract;
applying it to every feature adapter, class initializer and legacy property
callback remains explicitly tracked below.

| Condition | Common code | Required result |
| --- | --- | --- |
| Wrong native owner/handle type | WRONG_TYPE | Reject without invoking an implementation of the wrong type |
| Required store or typed slot absent | MISSING_SLOT | Reject access; never manufacture a replacement active implementation |
| Duplicate/reserved typed-slot registration | DUPLICATE_SLOT | Preserve the existing/pending slot; do not publish another owner |
| Operation in an invalid lifecycle phase | INVALID_STATE | Reject the transition or unsupported mutation |
| Closing/closed owner rejects mutation, or store is finalizing | CLOSED | Preserve closed state; permitted const reads end when finalization begins |
| Operation called outside creator thread | WRONG_THREAD | Do not execute the protected operation; acquiring a context does not change ownership |
| Other std::exception, allocation exception or unknown C++ exception | EXCEPTION | Return the declared safe fallback; no exception crosses the C boundary |

Precondition checks follow each public API's defined order. A request with several
invalid inputs is not guaranteed one universal first error. Closing a valid
GObject with no store is an intentional successful no-op; it is not a missing-slot
read. Repeated close is successful. These exceptions to ordinary access rules are
part of the public close contract.

## Translation and ownership

`boundary<Result>` catches all C++ exceptions and returns its explicit fallback;
`boundary_void` catches the same failures without a result. A typed `Error` retains
its code and message. Other standard exceptions retain `what()` under EXCEPTION;
unknown exceptions use `Unknown C++ exception`. Omission of GError output does not
turn failure into success or let an exception escape.

The caller supplies NULL or a pointer to a NULL GError, and owns any returned
error. The templates do not validate that precondition, initialize unrelated
outputs, roll back mutations or detect a normally returned FALSE. Each C adapter
must choose its documented output/failure state, preserve required native error
domains and own rollback before invoking code that can fail. Pure outputs use the
API's initialized fallback or explicit untouched/use-only-after-success rule;
partial owning transfer is forbidden. Propagating a
native GError is not the same as throwing a C++ exception. Never overwrite a live
caller error or free borrowed error storage.

Use nonthrowing C scalar/pointer/enum fallbacks. Argument and lambda-capture
construction before entering the helper is outside its catch. A throwing noexcept
destructor or close hook terminates before an outer catch can recover; cleanup
must itself remain nonthrowing. In-out accumulators follow their native semantics
and are not blindly zeroed. Document any API-specific preservation-on-failure
rule explicitly rather than inferring initialized outputs from boundary use.

When an owned GError must become a C++ message, consume it into a matching owner
before the potentially allocating string copy. `take_error_message` nulls the
source and releases it exactly once, including on a throwing copy. Typed native
domain/code fields must be captured before consumption if the API preserves them.
GLib allocation functions that abort on exhaustion are not made recoverable by
catching std::bad_alloc.

## Native callbacks and class initialization

Every exact-signature C entry/vfunc/signal/source callback must contain exceptions
at its own boundary. Connection manages callback lifetime and disconnection; it
does not wrap arbitrary callback bodies. Source catches exceptions, reports a
warning and removes that source. A void/property callback needs a documented
failure report and valid remaining state; returning from a catch is not a proof
of transactional rollback. Getter outputs retain their native initialized type
and a declared safe value. No adapter may use process exit or throw a pointer as
its recovery mechanism.

Class initialization must validate fallible C++ preparation before publishing
callbacks/properties, contain any exception inside the C callback and record a
terminal failed state that construction can reject. An already partially
registered GType cannot be safely reset by retrying class initialization. The
common boundary contract does not claim those requirements are already met by
every current class initializer.

`05.013/class-init-error`, `05.013/legacy-exit-removal` and
`38.004/all-vfunc-exception-containment` use this contract as input and remain
uncompleted implementation/verification obligations. Existing feature adapters
also retain their application work: for example PaintGate currently reports a
wrong options type through std::invalid_argument/EXCEPTION, whereas the shared
typed-owner path reports WRONG_TYPE. The contract's uniform classification must
be applied and tested there; this definition task does not label all feature APIs
as already uniform.

## Current evidence

Existing real C→C++→C fixtures cover result and void boundaries, typed/standard/
allocation/nonstandard exceptions, optional GError output and exactly-once stack
cleanup. The common Store cases exercise registration, lifetime and creator-thread
rejection; the close wrapper tests null and valid unbound owners. The targeted
12 cases pass natively and under the current source-matched ASan/UBSan binary.
These are common-boundary results, not all-vfunc certification. LSan and RTTI vptr
checks retain their documented environment/build limitations.

The source/result checkpoint is verified by `tools/check_painter_error_boundary.py`.
Original 05.013 closes the common definition and its common implementation
comparison; dependent implementation children and whole-feature gates remain open.
