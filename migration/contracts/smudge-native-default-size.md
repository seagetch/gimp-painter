# Dedicated Smudge: neutral dynamics and native default size

Scope: 23.004/native-default-size is a limited child of the open original 23.004
size/repositioning obligation. It does not establish arbitrary transformed
accumulator footprint compatibility, UI/profile persistence, high-precision
support, or new old-runtime/hardware evidence.

The dedicated Painter Smudge renderer already derives its scale from pinned old
brush dimensions. Its options deliberately do not enable the generic
`painter-legacy-brush-geometry` flag. Two shared providers must nevertheless use
legacy semantics for this dedicated options type:

- Empty smooth neutral dynamics curves use the captured old cubic sample
  rounding, just as explicitly legacy ordinary paint options do. Curves with
  explicit points and free curves keep their own samples. Shared dynamics are
  not mutated and standard Smudge continues to use modern neutral mapping.
- Native brush-size reset, selection and linked radius changes use the old
  generated dimensions (or unchanged bitmap dimensions), matching the dedicated
  renderer's scale denominator. The generic geometry flag stays false.

The native regression cases live in `gimp-painter-smudge`: `default-dynamics`
and `native-default-size`. They compare six linear outputs over 1030 inputs each
against 256 immutable old-runtime samples, check explicit curves and standard
Smudge isolation, compare 72 generated default sizes to the pinned old source
formula, exercise linked radius selection/change/unlink, and run real dedicated
strokes to verify unit/default-dynamic scale and cancellation.

Generated size expectations are source-derived, not new captured old runtime.
The old commit is `afa43fae3e920210146abed514f136fd49f671b5`.
