# Legacy attribute ordering and duplicates (10.018)

These are **source-derived old-reader rules**, not freshly executed arbitrary
permutation tests. The pin/hash and exhaustive image/layer/channel case inventory
are in `../tests/legacy-property-order-definition/rules.json`. Rules apply to
safely decoded records; they do not promise successful recovery of malformed
lengths, invalid types, or unsupported old callbacks.

## Sequential interpretation, not universal last-wins

The old reader reads type/size, dispatches immediately and repeats. END returns
at once; later bytes are not another property of that owner. Known fixed-width
handlers consume their expected words, not necessarily the declared size;
variable handlers have their own size/terminator rules. Thus sorting properties
or skipping every known field by declared length changes historical behavior.

- Image scalar values (colormap, compression, resolution, tattoo state, built-in
  or user unit) use successive handler assignments, subject to validation.
  The later successful applicable value wins. Colormap applies only to indexed
  images; v0 substitutes grayscale. Invalid resolution uses template defaults.
  Built-in UNIT and USER_UNIT affect the same image unit, so their order matters.
- Layer opacity, visibility, linked/lock flags, offsets, mode and tattoo are set
  on the **current** layer object. Mask apply/edit/show, text flags, group flags
  and ITEM_PATH assign deferred outputs; the later decoded assignment wins.
  Lock setters may be conditional on current layer capability.
- Channel opacity, visibility, linked/content lock, show-masked, color and tattoo
  similarly update the current channel. Setter validation and structural
  replacement prevent treating this as unqualified last-wire-bytes-wins.
- ACTIVE_LAYER/ACTIVE_CHANNEL and FLOATING_SELECTION update shared loader
  pointers/offsets, including across owners in load order. Repeated marker tags
  do not create independent selections.

## Accumulation and named records

GUIDES adds accepted guides by prepending, then reverses the **entire current
list at the end of each property**. Repeated guide properties therefore accumulate
but also reverse the prior accumulated order; they do not simply replace or
append a block. Negative positions are skipped and invalid orientations diagnosed.
SAMPLE_POINTS appends accepted samples, so repeated blocks accumulate in order.
PATHS/VECTORS append decoded objects to the current vector container; each helper
then applies its active index to that accumulated container, not an isolated new
block. Invalid vector versions/lengths follow their own skip/recovery branch.

PARASITES are attached in wire order. The underlying name-keyed list removes an
existing identical name before inserting the new copy: later same-name payloads
replace earlier active values, while distinct names coexist. This old semantic
collapse is not permission for the new provenance archive to discard duplicate
original framing.

## Structural records are order-sensitive

GROUP_ITEM destroys/replaces the current layer with a fresh group, explicitly
copying its name and drawable type, and adjusting active/floating pointers.
It does not generically carry all earlier scalar fields into the new group.
A scalar before this record may therefore not survive, while one after it sets
the group. Repeated GROUP_ITEM repeats reconstruction.

FILTER_SPEC/CLONE_SPEC likewise reconstruct a layer, explicitly carrying name,
dimensions, offsets, opacity, mode, visibility and drawable type, then install
new procedure/arguments or pending source name. Other earlier object state is
not guaranteed copied. A later structural record determines the final class;
repeated/mixed structural records must not be normalized into a flat map.
SELECTION replaces a channel with a selection, moves its tiles, resets bounds
state and clears an active-channel pointer; it does not generically copy all
prior channel metadata. Deferred flags/path state remain separate outputs.

## Unknown and new-format distinction

The old default handler reads/discards `prop_size` bytes in bounded chunks and
continues; it does not semantically adopt or persist unknown payloads. The new
compatibility decoder instead retains ordered framed ranges (including repeated
unknown records and END), and application provenance archives raw sequences as
defined by10.014. New v1 dictionaries have unique keys; duplicates are refused,
not given the old scalar last-assignment rule. Namespace marker precedence is
separate and described by10.016.

The verifier ensures every old image/layer/channel case has a recorded rule and
checks supporting helper/source hashes and port ordered-record representation.
It verifies the specification, not runtime safety of every historical sequence.
