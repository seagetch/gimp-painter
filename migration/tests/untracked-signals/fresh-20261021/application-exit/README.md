# Actual application accepted exit — 07.008

Installed native application under authenticated Xvfb, screen and settled window
both 1080×1920. The private profile enables painter-canvas-ui; the actual screenshot
was visually inspected and shows the oncanvas toolbar, dock panels and layer strip.
The baseline XCF was loaded. The optional HTTP service authenticated a loopback
GET /api/v1/images/ with status 200. An ephemeral per-run token was passed only in
the child environment and request header, never logged or included here.

XTest Ctrl+Q exercised the real accepted exit: verbose app_exit_after_callback
was observed, process exited 0, and the HTTP listener was closed. Source hashes
and executable hash are in result.json. This is normal native application
shutdown evidence, not whole-app ASan coverage or full UI parity.

An earlier setup attempt saw startup reset the window to 800×600 and is excluded.
The accepted runner resizes and rechecks the settled actual window before capture.
Runtime diagnostics (including missing optional icons and session bus) are retained.
No inherited environment or private runtime profile is packaged.

Exact tested production source snapshots are retained under tested-source/. Later
independent changes do not retroactively inherit this execution receipt.
