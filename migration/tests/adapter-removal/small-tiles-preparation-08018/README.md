# Small Tiles checked C preparation

Bounded successor to 08.018/blinds-procedure-preparation and its unchanged
historical proof. See migration/contracts/filter-small-tiles-preparation.md.
The original void setter, unknown-name clearing, arbitrary transforms and other
procedures remain outside this child; the canonical C-entry obligation stays
PARTIAL and parent08.018 stays open.

The real C expected-red is retained: the original native checked entry rejected
`plug-in-small-tiles` as unsupported (1.23s SIGABRT). It was a runnable test with
the real restored application, not a compiler/setup failure.

The implementation shares the existing staging and final publication guard,
with compile-time-reviewed four/seven-slot layouts. New tests cover four typed
zeros, exact shapes, independent scalar ownership/conversion, ignored context
payloads, finite/type rejection, missing trusted provider, both named typed
handle delegates, and transform-time caller mutation/replacement/close/provider
loss/corrupt destination. Existing Blinds and all earlier entry tests remain.
Its unsupported-name negative matrix now uses still-unsupported Retinex instead
of newly admitted Small Tiles; it is not weakened to accept arbitrary names.

The genuine-old live fixture loop is the existing Small Tiles oracle with only
its definition installer changed to the checked C entry, and expected saved
context placeholders changed to zero under the preparation contract. All25
scenes and50 expected output comparisons remain. Fixture bytes are not regenerated.
The archive and all1107 members were verified against the immutable provenance;
the323 live inputs are pinned in fixture-provenance.json. No new legacy runtime
capture or user-skipped Edge execution is claimed.

The standard Meson registration materializes verified Small Tiles fixtures via
run_small_tiles_entry_fixture_test.py before invoking the original target.
It retains the120-second suite budget and does not rely on a manually supplied
unverified directory. Sanitizer execution uses the same verified fixture wrapper.

No new UI caller is introduced. Tests run on the authenticated portrait Xvfb
1080x1920 environment; this is a C-entry/owner/model/pixel task, not an additional
GUI layout or installed-prefix claim. Instrumentation is bounded and enumerated;
RTTI-only sources, other GIMP/dependencies and the isolated execution helper are
not asserted to be sanitizer-instrumented. LSan remains disabled.

## Verified checkpoint

Normal44 PASS58.53s; bounded ASan/UBSan/float-cast-overflow44 PASS54.85s.
Both include all50 original output comparisons; original120-second budget retained.
Exactly46 source units are instrumented and38 are RTTI-compatible-only.
Normal100-input and sanitizer101-input immutable snapshots agree on all common
builder inputs. `validate.py --check-owned-source` verifies live owned sources.
Unrelated independently evolving main-tree differences are explicitly listed in
checkpoint-differences.json; `--check-source` is the stronger whole-checkpoint
comparison and is not claimed to pass on a later integrated tree.
The wrapper forwards both success/failure and replaces an unverified inherited
fixture directory with its own verified temporary corpus (three controls).
