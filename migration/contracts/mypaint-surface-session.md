# Extended MyPaint factory, Surface and native session contract

## Increment scope

This follows resource/evaluator checkpoint `bddb233e2f`. It wires the dedicated
GimpPainterMybrush data factory, installs all 177 pinned brushes and adds the
legacy raster/resource/session path. Standard GimpMybrush/libmypaint remains
registered and its factory is not replaced. No editor/tool completion is claimed.

## Factory

`painter-mypaint-brush-path` and `painter-mypaint-brush-path-writable` use the
standard config/data factory path contract with a distinct directory. Extension
`.myb` directories are intentionally shared through `mypaint-brush-paths`. Load,
refresh, writable duplicate, rename, save, delete, tags, memsize, clean and teardown
participate in normal GIMP factory lifecycle. Native test checks 177 loads,
178 after edit/save/reload, 177 after delete and unchanged standard factory count.
Unknown settings and missing ordinary resource names survive that round trip.
The test restores original path configuration before deleting its temporary folder.

A generic shutdown writable/search-path warning also occurs in preexisting native
harness logs from before this increment. Test-mode gimp_config_build_data_path
uses source-only directories whereas writable defaults use the user directory.
The warning is retained in raw logs; successful memory instrumentation does not
imply a warning-free whole application shutdown. No diagnostic was suppressed.

## Raster/resources

See `app/paint/painter-mypaint-surface/README.md` for exact source provenance and
arithmetic. Surface owns GEGL buffers; ordinary-resource providers retain shared
resource state and balanced brush begin/end-use. Dirty signal connections use the
common weak ownership/Connection abstraction. Missing references remain editable
strings; stroke fallback and preview fallback intentionally follow different old
paths. Paper preserves the old first byte channel and dirty invalidation.

Only nonlinear RGBA-u8 target buffers are implemented here. Other formats raise a
diagnostic exception before a native transaction begins. Invalid source masks and
unsafe/nonfinite dimensions also fail explicitly; no silent approximated brush.
Arbitrary 8192 bitmap and 4000 generated helper limits were removed: representable
pixel/fixed-point checks and the actual old direct-setter ranges are used instead.

## Native transaction controller

A plain C++ PaintCore retains current GimpPaintCore, options, drawable and image
via typed ObjectRef. The image lease is separate because item→image is weak. There is no parallel GObject implementation system. Future GObject
adapters must use the established named handle plus one BindingStore typed slot.
The controller validates resource/format first, starts a real native transaction,
uses its Undo snapshot, translates selection offsets, applies FG/BG and layer
alpha lock, and forwards all stationary pressure/time events. Finish commits one
Undo unit; cancel and destruction restore the native snapshot; repeated cancel is
safe. Notification callbacks lease owner, native core and drawable. An ending
state rejects reentrant motion/configuration while previews thaw and buffers clear.
A separate starting guard rejects recursive motion/configuration during native
preview-freeze notification. Cancel or finish during start is deferred until native
start returns, then closes the still-empty transaction without painting or Undo.
Image leases span native start/finish/cancel and their synchronous callbacks.
End-callback tests are armed after painting so the old evaluator’s automatic empty
first-event split cannot accidentally substitute for the real commit under test.
The Surface publishes active only after snapshot validation/allocation succeeds.

The legacy core overrides color/lock-alpha after new-stroke and its setters refresh
speed caches. Engine gained explicit finite runtime setters reproducing this order;
existing independent evaluator goldens must still pass. Options notify splitting,
context model bindings, multiple selected drawables, symmetry and registered
paint-core/tool/editor adapters remain later gates.

## Evidence and exact boundaries

- Sealed independent old runtime package: 148 Surface records, all bytes/sample
  values/dab returns compared exactly, 24 bitmap masks and 72 generated masks.
- Transparent lock-alpha capture records hidden black on touched zero-alpha
  incremental pixels and unchanged nonincremental target. The new code defines
  those measured bytes without the old undefined NaN-to-byte conversion.
- Nine native cases: both accumulation modes with exact Undo/Redo restoration;
  cancel/repeated cancel/destruction and preview balancing; stationary selection;
  resource missing-name/lifetime; paper invalidation; rejected snapshot/precision;
  transparent-lock and >8192 resource regression; start reentry/deferred cancel;
  last caller image-ref release at freeze/thaw/Undo dirty; layer removal at
  freeze/thaw. Image finalization and preview balance are asserted explicitly.
- `mypaint-surface-sanitizers.json`: nine native cases pass with 22 source files
  instrumented for AddressSanitizer, UndefinedBehaviorSanitizer and explicit
  float-cast-overflow; remaining GIMP/dependencies uninstrumented. RTTI is consistent
  across instrumented C++ objects. LeakSanitizer is disabled under the sandbox.
  Private rewritten thin archives cover all aliases; production objects untouched.
- Full application link and focused Meson results are recorded separately.

A subsequent independently sealed full-session comparison now passes for 32
warmed pinned-old scenarios and 96 complete native RGBA snapshots. See
`mypaint-full-session-comparison.md`. The distinction remains important: these
warmed old references do not establish old cold-start equivalence (cold old
nonincremental crashes), arbitrary ICC/high precision, brush pipes, all177 rendered
brushes, live GUI/hardware or Windows/macOS behavior.
