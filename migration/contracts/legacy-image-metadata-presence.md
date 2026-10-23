# Legacy image metadata presence ledger (10.017)

`../inventory/legacy-image-metadata-presence.tsv` records all image-property
calls in the pinned old `xcf_save_image_props`, header/table data, conditional
presence, representation and absence limits. It supplements the existing
`xcf-wire-records.tsv` and `xcf-field-acceptance.tsv`; it does not replace their
runtime coverage/remaining columns or claim new old-writer execution.

Source pin: `afa43fae3e920210146abed514f136fd49f671b5`. The ledger covers all12
image property kinds including END, plus profile/comment/grid parasite roles
and absence of a modern precision header word. Persistent-only parasite
filtering is decisive: a nonempty transient-only in-memory list is not evidence
that a PARASITES property is emitted. ICC profile is the named `icc-profile`
parasite, not an independent numeric property; absence is not an embedded default
profile. Grid uses `gimp-image-grid`, not an invented `gimp-grid` identifier.

Resolution and tattoo state are always emitted. Built-in/user units and
compatible old paths/vectors are alternatives selected by explicit writer
predicates. Guides, sample points and colormap are conditional. Source-based
absence/default descriptions are not independently mutated-file observations.
An arbitrary persistent parasite may hold other metadata; lack of a dedicated
old property does not prove arbitrary parasite contents were impossible.

The verifier checks complete writer-property coverage, pin/source hashes,
persistent filtering, profile/comment/grid names and the existing wire ledger.
It proves this definition is source-grounded, not every old metadata value's
successful modern restoration. Ordering/duplicates are the following task.
