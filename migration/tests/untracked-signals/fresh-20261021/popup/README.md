# Focused popup/dropdown/selector signal acceptance, 2026-10-21

Authenticated Xvfb 1080x1920x24: popup handles 34/34, editor dropdown 57/57, selector contracts 29/29 pass. Registered timeouts remain 120/120/180 seconds. Matching ordinary 120-case results are included in `normal-results.json` and selected stdout/stderr. Its five recorded source hashes match the focused sanitizer snapshot; raw Meson inherited environments are omitted.

The private builder instruments 21 explicit sources and recompiles 54 bridge sources for RTTI compatibility only. ASan, UBSan/vptr and float-cast-overflow apply only to listed instrumented units, not the entire app or GTK/dependencies. Leak detection disabled due container ptrace restrictions. Source hashes unchanged during build; executable hashes match runtime results.

All stdout/stderr retained, including missing source-tree icons, skipped `.dummy` plug-ins, unavailable D-Bus and device-mode diagnostics. No sanitizer error occurred. No inherited environment dump is published. This is lifetime regression coverage, not UI parity acceptance.

`build.py` is a local adaptation of the existing editor focused builder: one private object/archive set links all three test executables without replacing ordinary objects. Run under the shared build lock, with the pinned build environment; runtime uses the exact registered test environments and watchdogs. Absolute paths identify this execution workspace.
