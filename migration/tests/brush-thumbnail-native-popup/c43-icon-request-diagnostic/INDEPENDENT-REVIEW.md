# Independent C43 requested-icon diagnostic review

Status: PASS for the bounded local source/synthetic diagnostic scope. No unresolved blocking finding remains in the reviewed code. Review is local source and headless synthetic testing only. No GIMP/GTK GUI, debugger/ptrace, babl retry, native build, GitHub write, or production change was performed by this reviewer.

## Baseline and protected behavior

The restored checkout identifies exact published C42 HEAD `180446d7a1dfe92739b8bd1881922d5d384dfb26`, tree `a4166d0e0bc3e4983fc9cf93b914eb84d15c2e50`. Independently compared the 26 names in `verify_common_donor.PROTECTED` to HEAD: all byte-identical. The primary transform test, sample_case14 controller, preframe identity/sanitizer policy, image.inc and run.sh are also byte-identical. Original three-second stable-frame assertion and leak settings remain unchanged.

Removing only the two added IconCall lines from leaf.inc reproduces C42 leaf bytes exactly, SHA-256 `451188e12334aff2a5c4db533cf775164383b88cec355d96006a5725607b79b8`. Removing both image lines as well reproduces the original leaf hash `bc1754ff6dabe264157a7d5a65d36ee60cd6cc06f3c4790af70129c037666055`. Existing forwarding/restoration machinery is otherwise unchanged.

## Safety and semantics

Names are compared by length and exact bytes in a fixed 256-name dictionary. At most 128 bytes are inspected, accepting at most 127 bytes plus NUL. Configuration groups are capped at 128, followed by a numeric overflow aggregate. Existing dictionary/group keys remain searchable after capacity. No case-folding, normalization, string hash, name, path, or address is exported. Status 1 alone establishes requested-name equality within one run. Null, non-icon, too-long, dictionary-full and invalid-metadata buckets never establish name equality. Names are cleared at finish.

Entry gathers metadata and weak-snapshot membership before calling the saved original exactly once. The exit token contains only a group ordinal, active boolean and timestamp; exit does not access widget/object APIs. Fixed group storage survives original destruction/finalization, and counters, time arithmetic and unmatched calls fail closed. Durations are inclusive and nested spans overlap. Added metadata work affects enclosing image/leaf timings.

Fallback setup validates readable boolean `use-fallback`, a nonzero property ID, and the exact GtkImage owning type. It caches the GtkImage base GObject getter before timing and invokes it only for exact GtkImage runtime instances; subtypes fail closed. Local upstream GTK 3.24.49 source confirms gtkimage.c PROP_USE_FALLBACK only assigns a boolean from the helper, whose getter returns the stored bit; gtk_image_get_icon_name returns borrowed storage. gtkwidget.c lines 12065–12073 confirms direction reads private/default direction. No Debian patched-source byte-identity claim is made.

The C42 image metadata helper and weak snapshot are reused, including ancestor/host limitations. Collector attaches parent image capture/coverage information, and the icon parser explicitly states that missing semantic types can broaden ancestor buckets. Tracked is snapshot membership, not unique/new-image identity. No captured record establishes resolved asset identity, cache hit/miss, coldness, decoding, theme/style equivalence, readiness improvement, or native acceptance.

## Parser and integration

Strict numeric grammar, exact field counts/order, contiguous group/name ordinals, complete marker, config uniqueness, canonical invalid metadata, metric/summary conservation, name/storage/status consistency, clipping prerequisites, bounded input/chunks/records, mixed-transport rejection and overflow/drop distinction were reviewed. Raw rejected text is not retained in exported rows. Unknown-name aggregates do not grant equality coverage. All fault flags make icon results inconclusive.

Generation includes image before icon before leaf; begins icon after image and before the original timer; finishes icon before image. Generated icon source and parser join the identity closure and explicit Meson dependencies. Collector requires all five observations captured, image-to-leaf counts and icon-to-image entry/exit agreement. Durations deliberately are not compared for equality. If a prior observation is already inconclusive, a subsequent count crosscheck may be skipped, but overall capture cannot pass; the parent-image dependency is exported.

## Findings resolved during review

1. Status 6 initially retained normal metadata while the parser required canonical empty metadata. Producer was normalized, preserving only independent tracked membership.
2. Parser initially accepted impossible dictionary/group/drop declarations. It now enforces used contiguous name ordinals, no orphan dictionary entries without overflow, dictionary-full names=256, overflow only at group capacity, and dropped calls only at the global admission cap.
3. Combined report sizing needed stronger proof: the initial 512-group design was reduced to 256, but the 256-group fixture still did not maximize counter widths. A separately generated image/icon stream passed both parsers and matched counts yet raised the conservative combined fixture to 1,051,443 bytes, exceeding unchanged 1,048,576-byte report cap. Resolved: final group capacity is 128, and the reviewer replaced the assigned integration test with an independent per-field serialization envelope. Every image/icon numeric field is maximized independently, including repeat/nested/change counters, host ordinals and unknown metadata. Existing streams use conservative row/field/digit bounds, stacks use all 96 frames across entry plus eight samples, and 16 KiB is reserved for remaining report fields. This deliberately overestimates physically consistent records; it is not presented as a valid capture. The resulting envelope is 1,027,515 bytes, leaving 21,061 bytes below the unchanged 1,048,576-byte output cap. The 2,097,152-byte wire cap also remains unchanged. The writer continues to fail closed on excess output.

## Tests independently executed

- Icon synthetic suite: 7 tests PASS (real GLib/GObject property/lifecycle, synthetic GTK signatures, no GTK GUI).
- Icon adversarial parser: 15 tests PASS.
- Owner: 14 tests PASS.
- Existing leaf: 10 tests PASS.
- Existing image: 12 tests PASS.
- Integrated overlay/controller/restoration suite: 60 tests PASS; includes subordinate suites.
- `git diff --check`: PASS.

Test totals overlap because integration executes subordinate suites. These are not native or hosted capture results. Final 128-group module/parser, owner suite and 60-test integration suite were rerun after correction and passed. The independent reviewer edited only the assigned combined-size test function; all runtime changes were implemented by the implementation/integration workers.


## Remaining limits

This approves neither publication nor native acceptance. No full GIMP native build, GTK GUI run, sanitizer/LSan completion, actual pointer-debugger capture, performance win or causal fix was established. All timing remains diagnostic and includes instrumentation/debugger effects where applicable. Missing theme/style/resolved-resource identity prevents interpreting an equal request tuple as equal rendering work. Snapshot membership and ancestor categories retain C42's coverage limitations. Synthetic GTK structures/signatures do not prove the installed GTK ABI; real GObject lifecycle/property behavior is exercised separately.

The report-size test is an intentionally conservative numeric/shape envelope plus explicit reserved metadata space, not an assertion that such simultaneously maximal metrics can happen. It verifies output capacity without expanding capture budgets. Revisit it if record fields, descriptions, stack limits, or stream counts grow.

Restored repository history may remain incomplete as stated in the restoration proof, and the gimp-data submodule was not initialized. This review relied on exact restored target-tree bytes, not historical completeness.

## Final reviewed SHA-256

- `tools/brush-ci/readiness-owner/icon.inc`: `5b125b3b4bdb2032faee02b87c310a578d6b9c196af4f5ce2062a2b22d1f484f`
- `tools/brush-ci/readiness-owner/icon_records.py`: `9d06335a3061b719df7836416363c1f6a9d0eed373e200b976e12c5d804c27b9`
- `tools/brush-ci/readiness-owner/synthetic-icon-prefix.c`: `5a82e5d22cf4af8e167bdc52a172b4054f7f159753d785e557be882b95b3844c`
- `tools/brush-ci/readiness-owner/synthetic-icon-suffix.c`: `312a4f9ae2347a2a9cbd8b7f37f3a7be5f9bb87ec20e7918d464686ef52fc332`
- `tools/brush-ci/readiness-owner/test_icon.py`: `69b2a7f0a5684b7161b2c0143183470cb2af0caef8cd0bec24dd688e123af387`
- `tools/brush-ci/readiness-owner/test_icon_records.py`: `c348d7e2fb8e7cfc170f515747a6d99e0a3002561d74477a593181ed011b95b6`
- `tools/brush-ci/readiness-owner/collect.py`: `1abfeaa13dbd1de65e93e8cee3ed1bbd44f5943f3955261c097f0fc45bceea00`
- `tools/brush-ci/readiness-owner/generate.py`: `a52932fe437e34588809a111f59a5c586a5b0a3a73381d99fd2d18db2885298e`
- `tools/brush-ci/readiness-owner/identity.py`: `2c07d7b3cece059efdc6dafbf51bc658730c11cc54842429fb2e0c0ee0ae6f2f`
- `tools/brush-ci/readiness-owner/leaf.inc`: `e4b0f2e01556f6e83d76645f221ebc5eb861d7332dd25f23ad80944deafcfd2b`
- `tools/brush-ci/readiness-owner/meson.append`: `2033cda92f8e864ae8fa0f53103181c662ffd8583c6e2b4792f2167b5508f8ea`
- `tools/brush-ci/readiness-owner/image.inc`: `fd79b1fa80ad0be75d6ef8d9bbdcd251edfeb01dc497f8cb9576face93ee8ede`
- `tools/brush-ci/readiness-owner/test_owner.py`: `aed47aca43dd424c09a7da1e2f946093496e3b7800ea00286e165c6017ae10f9`
- `tools/brush-ci/test_readiness_owner_integration.py`: `c0f820a69298bc55536cf7b0755a396dceabd9df3a65030183a84ab4b3977941`

Locally reviewed upstream-source file identities:
- `gtkimage-3.24.49.c`: `34a3a1c41ce9f18f156323db75ebfaaff5251a283338762637fd47041d102af5`
- `gtkiconhelper-3.24.49.c`: `1f08f1d0d979f6d6893ea32cac2756108d78e4eef129a9a7de0b6031a710035c`
- `gtkwidget-3.24.49.c`: `2d36a4a7a71119fc38f1b939c03517eb0a37349458d50d23d726283c1c06acaa`
