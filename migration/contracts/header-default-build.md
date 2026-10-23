# Header compile probes in the default build

The frozen Linux aggregate intentionally runs plain `ninja -C BUILD`, seals
its outputs, then runs all registered tests with `meson test --no-rebuild`.
The registered public-header verifier needs both C11 and C++14 compile archives
and their generated probe manifest before that seal.

Both archives are now default build roots. Their generated-source dependencies
also generate the manifest; the generator remains a non-default custom target.
The archives remain uninstalled and the Meson test dependencies stay intact.
No target-specific prebuild, test-time build, gate exception, or weakened
compiler check is introduced.

## Evidence

The clean native environment at public commit
`b170d6f424d82dd2f46d59ebdb97eb3a389eb7c3` completed all 154 registrations:
153 passed, while the header verifier failed because its manifest was absent.
All source, submodule, executable, registry and freshness integrity checks passed.
The rejected aggregate and the earlier interrupted 5-result attempt are retained.
Neither is accepted evidence of a passing aggregate.

After the two archive flags were corrected, plain default Ninja generated and
compiled all 393 real probes (187 C and 206 C++). The registered header verifier
and additional-route regression then passed with `--no-rebuild` and no skips.
The isolated graph regression copies the production Meson fragment unchanged,
builds a fresh tiny C/C++ project using default Ninja, and checks that no-rebuild
verification changes no probe, manifest or archive. Negative controls exclude
each archive independently and make only the generator default-built.
See the [receipt](../tests/header-default-build.json).

This bounded fix does not itself establish a passing frozen aggregate. That
requires a new run at the new exact published commit with genuine before/after
seals. Platform, packaging, installed-runtime and complete UI parity gates
remain independent.
