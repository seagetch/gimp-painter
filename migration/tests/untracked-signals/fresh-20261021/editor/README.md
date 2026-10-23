# Focused editor signal lifetime acceptance, 2026-10-21

Original 25 cases pass (115.937 s), page-memory 3 cases pass (11.408 s). Each keeps its registered 120-second watchdog. Source hashes were unchanged.

Scope: 37 instrumented sources, 48 RTTI-compatibility-only sources. ASan, UBSan and float-cast-overflow; dependencies and other application units are not instrumented. Leak detection is disabled because this container denies ptrace. No whole-application or UI-parity claim.

Runs used authenticated Xvfb at 1080x1920x24. Full stdout/stderr retained: missing source-tree icons, skipped `.dummy` plug-ins, unavailable D-Bus, and disabled input-device mode diagnostics are not hidden. No sanitizer error was reported. No inherited environment dump is included.

The later direct-control extension changes the test harness, not this earlier
receipt. `tested-editor-test-source.cpp` preserves the exact25+3 tested source;
`tested-source-provenance.json` identifies its immutable Git blob. These old
registrations were not silently claimed as rerun after that extension. The new
three controls use their own current-source receipt.
