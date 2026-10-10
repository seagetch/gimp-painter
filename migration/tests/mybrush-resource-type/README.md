# Original WBS 08.005: native MyPaint resource and data factory

GimpPainterMybrush is a native GimpData subclass with GimpTagged and a typed
BrushSlot in the common BindingStore. GIMP owns its native loader factory;
gimp_get_data_factory resolves the extended resource type independently from
upstream GimpMybrush. The new, standard, writable .myb loader, configured paths,
restore/save/clear/exit hooks were already connected. This task restores the
three public class macros omitted from the current resource header.

The new native regression calls gimp_data_factory_data_new on the actual GIMP
factory. It verifies the registered direct parent and struct sizes, all class
macros, native data type, configured new callback, inherited name/MIME/extension,
immediate .myb save, clean state and strong-container membership. Deleting the
unselected resource through the factory deletes its file and destroys its sole
container-owned object, verified by a weak pointer. A second case retains the
resource across deletion, reads its owned settings successfully after removal,
and verifies finalization on the last caller unref. The standard callback returns
a stable, internal, clean resource outside the ordinary container. This borrowed
process-global singleton follows the standard-resource convention and is not
expected to die when the factory container is cleared.

Both factory cases and all four native resource cases pass with 22 current source
seals. The existing factory case refreshes all 177 pinned resources, duplicates
one, edits it, saves/reloads/deletes it, checks retained extension data and confirms
that the ordinary upstream factory is unaffected. The resource suite loads and
copies all 177, exercises icon/stream persistence, closed owner and copy finalizer
reentry. Six named tests therefore do not imply six brushes or all engine parity.
No new sanitizer or non-Linux run is claimed. Earlier evidence remains unchanged.

The exact old resource cpp/header/private-header and five core initialization
hunks map to their present type, loader and factory contracts. Whole-file anchors
close only this original type/factory acceptance; their engine, preview and full
persistence obligations remain separate.

One incorrect assignment was corrected before acceptance: old gimp.h source166
adds only GimpMypaintInfo *standard_mypaint_info, while source167 adds the actual
GimpDataFactory *mypaint_brush_factory and factory_table. Only task08.005 moves
between those source IDs. Existing completed type duties on source167 and all
other obligations remain unchanged. Source166's eight remaining broad routes are
still unresolved; this task does not accept them or assert a whole-repository
dead-member proof. The routing rule, both hunk tables and derived work/granularity
tables are consistent. The exact removed/added IDs and source payload hashes are
recorded separately from the nine accepted source-specific duties.
