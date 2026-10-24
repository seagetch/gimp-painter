# Colorify explicit typed tint: actual native XCF transport

Original 16.022 remains DOING. This is new explicit `(sudddd)` identifier/version/RGBA transport, **not recovery of the old writer's lost boxed tint**. Existing capsule argument codec suffices; no product/schema change.

Four valid tints and eight unsupported tuples make 24 actual Save/Open rounds. Valid metadata is serialized-byte exact including negative-zero RGB and unused alpha. Independent integer luminance plus exact half scaling checks all pixels, including alpha/hidden RGB. Opening a complete cache executes zero times; changed lower input recomputes valid definitions. Unknown identifier/version, shape, negative/out-of-range, NaN/infinity and null stay typed and fail without execution/cache replacement, before and after reopening. A retained older MaxRGB raw capsule deliberately disagrees with the new native model: it never overrides typed Colorify metadata.

Normal: two own cases plus Exchange model-precedence and Red Eye roundtrip regressions. ASan/UBSan: two own cases, eleven IO/loader/compat/argument/preservation/test units, native no-RTTI retained and vptr excluded. Filter core, storage/dependencies uninstrumented here; separate Colorify native23-unit proof remains independent. No UI or old application execution claim.

Checkpoint is exact503 plus prior frozen Colorify native/Exchange XCF sources and three new test-only files. Full before/after manifests compressed losslessly, selected source archive and exact commands retained. No Video raw overlay. Runner expects that checkpoint and is archival, not a portable fresh checkout builder. Portable verifier: `python3 migration/tests/colorify-xcf/validate.py [--check-owned]`.
