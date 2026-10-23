# Checked Retinex preparation entry

This bounded successor admits the exact `plug-in-retinex` name through the
existing checked C entry and its named FilterLayerRef delegates. It does not
restore the historical void setter or arbitrary PDB admission.

The original registration declares run-mode, image, drawable, scale, nscales,
scales-mode and cvar; the last is a PDB float, represented by a native double.
The reviewed native layout is six G_TYPE_INT values and one G_TYPE_DOUBLE.
NULL arguments produce typed zeros, not modern pspec defaults. Supplied arrays
must contain exactly seven GValues. Context slots 1 and 2 remain zero without
copying or retaining their payloads. All consumed scalars are staged before
callback-capable conversion. Finite float/double values targeting integers must
fit gint; the final double is not artificially restricted to the integer range.
Preparation preserves double precision and does not clamp rendering domains.

The scalar whitelist remains int/bool/float/double, with the exact run-mode enum
admitted only in slot 0. A whitelist member still needs a registered GLib
transform for its destination: boolean-to-double is unavailable and rejects
without changing the saved model. No substitute conversion is invented. Invalid shapes/types/nonfinite values fail before outer
publication. Provider metadata, owner leases, model identity/revision and binding
generation are rechecked after transformations and at publication. Reentrant
newer definitions and closure are not overwritten. A FALSE return after a
notification is not a universal rollback guarantee.

The historical void ABI, unchecked arbitrary transforms, unknown-name clearing,
derived GType identity and general procedure preparation remain PARTIAL. The
existing native Retinex implementation and isolated helper are unchanged. The
new C preparation route is compared with six genuine-old live scenes / twelve
output buffers, not described as a new run of all 174 isolated-helper outputs.
No new GUI caller/layout or full application sanitizer coverage is claimed.
