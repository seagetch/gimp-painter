# Run47 catalog-helper-mismatch

Exact cause: the frozen helper4cdc8a36be383487c4cca06fc173f8d3c2b949370318b66fd15f22548eecfada returns a catalog manifest without helper_sha256. The driver serialized it unchanged. The consumer requires helper_sha256. Both frozen source arms already pin the same unchanged helper; this is a missing producer witness, not evidence of different helper bytes.

Fix only the driver's existing staging bridge. Before executing the real helper, require a regular nonsymlink file with its exact pinned SHA256. Execute the unchanged stage_build_locales function. Recheck the same file/hash and reject an unexpected producer field. Then append the witnessed helper_sha256 to the private manifest. Preserve every returned metadata/catalog field and all consumer integrity checks. Do not edit the production staging helper, source arms, asset bytes, metadata checks or catalog selection.

Five tests execute the exact heredoc extracted from the candidate driver. The positive case uses the real byte-pinned production helper and actual minimal GNU .mo bytes with Meson-format metadata. It proves the old function omits the field, the bridge's manifest equals the original plus exactly helper_sha256, and staged catalog hashes match. Negatives cover wrong helper before execution, helper mutation during staging, forged producer field and helper symlink. Bash syntax and all5 tests pass.

This is a real helper fixture replay, not hosted GIMP catalog/runtime comparison acceptance. Full resource validation, native results, independent review and exact full-tree sealing remain required. No repository writes were performed externally.
