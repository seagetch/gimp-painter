# Same-extent matrix transformation — original 15.005 checkpoint

The pinned old `afa43fae` Filter transform delegates to its parent then unconditionally resets and invalidates (old source hash and excerpt included). Current ABI removes the old recursion-level argument.

An exact integer affine 180-degree matrix on an ordinary16×16 Gaussian Filter keeps source and extents unchanged. Red invoked native parent (buffer notification1), changed the immediate derived cache from red to blue, but generation stayed2→2 and the scheduling assertion aborted. The narrow transform override follows the committed rotate ownership pattern: retain owner/image, delegate without borrowing FilterImpl across native callbacks, recheck binding epoch, invalidate. Green advances2→3 and recomputes the unchanged lower-source red pixel. No generic own-buffer observer, scale/resize change, or Clone geometry path is introduced.

**Normal5 PASS:** matrix, rotate, flip including whole-image Undo/Redo, latest-lower restore and actual RUNNING duplication. **Selected ASan/UBSan3 PASS:** matrix, rotate, flip;20 instrumented and53 RTTI-only units, remaining dependencies uninstrumented, LSan off. This is model/API evidence, not new UI/screenshot evidence.

Source base20b2204b11df079c306a680b41ac623f3be75c76 plus explicit Options mode overlays and previously sealed Filter fixtures/rotate provider, followed by the four owned matrix changes. Red/green receipts and119-file green archive seal exact exercised inputs. Earlier rotate generator timestamp event occurred before this baseline; this proof makes no old generated-stamp invariance claim. All held providers and Clone remain committed baseline bytes; no old Edge execution. Source seals show no drift through tests. The flip fixture now expects delegated rotate and matrix while retaining inherited scale/resize; historical receipts remain immutable.

Original15.005 remains DOING: translate/scale/resize audit and full original dependencies remain open. No broader arbitrary transform, precision, mask or full-HEAD aggregate claim.
