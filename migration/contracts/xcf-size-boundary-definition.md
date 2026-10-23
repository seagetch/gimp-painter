# Compression, offsets and size boundaries (10.015)

This defines the division between the metadata compatibility decoder and native
XCF loading. Source-consistency checks are not proof that every compressed or
malformed file loads. Refer to the byte-decoder, Open integration, multipart and
adversarial-recovery contracts for their separate execution evidence.

## Ownership of decoding

- The pure compatibility module parses immutable metadata, property sequences,
  offset tables and bounded Filter/Clone records. It does **not** decompress
  tiles, create GEGL/GIMP objects, traverse pixel hierarchies or prove pixel
  decodability. Its candidate is not authority to choose a dialect by itself.
- Native `xcf-load.c` performs selected-dialect object/pixel construction and
  tile decoding. Compression codes are 0 none, 1 RLE and 2 zlib. Code3 fractal
  is recognized by the property parser but explicitly unimplemented by the tile
  loader; recognition must not be described as supported decompression.
- Compatibility metadata budgets are work limits. Exhaustion is inconclusive,
  not negative evidence that a file must be the other dialect. The application
  retains explicit recovery routes instead of guessing from a short scan.

## Offsets and arithmetic

Wire integers are big-endian except the separately specified extension payload.
Old Painter v0–4 uses four-byte offsets and no precision word. Standard v0–3
uses four-byte offsets without precision; standard v4–10 adds precision and
retains four-byte offsets; v11+ uses eight-byte offsets. The current pure decoder
audits standard versions through23; later unsupported versions do not become
legacy merely because modern decoding is unavailable.

Nonzero referenced offsets must be inside immutable source bounds with room for
required words before narrowing to host size. Length checks use subtraction
against remaining ranges; record lengths are not trusted allocation sizes.
A following tile-table address is **not** a monotonic end boundary: native tile
loading supports backward/aliased/interspersed addresses using a bounded encoded
read and exactly expected output pixels. Pure metadata acceptance does not claim
to validate tile cycles, compression completeness or pixel allocation products.

## Distinct limits (do not substitute one for another)

| Scope | Default/defined bound |
| --- | --- |
| Pure candidate metadata | 100000 records,16384 objects,1MiB per string,64MiB per property/extension,256MiB aggregate byte-work including revisits |
| Whole input snapshot for application Open | Explicit storage override `G_MAXINT64` bytes, subject to platform/file/I/O resources; not the metadata transport limit |
| Logical regenerated extension transport | 64GiB, from65536-byte blocks ×1048576 chunks; up to3 semantic transports per owner, no recursive manifests |
| Physical multipart property | Standard32-bit XCF property length; chunks are separately framed, not one oversized property |
| Owner-local transport input work | 6×(1048576+1) inner records including optional suppression carriers; does not increase logical byte limit |
| Ordinary inline-to-multipart threshold | 1MiB logical capsule; crossing selects multipart, not loss of semantics |
| Internal inline-only opt-out / native name materialization | 256MiB minus4096 bytes (`max_blob`), not the whole-file acceptance bound |
| Typed arguments | Depth32 and65536 aggregate slots/references; aggregate mutable string/array materialization256MiB |
| Semantic dictionary keys | 65536 field-work bound; duplicate keys separately refused |

The storage serializer otherwise has no semantic node/depth cap by default;
callers and typed codecs enforce their own limits. Snapshot backing uses private
file-backed immutable bytes, not a promise to materialize a64GiB capsule in RAM.
Limit/I/O/cancellation failures cause explicit errors, inert retention or
preflight refusal as specified at that layer, never a fabricated empty payload.
Neither “256MiB” nor “64GiB” is a universal XCF file-size limit or a dialect test.
