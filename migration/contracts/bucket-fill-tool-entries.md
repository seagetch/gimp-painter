# Original BucketFillBrush C names over the native FillBrush provider

The exact original function signatures are restored:
`GType gimp_bucket_fill_brush_tool_get_type(void)` and
`void gimp_bucket_fill_brush_tool_register(GimpToolRegisterCallback,gpointer)`.
The original include path, ordinary typedef names and type/cast/class macros
are source aliases to the single native GimpFillBrushTool/GimpFillBrushToolClass.
There is no second GType, instance store, tool controller or registration model.
The runtime type remains **GimpFillBrushTool**, not GimpBucketFillBrushTool;
historical struct tags/layout/subclass ABI are not restored by these aliases.

The original get_type forwards to the existing native lazy GType provider.
Shared TypeTraits and BucketFillBrushToolRef use the sole original C entries;
there is no local competing specialization or C++ registration implementation.
The handle adopts/retains native nonfloating tool ownership explicitly and
rejects wrong-type ownership without consuming the caller's reference.

Both register spellings share the native callback provider. It forwards borrowed
opaque user data exactly once, contains callback exceptions, and treats a NULL
callback as an inert call. Recursive registration callbacks are synchronous and
do not share mutable registration state. Native identifiers/options, context
mask, labels, accelerator, help and icon metadata are unchanged.

## Original behavior still owned by a successor task

`08.018/bucket-fill-original-registration` retains the original criterion:
require Gimp user data, ignore the supplied callback, register the paint core,
construct tool info with the original options class and metadata, set visible
and default-visible, install both ordinary and horizontal options GUI callbacks,
and add the tool through the original registry semantics. The original type name
and class-layout differences must remain explicit. This child does not silently
remove any of those obligations by choosing a native alias. Its canonical
register map row remains PARTIAL; the get_type row proves only the callable
native registration route. Parent08.018 is not closed here.
