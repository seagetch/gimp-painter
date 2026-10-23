# Incremental build evidence

`evidence.tar.gz` contains 39 UTF-8 members: accepted before/after measurements,
exact native commands/output, no-op checks, normalized shared-header dependency
sets, expected production sets, and diagnosed scope/priming attempts. Each member
is sealed by `evidence-manifest.json`. Repetitive full SDK include dumps are
omitted; their query hashes and selected project consumers are preserved.
Executable binaries and environment dumps are not included.

The archive's `run-incremental.py` is the executed native measurement driver.
Its paths refer to the recorded checkout/build environment; adapt those paths
and regenerate the expected graph/dependency inputs for another environment.
The earlier driver whose default measurements were retained is also archived.
Temporary header edits are protected by restoration in `finally`, and builds
are serialized with the existing build lock. Do not run two mutation drivers
against the same source tree concurrently.

```sh
python3 -B tools/check_painter_incremental.py
python3 -B tools/check_tasks.py
python3 -B tools/audit_legacy_granularity.py --check
```

The first command verifies the sealed measurements and exact current source
identities. It does not silently rerun native builds or turn historical results
into cross-platform/runtime evidence. The 28 source-duty mappings close only
their incremental-build verification conditions.
