# New Painter XCF identifier/version registry (10.011)

Acceptance is the **definition** of noncolliding identifiers and versions,
not complete save/load or cross-version application compatibility. The detailed
existing wire schemas remain [v1](xcf-painter-v1.md) and
[multipart transport](xcf-multipart-transport.md).

## Standard property namespace

All extension records are ordinary XCF `PROP_PARASITES` (**21**) records. No new
property number is allocated and legacy Painter property numbers 32/33 are not
repurposed. Standard XCF header/version and ordinary properties retain standard
meanings; an extension's capsule version is not the XCF file-header version.
The standard save version policy remains described in the v1 contract.

## Parasite names and payload signatures

`00` below denotes an actual zero byte; bytes are exact, not host integers.

| Name / reserved prefix | 12-byte payload prefix (hex) | Version meaning |
| --- | --- | --- |
| `gimp-painter-image` | `475058434600000001000000` | Logical v1 image dictionary |
| `gimp-painter-item` | `475058434600000001000000` | Logical v1 item dictionary |
| `gimp-painter-origin` | `475058434600000001000000` | Logical v1 origin archive |
| Same three names, multipart form | `475058434600000002000000` | Transport manifest v2; reconstructed logical capsule remains v1 |
| `gimp-painter-chunk-v1/` | `475058434300000001000000` | Chunk framing v1 |
| `gimp-painter-inert-v1/` | `475058434900000001000000` | Inactive-record identity carrier v1 |

Semantic dictionaries are canonical little-endian GVariant `a{sv}` and must
contain `version = uint32(1)`. The image dialect marker is exactly
`gimp-painter-standard-v1`. Persistent flag is 1. Names, flags, magic, dictionary
type/version and relevant owner/namespace identity must agree; a similar name
or a version byte alone is not authority to interpret arbitrary data.

Unknown versions/invalid dictionaries and user records occupying reserved names
are retained inert or cause preflight refusal according to the schema contracts;
they are not silently overwritten or guessed. No global claim that user-created
parasite names can never coincide is made. The guarantee here is **no collision
with standard numeric property meanings**, with explicit versioned named payloads.
The `v1` in a chunk/inert name is that framing's version, not semantic dialect
selection or the XCF header number.

## Evidence boundary

`migration/tests/xcf-extension-identifiers/verify.py` checks exact implementation
constant bytes/names, the standard enum, dictionary version/dialect, and native
writer framing. It is a source-consistency check, not a newly executed native
save/reopen test. Changing an identifier requires updating this registry and its
check deliberately; later record-specification and full-save tasks remain separate.
