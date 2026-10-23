# Original filename brush loader — bounded 08.018 child

Canonical row42 names `gimp_mypaint_brush_load`; the original acceptance is
removing Interface::cast from the C entry into the new implementation. This
change restores its absolute-filename C signature and dedicated source header,
with its three original GLib preconditions. One private GFile/input stream
feeds the sole native loader, parser and BrushSlot; no duplicate resource model.
The private stream is released on success/failure and no file/stream is borrowed
by the transfer-full result. File-open failures preserve GIO errors; parser
failures use the existing native boundary error policy. No C++ exception crosses
this C entry. The typed owner delegates once, consumes the list node and adopts
the resource, reporting failures as its established C++ error type.

`historical-contract.json` identifies the exact old source/header blobs. The
canonical row stays PARTIAL: native strict parsing, errors, parent-name and
embedded/sidecar image policy are explicit migration differences, not silently
restored legacy semantics or binary subclass ABI. Save row44 and parent08.018
remain open. This is non-UI work; no screenshot change is claimed.

Final ordinary native suite: **34/34 PASS**, including all 30 prior cases and
four new mixed-language cases, in 1.76 seconds. Final bounded ASan/UBSan plus
float-cast-overflow suite: **34/34 PASS** in 2.43 seconds. Both retain the
original 60-second budget and have no skipped/TODO cases. Native Xvfb verifies
1080×1920×24 with authenticated loopback access and rejects empty authority.
The new header closure compiles 409 probes (194 C11, 215 C++14); 17 route
rejection controls pass. One linked unmangled filename-entry definition and
actual C11/C++14 compiler commands are in `ordinary-receipt.json`.

The sanitizer build instruments 36 parser/provider/test/common-store and
registration translation units and rebuilds 40 further C++ units for RTTI
compatibility only; full lists and exact source/binary identities are in
`asan/build.json`. Other application/dependency units are uninstrumented and
LSan is disabled. This is not a whole-application or leak proof. No sanitizer
diagnostics or suppressed errors occurred. Benign synthetic plug-in/icon/DBus
messages remain in stderr. The C11 fixture has a nonfatal inherited-style
`-Wdeclaration-after-statement` warning; no compiler error.

A preimplementation `nm` observation records the absent filename symbol; it is
not presented as a runtime expected-red. An initial green run preceded an
additional JSON parent-name positive control; the final sealed run includes
that control. Evidence excludes inherited environment dictionaries.
`validate.py` checks current owned source hashes, ordinary/sanitizer identity,
exact TAP counts, original budgets, C compiler/language proof, canonical
PARTIAL disposition and complete recursive artifact hashes.

 C tests cover actual
absolute-path reads, two independent resources/icons after file deletion,
owned cleanup, NULL/relative/prepopulated-error preconditions, missing-file GIO
errors, malformed/empty/nonfinite input and minimal valid legacy content. C++
tests verify the exact C signature, owning copy/lifetime and error translation.
