# Exact-source normal Linux baseline

The complete default build and installed-prefix update succeeded at published
commit `65536379b9db5ad91edd3ad3a6a9933572770b8b`. All 154 registered Meson targets
passed in one serial invocation, exit 0, with no failed, timed-out or skipped
registrations. The authenticated loopback-only Xvfb screen was 1080×1920×24.
`report.json` retains every result, duration, return code, TAP skip and matched
warning/error diagnostic without inherited environment variables.

The same 31,834 selected source, target, object, installed-prefix and dependency
file-byte hashes were present before and after testing; no byte changed and the
source commit remained identical. These byte seals are deliberately narrower
than `capture_freshness_snapshot()` in the packaging checker: graph metadata,
file modes, symlink identities and absent outputs were not fully captured.
This evidence therefore does not close `34.015/frozen-linux-aggregate`.

One TAP subtest remains skipped: the >256 MiB native XCF multipart case requires
slow mode. Zero skipped registrations must not be read as zero skipped cases.
The result retains GEGL swap-cleanup diagnostics in gimp-filter-layer, the
writable-gradient search-path error in the direct-GIMP tests, and intentional
negative-case application warnings. The first two require fixes and fresh
runtime evidence; successful exit codes do not erase them. The report preserves
all matched messages, with repeated identical messages counted.

The prior 153-target baseline, original compact-options timeout and historical
failed frozen aggregates remain unchanged. This checkpoint does not claim
full application sanitizer coverage, legacy UI parity, other platforms,
physical tablet acceptance or a distributable package gate.
