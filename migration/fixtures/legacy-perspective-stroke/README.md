# Old source-only stroke constraint oracle

The complete two functions are unchanged bytes from pinned
`afa43fae3e920210146abed514f136fd49f671b5`, DisplayShell tool-events blob
`b670161eb5906349f84a51da961ae339b7209574`. Source/body hashes and line ranges
are checked. The adapter supplies only the old shell's scale, origin, angle and
pending fields, renames functions/type and suppresses debug printing. Model
accessors use current model data with default angle zero, explicitly excluding
the previously documented old angle/id getter defect. No old runtime is run.

The original arithmetic uses a32px scaled-input threshold, strict first-minimum
angular priority, and dominant-axis line constraint (not orthogonal projection).
The original event branch at1250–1293 clears pending and begins at the origin
without delivering that threshold-crossing sample to motion. Native tests assert
this ordering and use the unchanged lazy function on subsequent samples.

Native viewport conversion is deliberately shared before invoking this oracle.
It verifies old selection/constraint arithmetic and actual native event routing,
not old-versus-new viewport-matrix parity or physical device behavior.
