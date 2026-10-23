# Native loopback Xvfb verification, 2026-10-11

All 16 previously skipped registrations have successful individual Meson runs,
252 TAP cases, no individual skips. `attempts.json` preserves initial failures,
the timeout, corrected fixtures, and the final per-registration results. The
`.tap` files contain extracted TAP protocol lines from those actual successful
runs; they are not complete stdout/stderr. Full logs and tested replay scripts
are retained in the user-delivered `standard-native-checkpoint-20261011.zip`.

- Layer: original 60s timeout; passed 63 cases in 83.69s with standard multiplier 3
- Profile: real build-generated data; not installed-prefix startup validation
- Save: isolated in-memory GSettings; not persistent dconf integration coverage
- Fill: 32/32 native GUI cases passed; separate known UBSan arithmetic failure
  remains open and is not converted to a pass

Unmodified Xvfb used standard inherited listening fd 3, bound only to 127.0.0.1,
with cookie authentication and automatic listeners disabled. Authorized clients
succeeded; empty-auth clients were rejected; all servers and credentials were
cleaned up. No system security or network settings were changed.

The maintained procedure is `../../contracts/native-build-workflow.md`.
