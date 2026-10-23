# Original consuming tool-info registration

Original source oracle: `afa43fae3e920210146abed514f136fd49f671b5`,
`app/tools/gimp-tools.c:698` and `app/tools/gimp-tools.h`.
The original public operation is exactly
`void gimp_tools_register(Gimp*, GimpToolInfo*)`: validate both native types,
add to the application's sole tool-info container, then consume one supplied
reference. It is not the modern private many-argument registration callback.

The native private callback is renamed `gimp_tools_register_internal`; its
existing 50-provider fixture catalog, metadata, ordinary/horizontal GUI callback
selection, and native startup array remain unchanged. It now publishes through
the restored public consuming entry. A local tool-info lease covers synchronous
observers; the standard paintbrush is assigned only if the original registry is
still current and still contains the resource. No second registry or store exists.

Public preconditions additionally require a live native registry. Invalid
arguments do not consume the reference. Once validated, one reference is consumed
even when a duplicate native addition is rejected, matching original ownership.
Application and original container leases protect synchronous observer removal,
nested registration, container detachment, and caller-last-owner release. This is
owner-thread native GObject behavior, not a new cross-thread contract.

Tool-info ownership is not copied from the paint-info assumption: freshly made
options hold both a GimpContext tool reference and GimpToolOptions tool-info
reference. Native container removal clears the context reference through
`gimp_context_tool_removed`, leaving the explicit options cycle. Tests assert
that remaining reference, hold a temporary disposal lease, dispose the options
cycle, and observe finalization. Invalid/unpublished objects retain the original
caller reference plus both options back-references.

Three native tools intentionally have no options GUI callback: Offset, Threshold,
and Brightness-Contrast. The catalog oracle checks exactly those exceptions,
50 unique registered tool identifiers, valid tool GTypes/options, the standard
paintbrush, and existing FillBrush/Perspective Guide registrations. It does not
claim to render or complete every options panel.

The tenth test preserves a newly exposed generic-container regression: removing
the just-added standard paintbrush from an add observer while startup is frozen
must leave 49 tools and no standard tool. Net child-count change is not proof of
parent-vfunc chaining. That independent container fix is a prerequisite for closing
this bridge task; it is now independently implemented by the container-reentrant-membership and tool-options-retirement tasks. Both intermediate failed integration receipts are retained, not excluded.

This prerequisite does **not** complete old BucketFillBrush registration: the
old callback-ignored/Gimp-userdata semantics, paint-core registration, original
labels/help/shortcut/visibility, options identity, and both GUI factories remain
owned by `08.018/bucket-fill-original-registration`.
