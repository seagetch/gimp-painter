# Bounded clipped-alpha initial/update proof

Extend the unchanged independent old-source accumulator-center/composition oracle
to YA/RGBA partial alpha96/200 and fully transparent pixels with nonzero hidden
colors. Preserve zero-opacity input bytes, first successful overlap seeding, old
integer division/+1/rounding, hidden-color output behavior, and exact Undo/Redo.

No product change is needed if the existing implementation matches. Retain all
original52 source duties/criterion and existing separate blending/size/UI owners.
Source-duty mapping is explicit review, not automatic whole-task completion.
