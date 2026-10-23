# Original14.003/gegl-source-node: actual projection boundary

The existing implementation already updates the clone source node. This selected
model test checks the final image projection: one-pixel source update at a clone
with different source/clone offsets; unchanged neighboring pixel; clone movement
and disappearance from its old location; explicit detach followed by old-source
recolor. Detach retains the prior cache under the pinned legacy contract, rather
than clearing it. Seven exact RGBA pixel assertions pass.

This is a selected native model-only primary-build checkpoint, not clean whole
HEAD, UI, sanitizer or aggregate proof. No product source changes. The broader
exploratory pick/lock probes are separate original tasks and are not registered
in this checkpoint. Clean-build/sanitizer validation is queued. Original criterion
and dependency chain remain unchanged; this checkpoint is DOING, not DONE.
