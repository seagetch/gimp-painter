# Original 23.001: independent blending-output specification

Acceptance is **「Flow/Rate と区別した仕様を記録」**, not completion of all Smudge runtime, UI or platform work. The 52 original source assignments are individually retained in `source-duty-review.json` with original identity/criterion and specification section. `old-source/` contains exact pinned `afa43fae3e920210146abed514f136fd49f671b5` blobs; no old application is run by this proof. Subsequent 23.002–23.015 implementation/verification obligations remain separate.

## Channels and persistence

Legacy enum order is Opacity0, Size1, Angle2, Color3, Hardness4, Force5, AspectRatio6, Spacing7, Rate8, Flow9, Jitter10, **Blending11**. Both Flow and Blending exist: they are not alternate names. The historical enum description typo is `Bleding`; stable name/nick is BLENDING/`blending`. `GimpDynamicsPrivate::blending_output` is a distinct owned output, configured as the aggregate `blending-output`, initialized with output type11, returned by the accessor and property getter, copied through the existing output-copy machinery in the setter and unrefed at finalization.

Smudge options have `rate` in [0,100], default50, and `use-color-blending`, defaultFALSE. The latter is a stored Boolean gate, not a numeric flow control. Get/set operations retain the same field/property distinction. For each dab, the independent dynamics evaluator receives the same coordinates, paint options and fade point for Rate and Blending. There is **no FLOW output lookup** in the old Smudge source.

## Accumulation

Let I be sampled drawable bytes and A the previous accumulator. Effective rate is r=(rate/100)*D_RATE; q=ROUND(255*r). The conceptual expression A'=r*A+(1-r)*I is insufficient to reproduce bytes. For no-alpha Y/RGB, each channel is

    A'c = Ic + trunc0((Ic*(255-q) + Ac*q - Ic*255)/255).

`trunc0` is signed C integer division toward zero, not floor and not rounding the whole weighted sum. For YA/RGBA, u=(255-q)*Ialpha, v=(q+1)*Aalpha, s=u+v. If s=0, all output bytes are zero; otherwise

    A'c = Ic + trunc0((Ic*u + Ac*v - s*Ic)/s)
    A'alpha = s >> 8.

The q+1 and division by256 in alpha are intentional old arithmetic, including endpoint quirks: q=0 is not a mathematically exact sample replacement for alpha images. Do not normalize this to float lerp or silently repair it.

## Independent foreground shading

Only when `use-color-blending` is TRUE: b=D_BLENDING (initialized multiplier1), k=ROUND(255*b). Foreground F is converted through the image/drawable color API, with paint-area alpha forced opaque. `shade_region(A',B,F,k)` writes **separate output B**, never feedback into A'. For no-alpha:

    Bc = floor((A'c*(255-k) + Fc*k)/255).

For alpha: u=(255-k)*A'alpha, v=(k+1)*Falpha, s=u+v; if s=0 all bytes zero, otherwise

    Bc = A'c + trunc0((A'c*u + Fc*v - s*A'c)/s)
    Balpha = s >> 8.

Again k=0 can contribute foreground through the alpha +1 weighting. When disabled, the output source is A' directly. No-alpha output gets opaque alpha added; alpha output copies its existing alpha. Brush hardness/mask, dynamics opacity (zero exits before accumulation), context opacity and incremental native Replace composition happen afterward; neither Rate nor Blending is an alias for those controls.

## Footprint, allocation and lifetime

Active old start/motion code uses `ignore_scale` to omit only the Size dynamics multiplier while obtaining accumulation geometry, then resets it on the successful path (early-return cleanup is not inferred from this source). Other dynamics are not globally disabled. The retained footprint starts from ceil(sqrt(transformed_width²+transformed_height²)), with a one-pixel border. The initial radius is held, while the actual dynamically transformed brush crops the paint subwindow; the prior canvas pointer is restored after replacement. Clipped initial regions are prefilled from the clamped brush-position pixel before copying the available drawable. `accum_data` and optional `blending_data` are separate allocations, initialized NULL, and freed/reset on finish/finalize. `max_radius` is reset at start. Old `accum_size` and accumulator helper branches enclosed by #if0/#else of #if1 are **inactive historical code**, not an additional active algorithm to recreate. Header declarations, disabled fields and the whitespace-only hunk000988 remain traced; they introduce no separate formula.

These details specify dependencies of the expression but do not prove current allocation/size/lifecycle equivalence. Their implementation duties remain under 23.002/004 and tool lifecycle gates.

## Contrast with standard modern Flow

The exact current comparison source is separately frozen in `modern-smudge.c`. Standard Smudge obtains flow=(options.flow/100)*D_FLOW, supports pixmap paint and no-erasing behavior, and passes Flow and Rate into `gimp_gegl_smudge_with_paint`. Its conceptual paint=(1-flow)*Accum+flow*BrushColor does not imply equivalence to the legacy byte formulas, separate Boolean gate, named stored aggregate or alpha rounding. Dedicated Painter Smudge may share surrounding native infrastructure; it must not rename Blending to Flow or alter standard Smudge.

## Executed proof and limits

The specification's independent integer model compares accumulation and shade bytes for all 1,024 preexisting pinned old-runtime cases (four channel layouts × every byte weight, 57 pixels each). It checks both stages separately, including full hidden/alpha bytes; composition is retained in the original fixture but not relabeled as newly tested here. Causal negative models remove alpha +1 or replace signed truncation with floor; both must disagree. A two-dab test distinguishes separate shading from feedback. Source-order/name checks establish Rate then accumulation then independent Blending/shading then final replacement, and verify no Flow alias. No new GUI, sanitizer, physical-device, high-precision or full-port claim.
