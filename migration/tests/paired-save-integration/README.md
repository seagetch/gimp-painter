# Paired history integration with ordinary tool saving

The exact committed aggregate exposed an actual regression not caught by the
original bounded paired model/UI suite. An open interactive stroke is normally
finishable by the image `saving` signal; its preflight query must not report the
stroke as an unsafe in-progress sample. The new paired controller had treated
all started transactions as pending, preventing that ordinary save path.

The controller now distinguishes native Undo/Redo using the existing native
pop-active scalar. A narrow read-only internal C query exposes that marker;
both operations set it before pending-paint callbacks and retain it through
preflight, stack transition, notifications and reverse finish, clearing it
before releasing the image. The exact already-reviewed get_data fingerprint
count increases from1 to2, with no new signal, class ABI, pointer ownership or
second state store. Transient starting/processing/ending, batches and history
leases remain pending for saves; an idle interactive stroke blocks Undo/Redo
but can be finished normally for saving. Ordinary Undo entries are covered,
not only the new typed subclass.

After fixing preflight, the unchanged tool test found a second regression:
prepared publication retains active transaction ownership until commit, so its
own dirty notification during `image_saving` looked like an unrelated external
edit and halted the tool. The saving callback now uses the same existing Tool
Operation exclusion/owner lease as input processing. Physical release after
save remains valid; genuine external dirty notifications still cancel. Actual
save-time disposal remains safe, leaves one committed history entry and permits
pixel Undo with the original controller gone. The original assertions were not
inverted, skipped or softened. Both causal red logs are retained.

## Final proof

* Full native MyPaint tool27, paired18, existing Surface24, typed native bridge16
  and plain-environment paired UI1 all pass in authenticated portrait Xvfb.
* Selected16-unit ASan/UBSan runs paired18 and tool27 (45 cases), instrumenting
  the actual changed tool provider, session and controller/native history code.
  No dependency/full-application/vptr/leak-sanitizer claim.
* Two controller mutants distinguish blocking legitimate save from allowing
  active ordinary Undo. A third compiled tool mutant removes the saving guard
  and reproduces the actual later-release critical assertion.
* C11 and C++14/noRTTI first/double header probes: prior missing internal query
  fails, current declaration compiles. Runtime native callers exercise the
  actual symbol, not merely a compile-only substitute.
* Exact isolated source gate passes2513 files/718 reviewed sites; four dormant
  findings are unchanged. Nine held providers are committed-baseline sealed.
* Final linked facade3 and selected7-unit ASan3 replay pass. Its original source
  binding had not drifted; this is additional integration coverage, not a reason
  to reseal prior evidence or broaden its compatibility claims.

Moved standalone instrumented/controlled binaries initially selected installed
libgimpwidgets rather than the just-built library, causing a missing-symbol
loader failure. These diagnostics are retained. Reruns use the identical local
libgimp directories supplied by Meson; no product code bypass or relaxed test.

The paired UI default-directory repair has its separate immutable four-PNG
receipt and source validator. The previous a84cd exact aggregate failure stays
preserved. This corrects and re-closes only the supported fixed-bounds paired
child; the original full parent remains open for externally resized/expanded
geometry cancellation observer ordering. No native global cancellation or
allocator guarantee has been invented.
