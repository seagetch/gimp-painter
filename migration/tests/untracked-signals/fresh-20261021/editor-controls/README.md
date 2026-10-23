# Direct editor control ownership — 07.008

Three registered native cases and three focused ASan/UBSan/float-cast-overflow
cases pass under authenticated Xvfb1080×1920×24. The separate120-second
registration leaves the original25 editor cases and3 page-memory cases and
their timeouts unchanged.

Actual name-changed, Save-clicked and Delete-clicked native emitters are retained
past context replacement, owner close and repeated destroy/dispose. Their actual
handler IDs are disconnected; late emission does not change draft/resource
state or create dialogs. Emitter-first destruction, subsequent rebuild, and close
inside synchronous name notification, Save factory-add, and Delete prompt-show
callbacks pass. No production change was required.

Excluded Delete setup attempts lacked an independent saved resource and then
attached the show observer too late, during the same signal emission. These are
not production regressions. The corrected fixture seeds a dedicated saved resource
and attaches the dialog show-after observer while a child is shown, before the
dialog emission snapshots its handlers. Sanitized failed setup receipts are
preserved separately; acceptance contains only completed corrected runs.

The build report lists exact instrumented and RTTI-only files and source hashes.
Dependencies/other app units are not fully instrumented; LSan is disabled. All
ordinary runtime diagnostics remain in stdout/stderr; inherited environment maps
are excluded. These are owner-lifetime tests, not historical editor UI parity or
whole-application sanitizer acceptance.
