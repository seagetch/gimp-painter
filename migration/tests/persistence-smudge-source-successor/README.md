# Committed Smudge saved-field source successor

This metadata-only admission binds the original source pin (parent of 21e9163fd132754b5d575276f5e165c5158470d5) to committed 5d455fd98cb8416f71b65869fbe4801d72a9d36b. Both archived endpoints come from Git, never the held worktree provider. The two committed fixes alter only `motion()`: initial transparent sampling order and float-mask to byte-mask conversion. No held size-domain diagnostic is read or executed, and this package makes no new pixel/runtime claim.

All 17,439 bytes before `motion()` and 18,425 bytes from `interpolate()` onward are identical. This preserves Settings, options_set/get, class_init registrations, default rate 50, default use-color-blending FALSE, property types/flags, and the serialization-facing functions. The two saved-field records and all original family/inventory/TSV data remain immutable. Exact archive/transition pins prevent this boundary comparison from accepting arbitrary motion edits.

Proof baseline is exact committed 9abddbd31938730acaf5d79caf18d2222a4e848e plus the recovered Undo source-admission module (later committed e3b343eb95), this one Smudge admission and the derived manifest delta. It is not a test of later production commits. The source-only metadata gate uses committed native provider bytes, not held candidates. Historical runtime proofs are unchanged.

`test_successor.py` runs two groups with 12 causal rejection controls: changed settings/accessors/boundaries, wrong endpoints, changed source and damaged proof pins. No native process is launched. The full metadata gate may be rerun only on a separately frozen committed-provider snapshot; main contains unrelated held candidates.

The aggregate checker explicitly lists this successor module in its tests[] implementation seals. The derived manifest records its SHA together with the dispatcher and checker SHA changes; no implementation is admitted without an explicit source pin.
