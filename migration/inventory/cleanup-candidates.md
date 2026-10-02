# Cleanup candidate classification (01.014)

All **947 changed paths** were screened against their pinned source blobs.
`cleanup-candidate-review.tsv` records a separate result for every path; this
is a classification, not permission to remove a feature.

- The zero-byte `app/dummy.cpp` is named in both GUI and console Autotools
  executable source lists. It carries the C++ link-language role. Replace that
  role under 04.008 before excluding the empty carrier under 31.018
- `app/paint/gimpmypaintcore.h.bak` contains 162 lines of declarations. The
  whole-tree literal-name search finds no reference, but that is not a proof
  of absence of indirect use. Compare its API with the live core before removal
- Sixteen generated outputs are separated from generator inputs, including
  two include-only enum translation units. Keep enum meanings/PDB APIs and
  regenerate; the old composite dispatch table may be replaced only together
  with the verified seven-mode equations
- Nine C-family changed files have identical whole-file tokens after removal
  of comments and whitespace. The comparison retains quoted strings/chars,
  so strings containing comment delimiters are not silently discarded
- The tools and widgets MyPaint editors are two live implementations with
  overlapping purpose. Sharing their model does not authorize dropping controls
- The two ImageGenerator files are executable placeholder source. Their
  dependency/registration review stays open under 31.011–31.012
- `Makefile.am.skel` is executable Ruby, not a static manifest. It scans brush
  and preview assets and writes per-directory installation manifests
- `test.json` and `test2.json` are functional layer presets with filter
  procedures, modes, opacity and replacement instructions. Their names do not
  make them disposable fixtures
- Four `.gitignore` files are build-output metadata; replacement is tied to
  the current generated build rules

The other 909 paths retain their assigned implementation/asset contract.
No file or runtime feature was removed. In particular, generated source,
backup source, templates, manifests, empty build carriers and real assets are
not interchangeable categories.

Verification: `python3 tools/audit_legacy_candidates.py --check` reproduces all
947 rows, including blob sizes, candidate reasons and literal reference
searches. The built-in counterexamples require the Ruby generator, nonempty
backup and functional test-named presets to keep their distinct classes.
