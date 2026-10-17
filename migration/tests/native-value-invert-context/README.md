# Bounded native Value Invert acceptance receipts

The adjacent `../native-value-invert-context.json` is the compact index. Gzip
files retain the complete reports and logs, including both failed registered
attempts. Commands are exact local invocations; their absolute paths describe
the tested workspace, not an installed or portable runtime contract.

- Normal: 21 exact paths on `7b80f7990d573dcc0aa2e50f0e35f31c45173826`.
- Scoped ASan/UBSan: the same 21 paths on
  `a585034aeeb0040930c4e96e0af917af4392df20`; 40 instrumented and 36 RTTI-only
  units. Remaining code is uninstrumented and leak detection is disabled.
- Registered: 13 exact unfiltered targets on
  `7018d00e35b4c7ed1f1954df31c1a1e3878006d8`, after a complete default build.
  The first attempt lacked the helper and created 19 bytecode files. The second
  lacked its nested Blinds executable. Both failed receipts are preserved.
  The final run preflighted 18 selected executables, held the build lock and
  verified equal source path sets plus unchanged source/build seal maps.
- Python controls: 36 capture, 20 sanitizer-runner and 8 normal-runner tests.
  The earlier combined import exited after the sanitizer module's top-level
  20-test runner; its log is retained as incomplete, not a 64-test success.

Normal's fixture verification independently compared every live consumer file
against newly materialized pinned Value Invert archives. The sanitizer used the
verified outer wrapper, retained both archive verifier receipts, and sealed its
selected consumer files. No credential contents or full environment dump are
included. Runtime manifests after legacy recovery are post-build evidence and
were written after capture; they do not establish pre-capture archive seals.

Later provenance-only commits, this contract/ledger update and these receipts
are not substituted for any earlier exact tested tree. See the compact index
and contract for explicit limits, including spill RUNNING timing, installed UI,
other platforms, direct PDB/precision expansion and whole-project completion.

Publication preflight subsequently removed one redundant EOF blank line from
the native capture consumer. `whitespace-only-delta.json` retains the exact
one-byte proof and empty ignore-blank-lines diff; the original tested hashes
remain unchanged. Final source hashes and the repeated 64 Python/WBS controls
are recorded separately. This correction does not claim a new native run.
