"""Reviewed routing contracts for the fixed gimp-painter delta.

Rules assign preservation/replacement work, never prove implementation or
behavioral equivalence. Exact hunk exceptions take priority over file families.
"""
from pathlib import PurePosixPath
import re

# Each profile names bounded leaf actions, followed by independent checks.
PROFILES = {}
def profile(key, feature, tasks, tests, reason):
    PROFILES[key] = dict(feature=feature, tasks=tasks.split(), tests=tests.split(), reason=reason)

profile('build', 'C/C++ module and link integration', '04.002 04.008 04.009', '04.015 04.016', 'Translate the actual source/library registration into Meson without compiling existing C as C++.')
profile('build-config', 'Dependency/compiler feature configuration', '04.007 04.008 04.010 04.012 31.003', '07.014 31.009', 'Carry required dependencies, compiler/link conditions and the optional HTTP boundary into the fixed Meson baseline.')
profile('cpp-header', 'C/C++ header/type adapter', '04.003 04.004 08.019', '04.014 04.017', 'Replace legacy Traits/cast glue with typed handles while retaining C declarations and C++ readability.')
profile('cpp-types', 'Typed handles and GType contracts', '06.005 06.025 08.019', '07.003 07.015', 'Retain class/interface distinctions and use a single checked typed-handle API.')
profile('cpp-registration', 'GObject registration and lifecycle adapter', '06.007 06.008 06.009 06.010 06.011 06.012 06.013 06.014 06.015 06.016 06.026 06.027 06.029', '07.004 07.005 07.006 07.010 07.015 07.016 07.017', 'Replace private Impl placement with BindingStore; preserve vfunc/property/interface contracts and contain exceptions.')
profile('cpp-values', 'C++ handle/value ownership', '06.001 06.002 06.003 06.004 06.005 06.006 06.020 06.020/value-assignment 06.021 06.021/array-reassignment 06.022 08.019', '07.001 07.002 07.003 07.011 07.013', 'Retain/adopt/sink, borrowed values and allocation families remain distinct; preserve synchronized mutex scope ownership; remove raw BoundMethod ownership ambiguity.')
profile('cpp-signals', 'Signal and source ownership', '06.017 06.018 06.019 07.008/untracked-signals 07.008/connection-name', '07.007 07.008 07.009 07.013', 'Own connections and closure data; detach before owner/target destruction and preserve block/after semantics.')
profile('cpp-scope', 'Scoped pointers and mutex ownership', '06.010 06.021 06.022 06.029', '07.013 07.016', 'Match deleters/allocators and release on early return or exceptions.')
profile('cpp-selection', 'Selection matcher string and array ownership', '06.021 06.029', '07.013 07.016', 'Preserve selection matching and owned GLib string/array inputs, including construction-failure cleanup; this helper contains no store-owned Impl or mutex guard.')
profile('cpp-gtk', 'GTK construction and shared UI adapters', '06.023 08.017 29.019', '07.008 29.020', 'Port construction/packing/ownership to GTK3 and common handles; retain all control behavior.')
profile('json', 'JSON value ownership and preservation', '06.021 28.002', '19.015 28.015 36.026', 'JSON helpers are live persistence dependencies; preserve unknown members, types, ordering semantics and error ownership.')
profile('pixels', 'Legacy pixel backend replacement', '09.001 09.002 09.003 09.004 09.005 09.009 31.017', '09.016 09.017', 'Retain stride/format/subwindow and multiregion semantics on GeglBuffer; do not port TileManager itself.')
profile('texture-loop', 'Periodic texture region and stride semantics', '09.005 22.001 22.002 22.005 31.017', '09.016 22.010', 'Translate closed-loop source sampling and subwindow stride without changing paper phase or mask order.')
profile('composite', 'Custom seven-mode compositing', '13.001 13.001/gegl-operation-route 13.002 13.003 13.004 13.005 13.006 13.007 13.008 13.010 13.011 13.012', '13.013 13.014', 'Preserve the actual alpha/color equations and mode routing, not legacy enum numbers.')
profile('mode-enum', 'Saved compositing enum and selection', '10.005 11.004 13.009 30.013', '13.013 36.004', 'Keep old mode numbers as wire values and convert explicitly; regenerate enum/PDB derivatives.')
profile('config', 'Persisted custom configuration', '30.006 30.007 30.008 30.010 30.011', '19.016 24.017 36.022', 'Preserve path/default/global resource settings, serialization and existing user-edited resources.')
profile('core-hooks', 'Core public helper and object contract', '08.018/core-extension-hooks', '36.011 36.012', 'Port the named helper or lifecycle hook individually, checking source ownership, coordinates, invalidation and failure cases.')
profile('container', 'Container names and hierarchy state', '08.018/core-extension-hooks 30.001/tool-group-state', '14.019 29.020', 'Retain unique-name versus duplicate-name deserialization and parent-change notification contracts.')
profile('editability', 'Custom-layer editability and Undo suppression', '14.006/editability-contract 15.006/editability-contract 16.021', '09.017 14.019 18.010', 'Replace removed item is_editable slot with an explicit current lock/operation contract, preserving user Undo and dirty state.')
profile('invalidation', 'Origin-aware stack/projection invalidation', '09.013/gegl-graph-invalidation 17.002 17.003 17.019 17.020', '18.002 18.003 18.017', 'Carry the originating item through update propagation so unrelated/self changes are excluded without losing lower-layer changes.')
profile('clone', 'CloneLayer source state and behavior', '08.002 14.001 14.002 14.003 14.003/gegl-source-node 14.003/clone-pickable-opacity 14.004 14.005 14.006 14.006/editability-contract 14.007 14.008 14.009 14.010 14.012 14.013 14.014 14.017 14.018', '14.019 14.020', 'Preserve source resolution, live reference behavior, coordinates, inherited operations and teardown on the new backend.')
profile('clone-undo', 'CloneLayer Undo and restoration', '08.004 14.015 14.016', '14.019 14.020', 'Retain source reference and lifecycle state through Undo/Redo without dangling targets.')
profile('group-clone', 'Group duplication and clone remapping', '14.011', '14.019 14.020', 'Retain the old recursive old-to-new item map and distinguish internal from external clone references.')
profile('filter', 'Independent FilterLayer model and scheduler', '08.003 15.001 15.001/gvalue-deep-copy 15.002 15.003 15.004 15.005 15.006 15.006/gegl-result-source 15.006/filter-pickable-opacity 15.006/filter-progress-start 15.006/editability-contract 15.007 15.008 15.009 15.010 15.011 15.012 15.013 17.001 17.002 17.003 17.004 17.005 17.007 17.008 17.009 17.010 17.011 17.013 17.014 17.015 17.016 17.017 17.018 17.019 17.020 17.021 17.022 17.023 17.024 17.026/filter-end-timeout-cancel 17.028', '15.014 18.002 18.003 18.004 18.008 18.009 18.011 18.012 18.013 18.017', 'Keep the independent layer/runner/checkpoint contract and cached initial display; never substitute GEGL layer effects.')
profile('brush-read', 'Extended myb decoding', '19.001 19.002 19.003 19.004 19.005 19.006 19.006/v1-curve-validation 19.007 19.008 19.009 19.010', '19.015 19.015/v1-curve-roundtrip 36.026', 'Preserve all versions, setting curves, switches, resource names and unknown fields, including legacy parse quirks.')
profile('brush-write', 'Extended myb serialization', '19.011 19.011/output-stream-save 19.012', '19.015 19.015/v1-curve-roundtrip', 'Serialize custom settings, curves and metadata via current output-stream ownership and failure handling.')
profile('brush-resource', 'Extended brush data and preview resource', '08.005 19.013 19.013/preview-contract 19.013/icon-buffer 19.014', '19.015 24.017', 'Keep resource identity, dirty/editor state, preview, cloning and persistence separate.')
profile('brush-settings', 'Extended setting definitions and mapping values', '04.013/brush-setting-generator 08.007/dict-owner 19.003 19.006/mapping-value-owner 20.001', '19.015 20.018 34.013', 'Keep stable setting indices, defaults, input metadata and deep-owned curves; restore the absent generator explicitly.')
profile('brush-engine', 'Extended MyPaint evaluation and stroke state', '20.001 20.002 20.003 20.004 20.005 20.006 20.007 20.008 20.009 20.010 20.011 20.012 20.013 20.014 20.015 20.016 20.017', '20.018 21.019 21.020', 'Preserve evaluation, sampling/retained color order, random state and texture/stroke-opacity parameters in the selected engine.')
profile('brush-surface', 'Extended dab/color/nonincremental Surface', '09.003/region-lifetime 21.001 21.002 21.002/resource-owner 21.003 21.004 21.005 21.006 21.007 21.008 21.009 21.010 21.011 21.012 21.013 21.014 21.015 21.016 21.017 22.007', '21.018 21.019 21.020 22.010', 'Preserve mask-aware dab and sampling, retained valid color, buffer selection, nonincremental blending, stroke opacity and Undo.')
profile('brush-shape', 'GIMP shape transform and sampling', '21.002 21.002/resource-owner 21.003 21.004 21.005 21.006 22.003 22.005', '21.019 22.010', 'Use matching shape transforms for sampling and dabs and preserve hardness/aspect/angle and paper masking.')
profile('brush-backend', 'MyPaint drawable backend and snapshots', '09.003 09.003/region-lifetime 09.004 09.006 09.007 09.008 09.009 09.010 09.011 09.012 09.013 21.010 21.011', '09.016 09.017 21.019', 'Replace TileManager/PixelRegion without changing source snapshot, selection/mask or commit ownership.')
profile('brush-core', 'MyPaint core stroke integration', '08.006 08.006/core-options-init 20.017 21.014 21.015 21.016', '09.017 21.019 36.019', 'Preserve stroke lifecycle and resource binding through current paint-core start/motion/finish and tool switching.')
profile('brush-undo', 'MyPaint stroke Undo ownership', '09.011/mypaint-undo-stroke 21.014', '09.017 21.019', 'Implement Undo/Redo of stroke state and pixels; the legacy empty pop is not a valid migration implementation.')
profile('brush-options', 'Extended MyPaint option model', '08.007 08.007/dict-owner 19.016 24.001 30.007', '19.015 24.017', 'Keep all properties, defaults, saved-resource versus live-edit state and context propagation.')
profile('brush-history', 'Brush history and live-state restoration', '24.010 24.011', '24.017', 'Retain history entry values and resource references independently of live mutable options.')
profile('brush-tool', 'MyPaint tool and input lifecycle', '08.008 08.008/tool-core-lifetime 23.007 23.008 23.009 23.012 23.014', '23.015 26.013 36.019', 'Preserve tool/core ownership, pressure events, rulers and release/cancel behavior.')
profile('brush-editor', 'Extended brush editor controls', '24.001 24.001/editor-context-contract 24.002 24.003 24.004 24.005 24.006 24.008 24.012 24.013 24.014 24.015 24.015/mypaint-editor-create 24.016', '24.017 24.017/mypaint-editor-constructor-test', 'Retain every custom numeric/switch/curve/resource control in shared dock/popup/horizontal components.')
profile('brush-list', 'Brush list, selection and resource actions', '24.007 24.008 24.009 30.005', '19.015 24.017 29.020', 'Retain resource selection, tagging, edit-active, save/duplicate/rename/delete and popup callback behavior.')
profile('texture', 'Standard paintbrush/dynamics paper', '22.001 22.002 22.003 22.005 22.006 22.008 22.009 30.007', '22.010 24.017', 'Preserve ordinary brush paper resources, grain/contrast evaluation, mask order and option serialization.')
profile('smudge', 'Independent Smudge accumulation and blending', '23.001 23.002 23.003 23.004 23.005', '23.006', 'Keep legacy blending-output independent of Flow/Rate; preserve accumulation and size-change behavior.')
profile('pressure', 'Motion/pressure event semantics', '23.007 23.008 23.009 23.010 23.011 23.012 23.014', '23.015 26.013', 'Preserve fixed-position and tiny-motion pressure events plus smoothing/ruler order and release cleanup.')
profile('ruler', 'Perspective guide model and editing', '08.009 08.009/removed-signal 08.009/guide-owner 08.010 26.001 26.002 26.003 26.004 26.005 26.006 26.007 26.008 26.009 26.010 26.011 26.012', '26.013 26.014', 'Preserve up to three vanishing points, selection, screen-aware direction lock and image-owned lifetime.')
profile('ruler-canvas', 'Perspective guide display and transforms', '26.005 26.006 29.017', '26.014 29.020', 'Use current canvas/GTK3 transforms without changing ruler geometry or hit testing.')
profile('fill', 'Bounded brush flood fill and selection', '08.011 08.012 27.001 27.001/snapshot-contract 27.002 27.003 27.004/gegl-bounded-search 27.005/selection-barrier 27.006 27.007/paintcore-gegl-compose 27.008 27.009/stroke-lifecycle 27.010 27.011 27.012', '27.013 27.013/gegl-render-regression 27.014 27.015', 'Keep fixed stroke input/color, traversal bounds, threshold 30, grow and constant-mode composition; do not clip a global search afterward.')
profile('bucket-selection', 'Standard bucket selection traversal barrier', '27.005/selection-barrier', '27.014 27.015', 'Selection is a traversal input in the legacy standard bucket path, not only a final result mask.')
profile('rotation', 'Canvas rotation/flip geometry', '25.001 25.002 25.003 25.004 25.005 25.006 25.007 25.008 25.009 25.010', '25.011 25.012 26.014', 'Retain modifier transitions, start-angle semantics and flip direction while using the current display transform.')
profile('canvas-transform', 'Canvas transform and clipping support', '25.009 26.005 29.017', '25.011 26.014 29.020', 'Retain transformed canvas geometry, clipping/extents and hit-test coordinates on the current display matrix.')
profile('overlay', 'Canvas overlay placement and interaction', '29.001 29.007 29.008 29.010 29.011 29.012 29.013 29.014 29.015 29.016 29.017 29.018 29.019', '29.020', 'Retain color/tool/layer overlays, portrait layout, hide/input-through/focus restoration and teardown.')
profile('window', 'Dock/window layout and restoration', '29.011 29.016 30.005 30.005/shell-reparent-ref 30.016', '29.020 36.019', 'Port window/dock placement and serialization with symmetric references and image switching.')
profile('layer-tile', 'Layer tile display and operations', '08.013 29.001 29.002 29.002/add-timeout-owner 29.003 29.004 29.005 29.006 29.008/icon-null 29.019/preview-idle-owner', '29.020 29.020/preview-idle-teardown', 'Retain previews, selection, reorder, visibility and long-press operations, including custom layers and groups.')
profile('layer-popup', 'Layer/filter popup settings', '29.006 29.006/proc-args-owner 29.006/menu-borrowed-reference 29.006/popover-handler-teardown', '15.014 29.020', 'Keep filter arguments/source/mode controls; safely deep-copy values and detach borrowed popup owners.')
profile('popup', 'Popover/renderer event and lifetime', '08.015 08.016 08.016/pixbuf-owner 29.006/cellrenderer-gtk3 29.006/popover-class-signals 29.006/popover-handler-teardown 29.015 29.019', '29.020', 'Port GTK3 drawing/event ABI and preserve confirm/cancel/keyboard/focus behavior with owned connections.')
profile('tool-tile', 'Tool tile/group selection', '08.014 29.008 29.008/icon-null 29.009 30.001/tool-group-state', '29.020', 'Preserve grouped tool choice, current state, icons and selection propagation.')
profile('tool-group', 'Tool grouping/configuration', '30.001/tool-group-state 29.009 30.004 30.008', '29.020 36.019', 'Compare old expanded/visible/active-tool state and toolrc ordering before reusing upstream grouping.')
profile('toolbar', 'Tool options toolbar and compact standard controls', '29.010 29.010/toolbar-ref-balance 29.010/standard-tool-options 29.011', '29.020', 'Keep each standard tool option available in compact/popup/portrait UI; reuse controls without deleting behavior.')
profile('color-ui', 'Palette/color selector interaction', '29.007 29.007/palette-popup-teardown', '29.020', 'Retain foreground/background state, palette callbacks and compact color selector geometry/lifetime.')
profile('widget-helpers', 'Shared widget model/control extensions', '06.023 08.017 29.010/standard-tool-options', '07.008 29.020', 'Port the specific helper and its consumers; preserve values, notifications, model state and widget ownership.')
profile('runner', 'Procedure execution and cancellation', '16.002 16.003 16.005 16.006 16.007 16.008 16.009 16.010 16.011 16.012 16.013 16.014 16.015 16.016 16.017 16.018 16.019 16.020 16.021', '16.023 18.013 18.014', 'Retain running/reserved/cancel/finish and typed argument/result behavior on the current internal PDB API.')
profile('plugin-procedure', 'Legacy filter procedure execution contract', '16.001 16.003 16.004 16.022', '16.023 18.013 28.012', 'Audit this procedure by name for with-last-values/noninteractive execution, defaults, image/drawable values and output equivalence.')
profile('pdb-select', 'MyPaint selection PDB', '30.012 30.013 24.007', '24.017 34.013', 'Regenerate current PDB registration and client wrappers from one source, retaining select/set/close callback contracts.')
profile('preset-resource', 'Layer-preset JSON resource/factory', '28.001 28.002 28.003', '28.013 28.015', 'Keep schema, unknown keys, paths and model ownership; JSON wrappers are not disposable HTTP-only code.')
profile('preset-apply', 'Layer-preset structure application', '28.004 28.005 28.006 28.007 28.008 28.009 28.010 28.014/applier-owner', '28.011 28.012 28.013 28.015', 'Recreate editable layer/group/clone/filter structures, source links, modes and arguments with atomic Undo/rollback.')
profile('preset-gui', 'Layer-preset actions and preferences', '28.014 28.014/applier-owner 28.014/preset-preferences 28.014/preset-dialog-actions 28.014/preset-action-group', '28.013 28.015 29.020', 'Retain preset selection/preferences/actions and manage callback/factory/group cache lifetimes.')
profile('init', 'Feature initialization and lifetime', '30.016 30.016/feature-entry-point', '04.016 36.019', 'Register required types/resources in GUI and console paths and disconnect feature callbacks at shutdown.')
profile('http', 'Optional HTTP/REST feature', '31.001 31.002 31.003 31.004 31.005 31.006 31.007 31.008 31.008/router-lifetime', '31.009 31.010', 'Audit each route and consumer; keep optional isolation and request/owner cleanup, not a silent deletion.')
profile('placeholder', 'ImageGenerator placeholder dependency', '31.011 31.012', '31.020', 'Only remove placeholder registration after proving no saved functional data or dependent feature is lost.')
profile('cleanup', 'Nonfunctional/obsolete source candidate', '31.018', '31.020 38.001', 'Candidate-only classification; verify references/build use and preserve any actual functional content before removal.')
profile('xcf-read', 'Legacy XCF detection and property decoding', '10.002 10.003 10.004 10.006 10.007 10.008 10.009 10.010 11.001 11.002 11.003 11.005 11.006 11.007 11.008 11.010 11.011 11.017 11.018 11.019 11.020 11.021', '36.001 36.002 36.003 36.005 36.007 36.021', 'Distinguish legacy v4 and properties 32/33 from standard precision/properties; restore custom objects and retain unresolved records.')
profile('xcf-write', 'Custom layer saved representation', '10.011 10.012 10.013 10.014 12.001 12.002 12.002/clone-unresolved-write 12.003 12.003/filter-args-ownership 12.004 12.005 12.008 12.009', '12.013 12.014 12.015 12.015/clone-unresolved-roundtrip 12.015/filter-args-roundtrip', 'Preserve types, references, ordered typed arguments and cached pixels in an unambiguous versioned extension.')
profile('assets-brush', 'Bundled extended brush definition', '19.001 19.002 19.003 19.004 19.005 19.006 19.007 19.008 19.009 30.009 30.017', '19.015 36.014 36.015 34.014', 'Keep this exact brush identity/hash, all extension values and linked resources through decoding, rendering, editing and installation.')
profile('assets-preview', 'Brush preview/image source asset', '19.013/preview-contract 30.017', '24.017 34.014', 'Retain exact file identity and brush/template relation; regenerate only through a verified equivalent preview path.')
profile('assets-preset', 'Editable layer-preset definition', '28.001 28.004 28.005 28.006 28.007 28.008 30.017', '28.011 28.012 28.013 28.015 34.014', 'Even test-named presets are functional assets; preserve each definition and its editable generated structure.')
profile('assets-manifest', 'Resource distribution manifest', '30.009 30.010 30.011 30.017', '34.002 34.014 38.005', 'Replace Autotools install lists with complete Meson/package manifests, preserving every referenced asset.')
profile('assets-license', 'Asset provenance and usage notice', '34.005/license-manifest 38.010', '34.014 38.005', 'Keep author/license/provenance text in the distributable notices; names or README format do not justify deletion.')
profile('icons', 'Icon/theme resource', '30.003 30.017', '29.020 34.014', 'Register the legacy icon meaning and fallback at all sizes/scale factors; preserve source data or document a verified replacement.')
profile('translation', 'User-visible strings and translation', '30.014 30.015', '29.020 34.002', 'Register source strings and retain Japanese translations/help meaning for all preserved controls.')
profile('menus', 'Brush/dialog menu registrations', '30.001 30.005', '24.017 29.020', 'Keep action/menu IDs and the editor/brush dialog entry points aligned with current actions.')
profile('packaging', 'Legacy packaging and pinned dependencies', '32.001 33.001 34.005 34.005/license-manifest', '32.012 33.009 34.002', 'Old Flatpak instructions/patches are evidence for reproducibility; replace obsolete runtimes only with verified current packaging.')
profile('scripts', 'ImageMagick brush preview labeling', '30.017/brush-preview-generator', '34.013 34.014', 'Retain the in-place ImageMagick caption/border/resize operation; quote resource filenames and verify output assets without inventing a GIMP batch dependency.')
profile('manifest-generator', 'Ruby brush installation-manifest generator', '30.017/brush-manifest-generator', '34.013 34.014', 'Preserve globbed myb and _prev.png membership and deterministic sorting when replacing generated Autotools asset lists with Meson manifests.')
profile('scheme-modes', 'Script-Fu legacy compositing constants', '30.012/scheme-mode-constants', '13.013 34.013 36.004', 'Preserve seven Script-Fu names and translate their mode meaning explicitly instead of reusing conflicting old integer values.')
profile('tool-delete', 'Tool-option deletion result', '30.008/tool-options-delete-result', '30.008/tool-options-delete-result', 'Correct the inverted legacy unlink error condition; test successful removal, absent file and genuine failure without deleting unrelated state.')

profile('brush-context', 'Extended brush context value and resource identity', '19.013 19.014 30.007 30.008 30.011', '19.015 24.017 36.019', 'Preserve custom brush object/name, notify, copy, thaw/removal and fallback without replacing it with an unextended standard MyBrush.')
profile('fill-input', 'Bounded fill source/seed/offset preparation', '27.001/snapshot-contract 27.002 27.004/gegl-bounded-search 27.005/selection-threshold-contract', '27.013 27.014 27.015', 'Retain explicit input pixels/start color, selection bounds, source/mask offsets and transparent seed policy.')
profile('fill-iterator', 'Bounded fill traversal and iterator lifetime', '09.003 27.004/gegl-bounded-search 27.006 27.010', '09.016 27.013 27.014 27.015', 'Replace the three-region tile iterator with bounded GEGL traversal; preserve scanline connectivity and release every acquired region.')
profile('fill-coverage', 'Fill distance and soft selection penalty', '27.003 27.005/selection-barrier 27.005/selection-threshold-contract 27.006', '27.013 27.014 27.015', 'Preserve distance, transparency and coverage formula; zero selection is not an unconditional barrier at every legacy threshold.')
profile('device-identity', 'Input device unique-name identity', '30.007/device-name-identity', '23.015 36.019', 'Compare duplicate-device names and restored per-device state before accepting the old unique-names policy.')

profile('resource-registration', 'Extended brush factory lifecycle and search paths', '08.005 19.013 19.014 30.009 30.011 30.016', '19.015 24.017 36.019', 'Register the extended loader/factory and preserve restore, save, disposal and configured resource paths.')
profile('factory-registration', 'Generic JSON/preset resource factory registration', '28.003 30.016', '28.013 28.015 36.019', 'Preserve factory-table key lookup and lifetime separately from tool-group and brush registration.')
profile('core-type-declarations', 'Custom core type declarations and registration', '04.003 04.004 08.001', '04.014 04.017', 'Preserve each declared resource, layer, guide and tool type in the C/GObject registry without leaking C++ templates.')
profile('tool-registration', 'Custom tool registration in the shared manager', '08.008 08.010 08.012 30.003 30.004', '23.015 26.014 27.015 29.020', 'Register MyPaint, perspective guide and bucket brush in the current manager with correct options/core associations.')
profile('event-compression', 'Ordered motion compression and event reinjection', '23.009 23.014/event-compression-order', '23.015 29.020', 'Retain motion-event ownership and stop/reinject non-motion events in order; do not lose release, key, crossing or device changes.')
profile('ruler-input', 'Perspective lazy snap and paint-event ordering', '23.012 26.007 26.008 26.009 23.014', '23.015 26.013 26.014', 'Preserve direction-lock threshold, deferred begin_tool, full-motion tracking and stroke release state.')
profile('view-gesture', 'Drag rotate/zoom and transformed pan gestures', '25.001 25.002 25.003 25.004 25.005 25.006 25.007 25.008 25.009 25.009/drag-zoom-scroll', '25.011 25.012 29.020', 'Keep shift/control entry conditions, drag zoom distance and transformed pan origins alongside rotation/snap state.')
profile('mirror-key', 'Mirrored horizontal keyboard input', '25.008/mirrored-arrow-input', '25.011 29.020', 'Preserve Left/Right key remapping only in mirrored display state and verify press/release and tool event routing.')

# Exact configure.ac payload review: migration/inventory/configure-hunk-routing-review.json.
# Diff headings are unchanged context, not the changed build contract.
profile('configure-001888', 'Babl and GEGL minimum dependency versions', '03.002', '34.001 34.006', 'Changes only Babl 0.1.10 to 0.1.12 and GEGL 0.2.0 to 0.3.0. Retain the version requirement and prove its current dependency replacement; do not transplant the old versions into GIMP 3. The gimp_full_name source_scope is unchanged diff context, not a project-name change. Dependency configuration belongs to 03.002 and current release/cross-platform compile verification to 34.001/34.006.')
profile('configure-001889', 'ALSA dependency macro defect and optional configuration', '03.006', '34.001 38.001', 'The complete payload changes m4_define to m5_define while keeping ALSA 1.0.0. Treat this as a suspicious source macro defect, not as a callback or a requested feature deletion. Keep the optional ALSA dependency contract in the current Meson options; verify current release configuration and retain final source disposition review. No claim that the legacy macro defect is harmless is made.')
profile('configure-001890', 'Optional HTTP Soup dependency', '31.003 31.004', '31.009 31.010', 'Adds libsoup >=2.46 metadata for the HTTP subsystem. Preserve the optional feature boundary and adapt it to the adopted Soup API; verify both disabled core independence and enabled endpoint behavior. The old library version is provenance, not the required GIMP 3 ABI.')
profile('configure-001891', 'Required JSON dependency for brush and preset persistence', '04.009', '19.015 28.015 31.009', 'Adds JSON-GLib >=1.0 metadata. JSON is shared by brush/preset persistence and must not become HTTP-only or rely on an accidental transitive archive dependency. Existing original 04.009 owns link dependency/order verification; its already-existing 04.009/json-dependency child contains the precise direct-link duty and remains unchanged. Verify brush/preset round trips and HTTP-disabled persistence.')
profile('configure-001892', 'C++ compiler detection and C++14 standard', '03.001 03.007', '07.014 34.006', 'Adds only AC_PROG_CXX, AC_PROG_CXX_C_O and AX_CXX_COMPILE_STDCXX_14. The corresponding original duties are Meson C/C++ capability and C++ standard selection. It changes no GObject lifecycle callback, exception policy or visibility rule. Preserve independent mixed-executable and OS compile verification; no current platform pass is inferred.')
profile('configure-001893', 'GEGL dependency ABI selection', '03.002', '34.001 34.006', 'Changes the pkg-config module from gegl-0.2 to gegl-0.3. Retain its dependency/ABI identity as provenance and verify the adopted GIMP 3 gegl-0.4 replacement in current builds, without copying a legacy incompatible ABI or inferring pixel equivalence.')
profile('configure-001894', 'Required JSON pkg-config discovery', '04.009', '19.015 28.015 31.009', 'Adds the JSON-GLib pkg-config probe, paired with hunk 4 version metadata. Preserve explicit direct include/link dependencies for brush/preset persistence. The existing 04.009/json-dependency child specifies that work; this recommendation references its original parent and keeps all obligations TODO. Verify persistence with HTTP disabled as well as round trips.')
profile('configure-001895', 'HTTP optional probe and build defines', '31.003 31.004', '31.009 31.010', 'Adds enable-httpd, Soup discovery, HAVE_LIBSOUP/USE_HTTPD defines and USE_HTTPD Automake conditional. Keep optional isolation and current dependency adaptation, including missing-dependency/disabled behavior. The old conditional tests enable_httpd even if Soup discovery failed; preserve the source evidence and verify the intended boundary instead of copying that inconsistency. Current secure/default-disabled policy is unchanged.')
profile('configure-001896', 'Painter user profile and resource path identity', '30.010 30.011', '04.016 36.022', 'Changes the default user directory from .gimp-2.8 to .gimp-painter-2.8 via gimp_user_version substitution. Preserve old-profile discovery, user-edit protection and resource lookup when migrating to the current XDG/profile scheme. This is neither project-name metadata nor a GObject callback. Verify installed startup and missing-resource information retention.')
profile('configure-001897', 'Optional HTTP module build registration', '31.003 31.004', '31.009 31.010', 'Registers app/httpd/Makefile generation, whose module remains an optional feature in Meson. Keep the HTTP build/dependency boundary and enabled/disabled verification. The current C++ source registration was separately reviewed under 04.002; this correction does not reopen or falsely complete that registration checkpoint.')
profile('configure-001898', 'Preset module build integration', '28.002 28.003 28.014', '28.013 28.015', 'Registers app/presets/Makefile generation. Preserve the module purpose through the current JSON resource model, factory and selection UI, even though replacement sources no longer live in app/presets. Existing C++ registration evidence is separate; complete preset fixture and persistence obligations remain TODO.')
profile('configure-001899', 'Preset and extended brush install manifests', '30.017', '34.002 34.014', 'Registers data/layer-presets and root plus six named MyPaint brush-directory Makefiles. Preserve every exact directory/asset identity and current install manifest coverage; this adds no callback, exception or visibility policy. Verify Linux installed assets and the independent resource-manifest check.')
profile('configure-001900', 'Soup configuration summary output', '31.003 31.004', '31.009 31.010', 'Adds only the printed SOUP availability line. Preserve accurate optional dependency/feature reporting with the HTTP boundary and its current dependency adaptation; verify enabled/disabled configuration states without treating an informational line as lifecycle code.')

# Exact build-input review: migration/inventory/cpp-registration-review.json.
# These inputs are not all C++ translation-unit registrations.
profile('registration-3', 'Reviewed Makefile link-only', '04.008 04.009', '04.015 04.016', 'This hunk adds only a dummy C++ link-driver TU or an archive/HTTP LDADD entry. Final-link/runtime/archive checks belong to04.008/04.009; main.c stays C.')
profile('registration-4', 'Reviewed Makefile link-only', '04.008 04.009', '04.015 04.016', 'This hunk adds only a dummy C++ link-driver TU or an archive/HTTP LDADD entry. Final-link/runtime/archive checks belong to04.008/04.009; main.c stays C.')
profile('registration-5', 'Reviewed Makefile link-only', '04.008 04.009', '04.015 04.016', 'This hunk adds only a dummy C++ link-driver TU or an archive/HTTP LDADD entry. Final-link/runtime/archive checks belong to04.008/04.009; main.c stays C.')
profile('registration-6', 'Reviewed Makefile link-only', '04.008 04.009', '04.015 04.016', 'This hunk adds only a dummy C++ link-driver TU or an archive/HTTP LDADD entry. Final-link/runtime/archive checks belong to04.008/04.009; main.c stays C.')
profile('registration-7', 'Reviewed Makefile link-only', '04.008 04.009', '04.015 04.016', 'This hunk adds only a dummy C++ link-driver TU or an archive/HTTP LDADD entry. Final-link/runtime/archive checks belong to04.008/04.009; main.c stays C.')
profile('registration-8', 'Reviewed Makefile link-only', '04.008 04.009', '04.015 04.016', 'This hunk adds only a dummy C++ link-driver TU or an archive/HTTP LDADD entry. Final-link/runtime/archive checks belong to04.008/04.009; main.c stays C.')
profile('registration-9', 'Reviewed Makefile c-feature-registration', '30.001 24.007', '29.020', 'C brush-list action/header registration. This is feature integration, not a C++ compiler-registration prerequisite. The source-specific feature action remains TODO.')
profile('registration-10', 'Reviewed Makefile c-feature-registration', '30.001 24.001', '29.020', 'C editor action/header registration; editor actions now share the native brush-action/editor route. This is feature integration, not a C++ compiler-registration prerequisite. The source-specific feature action remains TODO.')
profile('registration-38', 'Reviewed Makefile private-headers', '04.005', '04.014 04.015', 'Only private .hpp headers are appended; no compiled translation unit is added. Header exposure and compile/dependency checks remain open, as do the headers own feature obligations.')
profile('registration-129', 'Reviewed Makefile c-feature-registration', '08.009 26.001', '26.013 26.014', 'C image-owned perspective model/header registration. This is feature integration, not a C++ compiler-registration prerequisite. The source-specific feature action remains TODO.')
profile('registration-131', 'Reviewed Makefile c-feature-registration', '30.004 30.008', '29.020 36.019', 'C tool grouping/header registration, now native upstream source names. This is feature integration, not a C++ compiler-registration prerequisite. The source-specific feature action remains TODO.')
profile('registration-457', 'Reviewed Makefile c-feature-registration', '25.001', '25.011 25.012', 'C canvas rotation/header registration. This is feature integration, not a C++ compiler-registration prerequisite. The source-specific feature action remains TODO.')
profile('registration-868', 'Reviewed Makefile private-headers', '04.005', '04.014 04.015', 'Only private .hpp headers are appended; no compiled translation unit is added. Header exposure and compile/dependency checks remain open, as do the headers own feature obligations.')
profile('registration-887', 'Reviewed Makefile private-headers', '04.005', '04.014 04.015', 'Only private .hpp headers are appended; no compiled translation unit is added. Header exposure and compile/dependency checks remain open, as do the headers own feature obligations.')
profile('registration-1009', 'Reviewed Makefile pdb-generated-registration', '30.012 30.013', '34.013', 'Adds only a named MyPaint-selection PDB input or generated C/header to a source list. The missing API and its generated registration remain explicit TODO obligations in30.012/30.013, with generated-output verification in34.013. It changes no common generator rule or C/C++ dependency; remove only the duplicate04.013 assignment without claiming feature completion.')
profile('registration-1081', 'Reviewed Makefile c-feature-registration', '29.010', '29.020', 'C brush options GUI/header registration; native Brush editor is consumed by the compact adapter. This is feature integration, not a C++ compiler-registration prerequisite. The source-specific feature action remains TODO.')
profile('registration-1083', 'Reviewed Makefile c-feature-registration', '29.010', '29.020', 'C dynamics options GUI/header registration; native Dynamics editor is consumed by the compact adapter. This is feature integration, not a C++ compiler-registration prerequisite. The source-specific feature action remains TODO.')
profile('registration-1084', 'Reviewed Makefile placeholder-registration', '31.011 31.012', '31.020', 'Registers the ImageGenerator placeholder. Its declared scope is dependency proof then removal, not forcing a nonfunctional C++ tool into the build; that proof/removal acceptance remains TODO.')
profile('registration-1514', 'Reviewed Makefile c-feature-registration', '24.007 30.012', '29.020', 'C brush factory view/select registration; complete old selection PDB remains open. This is feature integration, not a C++ compiler-registration prerequisite. The source-specific feature action remains TODO.')
profile('registration-1517', 'Reviewed Makefile c-feature-registration', '08.015 29.010', '29.020', 'C popup-button registration; GTK3 popovers are consumed by the current compact adapter. This is feature integration, not a C++ compiler-registration prerequisite. The source-specific feature action remains TODO.')
profile('registration-1518', 'Reviewed Makefile asset-header', '30.003 30.017', '34.014', 'Adds only the generated titlebar pixbuf header, an icon/resource duty; no compiled source or link-runtime change occurs.')
profile('registration-1519', 'Reviewed Makefile c-feature-registration', '29.010', '29.020', 'C toolbar registration; canonical native control tree is owned by the compact adapter. This is feature integration, not a C++ compiler-registration prerequisite. The source-specific feature action remains TODO.')
profile('registration-1522', 'Reviewed Makefile newline-only', '38.001', '38.001', 'The complete hunk only adds the missing final newline after rm -f xgen-wec. It introduces no source, dependency, or enum-generation semantic change. Preserve this proven nonfunctional disposition for final hunk reconciliation under38.001; no implementation or file-removal work is required.')
profile('registration-2293', 'Reviewed Makefile pdb-generated-registration', '30.012 30.013', '34.013', 'Adds only a named MyPaint-selection PDB input or generated C/header to a source list. The missing API and its generated registration remain explicit TODO obligations in30.012/30.013, with generated-output verification in34.013. It changes no common generator rule or C/C++ dependency; remove only the duplicate04.013 assignment without claiming feature completion.')
profile('registration-2294', 'Reviewed Makefile pdb-generated-registration', '30.012 30.013', '34.013', 'Adds only a named MyPaint-selection PDB input or generated C/header to a source list. The missing API and its generated registration remain explicit TODO obligations in30.012/30.013, with generated-output verification in34.013. It changes no common generator rule or C/C++ dependency; remove only the duplicate04.013 assignment without claiming feature completion.')
profile('registration-2365', 'Reviewed Makefile menu-manifest', '30.001 30.005 30.017', '29.020 34.014', 'Adds two installed menu XML assets; registration belongs to menu/action/resource distribution, not a C++ source or runtime link.')
profile('registration-2484', 'Reviewed Makefile pdb-generated-registration', '30.012 30.013', '34.013', 'Adds only a named MyPaint-selection PDB input or generated C/header to a source list. The missing API and its generated registration remain explicit TODO obligations in30.012/30.013, with generated-output verification in34.013. It changes no common generator rule or C/C++ dependency; remove only the duplicate04.013 assignment without claiming feature completion.')

FILES = {}
def files(key, *names):
    for name in names:
        if name in FILES: raise ValueError('duplicate route '+name)
        FILES[name] = key

def group(key, directory, names):
    files(key, *(directory+'/'+n for n in names.split()))

group('cpp-signals','app/base','delegators.hpp')
group('cpp-types','app/base','glib-cxx-types.hpp glib-cxx-bridge.hpp')
group('cpp-registration','app/base','glib-cxx-impl.hpp')
group('cpp-values','app/base','glib-cxx-utils.hpp')
group('cpp-scope','app/base','scopeguard.hpp')
group('cpp-selection','app/base','selectcase-utils.hpp')
group('cpp-gtk','app/base','glib-cxx-def-utils.hpp')
group('json','app/base','json-cxx-utils.hpp')
group('http','app/base','route.hpp soup-cxx-utils.hpp')
group('pixels','app/base','pixel-processor.c pixel.hpp temp-buf.c temp-buf.h')
group('texture-loop','app/base','pixel-region.c pixel-region.h')
group('cleanup','app/base','tile-cache.c tile-manager.c tile.c')
group('mode-enum','app/base','base-enums.c base-enums.h')
group('config','app/config','gimpcoreconfig.c gimpcoreconfig.h gimpguiconfig.c gimprc-blurbs.h')
group('container','app/core','gimpcontainer.c gimpcontainer.h gimplist.c gimpviewable.c gimpviewable.h')
group('core-hooks','app/core','gimpcurve.c gimpdatafactory.c gimpdrawable.c gimpparamspecs-duplicate.c gimpparamspecs.c gimpparamspecs.h')
group('invalidation','app/core','gimpdrawablestack.c gimpdrawablestack.h gimpimage-crop.c gimpprojection.c gimpprojection.h gimpmarshal.list')
group('editability','app/core','gimpitem.c gimpitem.h gimpimage-undo.c')
group('clone','app/core','gimpclonelayer.cpp gimpclonelayer.h')
group('clone-undo','app/core','gimpclonelayerundo.cpp gimpclonelayerundo.h')
group('group-clone','app/core','gimpgrouplayer.c gimpgrouplayer.h')
group('filter','app/core','gimpfilterlayer.cpp gimpfilterlayer.h')
group('brush-read','app/core','gimpmypaintbrush-load.cpp gimpmypaintbrush-load.h')
group('brush-write','app/core','gimpmypaintbrush-save.cpp gimpmypaintbrush-save.h')
group('brush-resource','app/core','gimpmypaintbrush.cpp gimpmypaintbrush.h gimpmypaintbrush-private.hpp')
group('brush-settings','app/core','mypaintbrush-brushsettings.c mypaintbrush-brushsettings.h mypaintbrush-enum-settings.h mypaintbrush-mapping.hpp')
group('ruler','app/core','gimpperspectiveguide.cpp gimpperspectiveguide.h gimpimage-perspective-guide.c gimpimage-perspective-guide.h gimpimage-private.h gimpimage-snap.c gimpimage-snap.h')
group('tool-group','app/core','gimptoolgroup.c gimptoolgroup.h gimptoolitem.c gimptoolitem.h gimptoolinfo.c gimptoolinfo.h')
group('config','app/core','gimptooloptions.c gimptooloptions.h gimp-user-install.c')
group('smudge','app/core','gimpdynamics.c gimpdynamics.h gimpdynamicsoutput.h')
group('bucket-selection','app/core','gimpdrawable-bucket-fill.c')
group('fill','app/core','gimpimage-contiguous-region.c gimpimage-contiguous-region.h')
group('mode-enum','app/core','gimplayer.c')
group('brush-context','app/core','gimpcontext.c gimpcontext.h')
group('init','app/core','gimp.c gimp.h core-types.h core-enums.c core-enums.h gimpimage.c gimpimage.h')
group('cleanup','app/core','gimpdrawableundo.c')
group('brush-shape','app/paint','gimpmypaintcore-brushfeature.hpp')
group('brush-backend','app/paint','gimpmypaintcore-drawablefeature.hpp')
group('brush-surface','app/paint','gimpmypaintcore-surface.cpp gimpmypaintcore-surface.hpp')
group('brush-core','app/paint','gimpmypaintcore.cpp gimpmypaintcore.hpp')
group('brush-undo','app/paint','gimpmypaintcoreundo.cpp gimpmypaintcoreundo.h')
group('brush-history','app/paint','gimpmypaintoptions-history.cpp gimpmypaintoptions-history.hpp')
group('brush-options','app/paint','gimpmypaintoptions.cpp gimpmypaintoptions.h')
group('brush-engine','app/paint','mypaintbrush-brush.hpp mypaintbrush-stroke.cpp mypaintbrush-stroke.hpp mypaintbrush-surface.hpp')
group('texture','app/paint','gimpbrushcore.c gimpbrushcore.h gimppaintoptions.c gimppaintoptions.h')
group('smudge','app/paint','gimpsmudge.c gimpsmudge.h gimpsmudgeoptions.c gimpsmudgeoptions.h')
group('editability','app/paint','gimppaintcore.c gimppaintcore.h')
group('pressure','app/paint','gimpink.c gimpink.h')
group('init','app/paint','gimp-paint.c gimp-paint.h paint-types.h')
group('cleanup','app/paint','gimpmypaintcore.h.bak')
group('brush-surface','app/paint-funcs','mypaint-brushmodes.hpp')
group('composite','app/paint-funcs','paint-funcs-generic.h paint-funcs.c paint-funcs.h')
group('runner','app/pdb','pdb-cxx-utils.cpp pdb-cxx-utils.hpp gimp-pdb-compat.c gimp-pdb-compat.h gimpprocedure.h')
group('pdb-select','app/pdb','mypaint-brush-select-cmds.c internal-procs.c internal-procs.h')
group('preset-resource','app/presets','gimpjsonresource.cpp gimpjsonresource.h preset-factory.cpp preset-factory.h presets-enums.c presets-enums.h')
group('preset-apply','app/presets','layer-preset.cpp layer-preset.h')
group('preset-gui','app/presets','layer-preset-gui.cpp layer-preset-gui.h preset-factory-gui.cpp preset-factory-gui.h')
group('init','app','app.c gimp-features.cpp gimp-features.h')
group('core-hooks','app','sanity.c signals.c')
group('cleanup','app','dummy.cpp')
group('ruler-canvas','app/display','gimpcanvasperspectiveguide.cpp gimpcanvasperspectiveguide.h')
group('rotation','app/display','gimpdisplayshell-rotate.c gimpdisplayshell-rotate.h')
group('canvas-transform','app/display','gimpcanvasarc.c gimpcanvascorner.c gimpcanvasgrid.c gimpcanvasguide.c gimpcanvashandle.c gimpcanvasitem.h gimpcanvaspath.c gimpcanvasrectangle.c gimpcanvasrectangleguides.c gimpdisplayshell-draw.c gimpdisplayshell-expose.c gimpdisplayshell-expose.h gimpdisplayshell-render.c gimpdisplayshell-scroll.c gimpdisplayshell-scroll.h gimpdisplayshell-selection.c gimpdisplayshell-transform.c gimpdisplayshell-transform.h')
group('overlay','app/display','gimpdisplayshell-overlays.cpp gimpdisplayshell-overlays.h gimpdisplayshell.c gimpdisplayshell.h gimpdisplayshell-callbacks.c gimpdisplayshell-tool-events.c')
group('window','app/display','gimpdisplay.c gimpdisplayshell-close.c gimpimagewindow.c gimpimagewindow.h')
group('pressure','app/display','gimpmotionbuffer.c')
group('toolbar','app/display','gimpstatusbar.c')
group('window','app/gui','gimpuiconfigurer.c gui-vtable.c gui.c')
group('brush-editor','app/tools','gimpmypaint-gui-base.hpp gimpmypaintbrusheditor.cpp gimpmypaintbrusheditor.hpp gimpmypaintbrushoptions-gui.cpp gimpmypaintbrushoptions-gui.h gimpmypaintoptions-gui.cpp gimpmypaintoptions-gui.h')
group('brush-tool','app/tools','gimpmypainttool.cpp gimpmypainttool.h')
group('ruler','app/tools','gimpperspectiveguidetool.cpp gimpperspectiveguidetool.h gimpdrawtool.c gimpdrawtool.h')
group('fill','app/tools','gimpbucketfillbrushtool.cpp gimpbucketfillbrushtool.h')
group('placeholder','app/tools','gimpimagegeneratortool.cpp gimpimagegeneratortool.h')
group('tool-group','app/tools','gimp-tools.c gimp-tools.h')
group('editability','app/tools','gimppainttool.c')
group('core-hooks','app/tools','gimptool.c gimptool.h')
group('brush-list','app/widgets','gimpmypaintbrushfactoryview.c gimpmypaintbrushfactoryview.h gimpmypaintbrushselect.c gimpmypaintbrushselect.h')
group('brush-editor','app/widgets','gimpmypaintbrusheditor.cpp gimpmypaintbrusheditor.h gimpcurveview.c gimpcurveview.h')
group('layer-tile','app/widgets','gimplayertileview.cpp gimplayertileview.h')
group('layer-popup','app/widgets','gimplayerpopup.cpp gimplayerpopup.h')
group('tool-tile','app/widgets','gimptooltileview.cpp gimptooltileview.h')
group('popup','app/widgets','gimpcellrendererpopup.cpp gimpcellrendererpopup.h popupper.cpp popupper.h gimppopupbutton.c gimppopupbutton.h')
group('color-ui','app/widgets','gimpfgbgeditor.c gimpfgbgeditor.h gimppaletteview.c gimppaletteview.h')
group('tool-group','app/widgets','gimptooleditor.c gimptooleditor.h gimptoolbox.c gimptoolpalette.c')
group('toolbar','app/widgets','gimptooloptionstoolbar.c gimptooloptionstoolbar.h')
group('window','app/widgets','gimpdock.c gimpdock.h gimpdockable.c gimpdockbook.c gimppanedbox.c gimpsessioninfo-dock.c gimpsessioninfo-dock.h')
group('pressure','app/widgets','gimpdeviceinfo-coords.c gimpdevicemanager.c')
group('overlay','app/widgets','gimpoverlaybox.c gimpoverlaychild.c')
group('canvas-transform','app/widgets','gimpnavigationview.c')
group('widget-helpers','app/widgets','gimpcellrendererviewable.c gimpcontainerbox.c gimpcontainertreeview-dnd.c gimpcontainertreeview-private.h gimpcontainertreeview.c gimpcontainertreeview.h gimpdatafactoryview.c gimpdnd.c gimpdnd.h gimpdynamicsoutputeditor.c gimpeditor-cxx.cpp gimpeditor-cxx.h gimpeditor-private.h gimpeditor.c gimpeditor.h gimpitemtreeview.c gimplayertreeview.c gimpselectiondata.c gimpselectiondata.h gimpviewrenderer.c gimpviewrenderer.h gimpwidgets-constructors.c gimpwidgets-utils.c gimpwidgets-utils.h widgets-enums.h widgets-types.h')
group('icons','app/widgets','gimptitlebaricon-pixbuf.h')
group('translation','app/widgets','gimphelp-ids.h')
group('xcf-read','app/xcf','xcf-load.c xcf-private.h xcf.c')
group('xcf-write','app/xcf','xcf-save.c')
group('brush-list','app/actions','mypaint-brush-editor-actions.c mypaint-brush-editor-actions.h mypaint-brushes-actions.c mypaint-brushes-actions.h')
group('menus','app/actions','actions.c dialogs-actions.c layers-actions.c layers-commands.c layers-commands.h tool-options-actions.c')
group('mode-enum','app/actions','context-commands.c')
group('invalidation','app/actions','view-commands.c')
group('menus','app/dialogs','dialogs-constructors.c dialogs-constructors.h dialogs.c')
group('config','app/dialogs','preferences-dialog.c')
group('menus','app/menus','menus.c')
group('core-hooks','app/file','file-open.c')
group('mode-enum','libgimp','gimpenums.c.tail gimpenums.h')
group('pdb-select','libgimp','gimp_pdb_headers.h gimpmypaintbrushselect_pdb.c gimpmypaintbrushselect_pdb.h')
group('config','libgimpbase','gimpbaseenums.c gimpbaseenums.h')
group('cpp-header','libgimpcolor','gimpcairocolor.h')
group('cpp-header','libgimpconfig','gimpconfig-params.h')
group('color-ui','modules','gimpcolorwheel.c gimpcolorwheel.h')

# Families have one concrete migration role. Unknown app paths are errors.
def path_profile(path):
    p = PurePosixPath(path)
    if path in FILES: return FILES[path]
    if path == 'menus/Makefile.am': return 'registration-2365'
    if p.name == '.gitignore': return 'cleanup'
    if path == 'data/mypaint-brushes/Makefile.am.skel': return 'manifest-generator'
    if path == 'plug-ins/script-fu/scheme-wrapper.c': return 'scheme-modes'
    if path == 'tools/pdbgen/enums.pl': return 'mode-enum'
    if p.name.startswith('Makefile.am'):
        return 'assets-manifest' if path.startswith(('data/','themes/','etc/')) else 'build'
    if path == 'configure.ac': return 'build-config'
    if path.startswith('app/composite/'): return 'composite'
    if path.startswith('app/httpd/'): return 'http'
    if path.startswith('app/plug-in/'): return 'runner'
    if path.startswith('app/tools/'): return 'toolbar'
    if path.startswith('libgimpwidgets/'):
        return 'icons' if p.name in ('gimpstock.c','gimpstock.h') else 'widget-helpers'
    if path.startswith('build/flatpak/'): return 'packaging'
    if path.startswith('data/mypaint-brushes/'):
        if p.suffix == '.myb': return 'assets-brush'
        if p.suffix == '.png' or path.endswith('.xcf.gz'): return 'assets-preview'
        if p.suffix == '.sh': return 'scripts'
        if p.name.lower() == 'readme.txt': return 'assets-license'
        if p.name == 'files': return 'assets-manifest'
    if path.startswith('data/layer-presets/') and p.suffix == '.json': return 'assets-preset'
    if path.startswith('themes/') and p.suffix == '.png': return 'icons'
    if path.startswith('menus/'): return 'menus'
    if path.startswith('po/'): return 'translation'
    if path.startswith('plug-ins/'): return 'plugin-procedure'
    if path.startswith('tools/pdbgen/'): return 'pdb-select'
    if path == 'etc/toolrc': return 'tool-group'
    if path == 'etc/sessionrc': return 'window'
    if path.endswith('.h') and path.startswith(('app/core/','app/config/','app/widgets/')): return 'cpp-header'
    raise ValueError('Unreviewed source path: '+path)

# Semantic exceptions for mixed files. They reference the zero-context hunk index.
OVERRIDES = {
    'app/actions/actions.c': {1:'layer-tile',2:'brush-list',3:'brush-list',4:'layer-tile',5:'layer-tile',6:'layer-tile',7:'layer-tile'},
    'app/actions/dialogs-actions.c': {1:'preset-gui',2:'brush-list',3:'preset-gui'},
    'app/actions/layers-commands.c': {1:'menus',2:'mode-enum',3:'menus',4:'menus',5:'menus'},
    'app/core/gimpgrouplayer.c': {3:'invalidation',8:'invalidation'},
    'app/core/gimpimage.c': {1:'ruler',2:'ruler',3:'ruler',4:'ruler',5:'invalidation',6:'invalidation',7:'invalidation',8:'invalidation',9:'invalidation'},
    'app/core/gimptooloptions.c': {7:'tool-delete'},
    'app/core/core-types.h': {i:'core-type-declarations' for i in range(1,6)},
    'app/core/gimp.c': {1:'resource-registration',2:'tool-group',3:'factory-registration',4:'tool-group',5:'resource-registration',6:'resource-registration',7:'resource-registration',8:'factory-registration',9:'resource-registration',10:'tool-group',11:'factory-registration'},
    'app/core/gimp.h': {1:'resource-registration',2:'core-type-declarations',3:'tool-group',4:'tool-group',5:'factory-registration'},
    'app/tools/gimp-tools.c': {3:'fill',4:'placeholder',5:'ruler',6:'brush-tool',9:'toolbar',11:'tool-registration',12:'tool-registration',13:'placeholder',19:'toolbar',20:'toolbar',21:'toolbar',22:'toolbar',28:'toolbar',29:'toolbar',30:'toolbar',38:'toolbar',40:'toolbar'},
    'app/display/gimpdisplayshell-tool-events.c': {
        **{i:'view-gesture' for i in (1,3,4,6,7,19,22,25,28,29,30,31,32,43,44,45,46,47)},
        **{i:'ruler-input' for i in (2,5,9,20,21,24,33,34,35,36,37,38,39,42)},
        **{i:'event-compression' for i in (8,10,26,27,51,52)},
        **{i:'cleanup' for i in (11,12,18,50)},40:'mirror-key',48:'toolbar',49:'toolbar',
    },
    'app/widgets/gimpwidgets-constructors.c': {i:'mode-enum' for i in range(1,6)},
    'app/widgets/gimpdnd.c': {i:'tool-group' for i in range(1,16)},
    'app/widgets/gimpdnd.h': {1:'tool-group'},
    'app/widgets/gimpselectiondata.c': {i:'tool-group' for i in range(1,5)},
    'app/widgets/gimpselectiondata.h': {1:'tool-group'},
    'app/dialogs/dialogs-constructors.c': {1:'brush-list',2:'brush-editor',3:'brush-list',4:'brush-list',5:'brush-editor'},
    'app/dialogs/dialogs.c': {1:'brush-list',2:'brush-editor'},
    'app/core/core-enums.c': {1:'preset-apply',2:'brush-undo',3:'brush-undo',4:'preset-apply',5:'brush-undo',6:'brush-undo',7:'smudge',8:'smudge'},
    'app/core/core-enums.h': {1:'preset-apply',2:'brush-undo',3:'brush-undo',4:'smudge',5:'brush-context',6:'brush-context',7:'brush-context',8:'brush-context'},
    'app/core/gimpcontext.c': {i:'cpp-header' for i in (11,14,15,23,25,26,27,28,29,30,31,32,33)},
    'app/core/gimpcontext.h': {i:'cpp-header' for i in (2,4,5,7,8)},
    'app/paint/gimppaintcore.c': {1:'cleanup',2:'cleanup'},
    'app/paint/gimppaintcore.h': {1:'pressure'},
    'app/paint/gimppaintoptions.c': {1:'pressure',8:'cleanup'},
    'app/paint/gimppaintoptions.h': {1:'pressure',5:'pressure'},
    'app/paint/gimpbrushcore.c': {3:'cleanup',4:'cleanup',7:'cleanup',12:'smudge',15:'cleanup',18:'pressure',19:'pressure',20:'pressure',29:'smudge'},
    'app/paint/gimpbrushcore.h': {3:'smudge'},
    'app/widgets/gimpdevicemanager.c': {1:'device-identity'},
    'app/widgets/gimpdynamicsoutputeditor.c': {1:'cleanup',2:'cleanup',3:'cleanup'},
    'app/core/gimpimage-contiguous-region.c': {
        **{i:'fill-iterator' for i in (1,3,4,6,25,26,27,28,29,30,34,35,36,37,38,41,42,43,44,45,46,47,48,51,52,53,54,57,58,59,60,61,62,63)},
        **{i:'fill-coverage' for i in (2,5,17,19,21,22,23,24,31,32,39,40,49,50,56)},
        **{i:'fill-input' for i in (8,9,10,11,12,13,14,15,16,18)},
        **{i:'cleanup' for i in (7,20,33,55)},
    },
}


# Exact hunk exceptions, reviewed against byte-verified pinned source/base blobs.
OVERRIDES['configure.ac'] = {
    1: 'configure-001888',
    2: 'configure-001889',
    3: 'configure-001890',
    4: 'configure-001891',
    5: 'configure-001892',
    6: 'configure-001893',
    7: 'configure-001894',
    8: 'configure-001895',
    9: 'configure-001896',
    10: 'configure-001897',
    11: 'configure-001898',
    12: 'configure-001899',
    13: 'configure-001900',
}

OVERRIDES.setdefault('app/Makefile.am', {}).update({3: 'registration-3', 4: 'registration-4', 5: 'registration-5', 6: 'registration-6', 7: 'registration-7', 8: 'registration-8'})
OVERRIDES.setdefault('app/actions/Makefile.am', {}).update({1: 'registration-9', 2: 'registration-10'})
OVERRIDES.setdefault('app/base/Makefile.am', {}).update({1: 'registration-38'})
OVERRIDES.setdefault('app/core/Makefile.am', {}).update({2: 'registration-129', 4: 'registration-131'})
OVERRIDES.setdefault('app/display/Makefile.am', {}).update({3: 'registration-457'})
OVERRIDES.setdefault('app/paint-funcs/Makefile.am', {}).update({1: 'registration-868'})
OVERRIDES.setdefault('app/paint/Makefile.am', {}).update({3: 'registration-887'})
OVERRIDES.setdefault('app/pdb/Makefile.am', {}).update({2: 'registration-1009'})
OVERRIDES.setdefault('app/tools/Makefile.am', {}).update({1: 'registration-1081', 3: 'registration-1083', 4: 'registration-1084'})
OVERRIDES.setdefault('app/widgets/Makefile.am', {}).update({1: 'registration-1514', 4: 'registration-1517', 5: 'registration-1518', 6: 'registration-1519', 9: 'registration-1522'})
OVERRIDES.setdefault('libgimp/Makefile.am', {}).update({1: 'registration-2293', 2: 'registration-2294'})
OVERRIDES.setdefault('menus/Makefile.am', {}).update({1: 'registration-2365'})
OVERRIDES.setdefault('tools/pdbgen/Makefile.am', {}).update({1: 'registration-2484'})

# These provider/caller hunks expose the editor DSL's raw widget boundary or
# keep its two item-tree callers inactive. Native GTK constructors, model and
# layout changes elsewhere do not inherit a Definer/Packer ownership duty.
GTK_DSL_SUPPORT = {
    'app/widgets/gimpeditor-cxx.h': {1},
    'app/widgets/gimpeditor-private.h': {1},
    'app/widgets/gimpeditor.c': {1, 2, 3, 4, 5, 6},
    'app/widgets/gimpitemtreeview.c': {1, 4, 5, 6, 7},
}

# Only these reviewed widget hunks reach the popover's Connection owner.
# Declarations, native C signal handlers and unrelated GTK layout/model changes
# do not exercise the original 07.008 freed-emitter destructor contract. Keep
# the common cpp-signals provider and every other lifetime/feature duty intact.
EMITTER_TEARDOWN_TRIGGERS = {
    'app/widgets/gimpeditor-cxx.cpp': {1},
    'app/widgets/gimpcontainertreeview.c': {8},
    'app/widgets/gimplayertreeview.c': {7},
}


def uses_gtk_dsl(path, lines):
    """Recognize the actual C++ DSL, ignoring names in comments and strings."""
    if PurePosixPath(path).suffix not in ('.cpp', '.hpp', '.h'):
        return False
    code = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  '', '\n'.join(lines), flags=re.S)
    if re.search(r'\bGLib\s*::\s*(?:with\s*\(|Definer\s*<)', code):
        return True
    imported = re.search(r'\b(?:using\s+namespace\s+GLib\s*;|namespace\s+GLib\s*\{|using\s+GLib\s*::\s*(?:with|Definer)\s*;)', code)
    return bool(imported and re.search(r'\b(?:with\s*\(|Definer\s*<|class\s+Definer\b)', code))


def route(row, added, removed):
    path = row['path']
    key = OVERRIDES.get(path, {}).get(int(row['index']), path_profile(path))
    # Only classify type declarations as header glue if every changed nonempty
    # line is one of the visible wrapper constructs; never swallow real APIs.
    lines = [l.strip() for l in added+removed if l.strip()]
    allowed = re.compile(r'^(?:#(?:ifn?def|endif|include)|extern "C\+\+"|__DECLARE_GTK_|[{};]+)')
    if lines and any('__DECLARE_GTK_' in l for l in lines) and all(allowed.match(l) for l in lines):
        key = 'cpp-header'
    result = dict(PROFILES[key]); result['profile'] = key
    # The first gimp.h hunk adds only a GimpMypaintInfo pointer. The second
    # adds the actual MyPaint GimpDataFactory pointer and factory lookup table.
    # Correct only the resource-type gate here; other assigned obligations
    # retain their independent, still-unresolved acceptance requirements.
    if path == 'app/core/gimp.h' and int(row['index']) == 1:
        result['tasks'] = [task for task in result['tasks'] if task != '08.005']
    if path == 'app/core/gimp.h' and int(row['index']) == 2:
        result['tasks'] = [*result['tasks'], '08.005']
    # Hunk 12 registers only the perspective-guide tool. Its 08.010 duty
    # remains; do not infer a MyPaint type obligation from the broad profile.
    if path == 'app/tools/gimp-tools.c' and int(row['index']) == 12:
        result['tasks'] = [task for task in result['tasks'] if task != '08.008']
    dsl = uses_gtk_dsl(path, added + removed)
    support = int(row['index']) in GTK_DSL_SUPPORT.get(path, set())
    result['tasks'] = [task for task in result['tasks']
                       if task != '06.023' or dsl or support]
    if dsl and '06.023' not in result['tasks']:
        result['tasks'].insert(0, '06.023')
    if key in ('widget-helpers', 'cpp-gtk'):
        trigger = int(row['index']) in EMITTER_TEARDOWN_TRIGGERS.get(path, set())
        result['tests'] = [test for test in result['tests']
                           if test != '07.008' or trigger]
    return result
