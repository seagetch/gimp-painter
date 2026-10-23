# Register the large XCF round trip

The previous normal aggregate returned Meson OK while skipping the existing
>256 MiB multipart native Save/Open subtest. The measured unchanged large case
passed with a 268,435,457-byte payload and three public Save/Open cycles in
38.96 seconds. The full registered target now runs `-m slow` and has a 180-second
outer budget. Its ordinary Meson invocation, without extra args or a timeout
multiplier, passed all 49 cases in 87.50 seconds with no TAP skips. A 60-second
budget would not cover that observed complete run.

No subtest, assertion, payload, numerical tolerance or application timeout was
changed. Existing opt-in direct-binary behavior remains available; the standard
Meson registration now explicitly includes the expensive case. The original
skip and first nested-ternary Meson configuration error are retained in the
historical checkpoint and this report. The corrected Meson source uses explicit
if/elif and normal regeneration, never edits generated backend files.

This is focused registered-XCF evidence on authenticated portrait TCP Xvfb,
not a new full aggregate, instrumented application or frozen packaging pass.
