# Seven custom modes: byte formula and source provenance

Checkpoint for original tasks 13.001 and 13.013, **not closure of section 13**.
Pinned old source: `afa43fae3e920210146abed514f136fd49f671b5`.

## Representation and exact arithmetic

The old RGBA path uses **straight/unassociated nonlinear byte RGB and alpha**.
RGB is not premultiplied before these functions. Let A be backdrop, B source,
M mask (255 when absent), O effective byte opacity. Define:

- `mul(x,y)`: `t=x*y+128; ((t>>8)+t)>>8`.
- `mul3(x,y,z)`: `t=x*y*z+0x7f5b; ((t>>7)+t)>>16`.
  Preserve the old approximate macro, not a mathematically exact division.
- `div(x,y)`: `x/y + (x%y > y/2)`, unsigned integer division.
  Exact half rounds downward, not the usual half-up convention.
- Replace R(A,B): `alpha=mul(B.a-A.a,mul(M,O))+A.a`.
  If alpha is zero retain A RGB. Otherwise
  `ratio=div((M*O/255)*B.a,alpha)` (division **before** multiplication by B.a),
  `delta=div(abs(B.c-A.c)*ratio,255)` and `out.c=A.c +/- delta` toward B.c.
  The resulting channel is assigned to a byte, exactly as in old code.

Negative `mul` operands occur in Replace alpha. Signed arithmetic right shift
is the historical GCC/Linux behavior retained by this compatibility path;
this is not a portable mathematical floor-division implementation. RGBA input
and opacity are bytes. The source functions do not add a floating-point clamp
or premultiply/unpremultiply stage. They retain hidden RGB at alpha zero.
The GEGL boundary separately clamps finite inputs to [0,1], quantizes nearest
byte (`floor(v*255+.5)`), and maps non-finite values to zero. Higher precision
input to this compatibility operator is deliberately byte-quantized; that does
**not** establish a high-precision extension or close 13.012.

## Per-mode formula / source operand order

| Raw | Mode | Old function and result |
|---|---|---|
|23|Erase|`erase_inten_pixels`: A RGB; `A.a-mul(A.a,mul3(B.a,M,O))`|
|24|Replace|`replace_inten_pixels`: R(A,B)|
|25|Anti-Erase|`anti_erase_inten_pixels`: A RGB; `A.a+mul(255-A.a,mul3(B.a,M,O))`|
|26|src-in|`gimp_composite_src_in_any_any_any_generic`: C=B, C.a=mul(A.a,B.a); then R(A,C)|
|27|dst-in|Swap A/B **before** the src-in intermediate and Replace|
|28|src-out|`gimp_composite_src_out_any_any_any_generic`: C=B, C.a=mul(255-A.a,B.a); then R(A,C)|
|29|dst-out|Swap A/B **before** the src-out intermediate and Replace|

Original files: `app/paint-funcs/paint-funcs.c` (three functions, `INT_DIV`,
`layer_modes` reverse_source column, and `combine_sub_region` operand exchange);
`app/paint-funcs/paint-funcs-utils.h` (macros);
`app/composite/gimp-composite-generic.c` (IN/OUT intermediate).
The IN/OUT old prose comments misleadingly say dst; function names plus
reverse_source dispatch are authoritative. DST zero opacity retains the swapped
backdrop; substituting generic Porter-Duff DST operations would change this.

All seven currently route to `gimp:painter-legacy-mode`, with separate appended
Painter enum IDs, nonlinear RGB composite space, union affected region, and
straight `R'G'B'A` values. Existing `legacy-layer-modes.md` describes source extent,
bottom-node initialization and the projection/paint opacity scaling difference.
Neither the scalar oracle nor this formula inventory proves group isolation,
pass-through ordering, UI labels, high precision extension, or artwork match.
Those remain 13.009–13.014 integration gates, with original dependencies retained.

## Independent source oracle

`migration/tests/legacy-seven-formulas/run.py` extracts the **unchanged function
bodies** and macro header from the pinned Git object. It supplies only the RGBA
context shim and original routing, then compiles the current production helper
and kernel bodies unchanged beside them. No legacy application, Edge/string
runtime, GUI, or shared native build is executed. SHA256 of original full source,
extracted bodies, production source and test harness are in each report.

Per mode: all 65,536 alpha pairs × nine opacity boundaries × seven mask states,
plus 100,000 deterministic full-RGBA cases, each of those also testing both
output aliases. Total **29,601,376 exact RGBA comparisons**, plus 1,400,000 alias
checks. Optimized and ASan/UBSan reports pass; leak checking is disabled. This
supplements (does not replace) existing genuine old-app 56 custom-mode scenes
and native projection/paint-route checks under task 10.005. It is numeric/source
proof, not a UI screenshot or a new full-app run.

Run from an environment providing compiler and glib pkg-config:

```
python3 migration/tests/legacy-seven-formulas/run.py --report /tmp/seven.json
python3 migration/tests/legacy-seven-formulas/run.py --sanitize --report /tmp/seven-asan.json
```
