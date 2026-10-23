# Original WBS 08.001: native type registration before loading

Cold name-based Filter argument decoding rejected the literal saved types
GimpCloneLayer, GimpFilterLayer and GimpPerspectiveGuide immediately after
gimp_new, before any feature instance had been created. The reproduced failure
is retained in baseline.json. gimp_init now ensures these three native GTypes
immediately after enum registration and before units, factories or configuration
loading. This registers types and ancestors; it does not create layer/guide
instances, C++ BindingStore slots, background jobs or a GTK display.

The native startup fixture asserts all three names are absent before gimp_new,
then exercises both legacy GValue and current snapshot decoders using only
literal names and typed null references. Declared types and reference metadata
survive. An unknown name still fails and is not registered by input data.
Existing XCF loaders can warm the same types through constructors or type checks,
so the reproduced cold-decoder defect is not evidence that every first XCF load
failed. Full persistence behavior and feature lifetimes retain their own gates.

The six assigned source obligations are traced to exact pinned legacy header
blobs in source-mapping.json. Their inventory source_scope fields are surrounding
context anchors, not necessarily the added declarations. No assignments are
removed or moved. ToolItem and PaintInfo register through the native container
and paint registries. ToolGroup's existing loader registers the needed types
before name-based parsing; a cold invocation successfully loads a synthetic
group. The old MypaintInfo declaration and commented option prototype do not
establish a separate runtime type to resurrect. The old brush/JSON factories map
to native PainterMybrush/LayerPreset factories, whose registered child types and
type-based lookup are checked. A real GimpRc, instantiated without loading files,
deserializes all four synthetic resource-path properties.

Startup6, Clone37, Perspective9 and eight selected Filter regression cases pass
in current native app-test executables. Nineteen source seals are unchanged
during the run. The app Meson target compiles the new C++ fixture alongside the
existing C test utilities; the existing C application sources remain C. The
fixture pairs constructed's contexts explicitly because it stops before normal
configuration loading and exit/save processing. It uses a private process cache.

The historical failing report used an earlier three-case fixture; its original
hashes and bytes are preserved rather than relabelled as the final six-case run.
The first four-case success was expanded with actual ToolGroup and GimpRc loads;
the final reports above are the current acceptance evidence. The existing class
behavior checks cover this early registration change, not full application,
all-platform, tablet, sanitizers or all saved-file compatibility. UI-only native
types continue to register in their owning GUI/tool setup before their loaders;
this change does not eagerly initialize every historical C++ type name.
