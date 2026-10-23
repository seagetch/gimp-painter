# Original header-registration proof and current admission

The original04.014 report/checker described an immutable checkpoint, not every
later descendant. Its unchanged-source assertions and unrelated-ledger checksum
must not be refreshed to whichever headers or completed tasks happen to exist.

`historical-checkpoint.tar.gz` pins172 exact files from
`27f34ac3282c058caf309147dc7460a34d4b87de`, including the original checker,
original duty ledger, every default/HTTP source input and all110 original evidence
members through the nested original archive. The new checker executes that exact
unchanged checker in an isolated read-only-file snapshot. Original231/233 compiler
results,18 controls and all92 duty mappings remain strictly verified. Original
report/archive/manifest/review files in the live repository must remain identical.
No old digest or criterion was rewritten.

The live92 duties separately retain their source identities, phase, owner task,
action, acceptance text, DONE status and nonempty proof references. Unrelated
successor statuses are not frozen: the old unrelated-row checksum is still
verified in its original checkpoint. No duty is removed or retargeted here.

Default command:

```
python3 tools/check_painter_header_registration.py
```

This means historical proof plus current duty coverage, **not current header
compilation**. The output explicitly says so. Current compilation requires:

```
ninja -C BUILD app/painter/tests/headers/libpainter-public-headers-c.a \
  app/painter/tests/headers/libpainter-public-headers-cpp.a
meson test -C BUILD --no-rebuild painter-public-header-compilation \
  painter-additional-header-routes
python3 tools/check_painter_header_registration.py --build BUILD --report REPORT
```

The current gate checks live generated specification/fingerprints, generated
bodies, counts, archive hashes, unique compiled source vectors, C11/C++14 and
-Werror flags, existing objects and actual noninstalled default-build static
targets. Default-build membership is an explicit later requirement, recorded by
`migration/tests/header-default-build.json`; it does not rewrite the historical
statement that the original archives were not default roots.

Twenty-five tests cover original checker integrity, duty deletion/duplication/
retargeting/acceptance weakening/evidence loss, legitimate unrelated completion,
and synthetic current-admission corruption (not synthetic compiler success).
Native compilation evidence is separate. Configured HTTP-enabled compilation
must never be relabeled as current HTTP-disabled or cross-platform coverage.
Current source changes correctly invalidate the corresponding build receipt.

The first current413 compile used the navigation owner's interim void pan_begin
declaration. That owner's subsequent gboolean return correction invalidated its
source seal as intended. `first-current-checkpoint/` preserves the first proof
unchanged; `supersession.json` pins it and records a separate fresh registered
413 compile of the final gboolean header. Current receipt checks never relabel
old objects as having compiled that later declaration.
