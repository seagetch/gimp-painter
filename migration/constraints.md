# Non-negotiable compatibility conditions

1. Legacy gimp-painter XCF documents must open through the regular Open path.
   An external conversion step is not an accepted solution.
2. Layer types, values, references, arguments, hierarchy, masks, pixel data,
   and resource metadata must survive edit/save/reopen. Rasterizing the custom
   layer types is a failure.
3. CloneLayer source resolution, live updates, transforms, duplication and Undo
   follow the legacy behavior even if their internal references change.
4. FilterLayer remains a custom layer with nonblocking, dependency ordered
   evaluation and explicit cancellation, commit and convergence checkpoints.
   Do not substitute GIMP's GEGL nondestructive layer effect. GeglBuffer is
   permitted as pixel storage.
5. Extended MyPaint drawing and saved settings must survive as editable state.
   A modern feature sharing the name Smudge or MyPaint is not proof of parity.
