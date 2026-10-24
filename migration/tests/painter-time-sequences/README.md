# Original23.009: registered MyPaint timestamp policy

Original acceptance **「間引き・逆行・同時刻を処理」**, dependency23.008, remains OPEN/DOING. This bounded proof does not close event thinning/GDK history, old device behavior, dependency chain, physical tablet timing, or other platforms. No production implementation changed.

## Active old and current behavior

Pinned old `afa43fae3e920210146abed514f136fd49f671b5` active MyPaint tool uses unsigned `(time-last_event_time)/1000.0`, a positive-time sentinel, and drops zero-duration calls. Its Python negative-time clamp/equal-event replay is commented out, not an active feature. `source-audit.json` preserves exact original SHA, active/commented excerpts and current provider source.

Current native tool deliberately keeps same-time pressure samples, treats reverse/ambiguous intervals above INT32_MAX as zero, accepts the inclusive half-range boundary, and handles small UINT32 wrap through modular subtraction. Its explicit have_time handles timestamp zero. This is not a claim of old equal/reverse evaluator trajectory parity.

## Native proof

Seven cases: ordinary, same-time, reverse then resume, UINT32 wrap, INT32_MAX+1 rejection, timestamp-zero start, inclusive INT32_MAX acceptance. Each invokes the actual registered MyPaint tool directly, forwards the tool→session C call through an observing link wrapper **without replacing any argument or implementation**, and asserts exact integer-milliseconds/1000 equality, coordinates, pressure, and call count/order.

Each sequence is compared with a second real native scene having normalized absolute times and identical expected deltas. Actual raster bytes match exactly; every scene paints nonzero alpha (190–262 pixels), commits one native Undo, and reproduces exact pixels on Undo/Redo. This is direct native callback input, **not queued GDK input or real tablet hardware**.

The accepted24.8-day interval is bounded by the existing evaluator reset before its dab loop. With this fixture resource (40dabs/second, exp(1.5) radius, small coordinates), pre-reset count is finite (~85.9million float), then fixed STATE_COUNT reset returns; it does not iterate that count. The test asserts no new pixels from that reset step, followed by normal painting. The normalized reference preserves this reset/split trajectory rather than pretending it is an ordinary short interval.

**7 normal +7 focused ASan/UBSan cases pass.** Instrumented scope is the fixture plus the single native MyPaint tool provider; app dependencies/evaluator are uninstrumented and LSan is disabled. Three private provider mutations fail causally: removing reverse-time bound, changing inclusive bound to exclusive, and dropping zero-duration calls. No main provider or clean source was mutated.

## Image and limitations

`timestamp-wrap-oncanvas-portrait.png` is the actual rendered1080×1920 Xvfb application with oncanvas UI enabled, showing the real wrap-sequence stroke after presentation-only projection flush and5×zoom. The initial screenshot had a blank canvas view despite actual drawable alpha; its receipt/image are retained. Presentation was refined without changing input timing or product code. This screenshot is **not automatic projection-notification proof**. Existing theme/icon/session-bus warnings are retained.

Host is exact released `0a775df4135052a2c851c9a5dfa37cea25887268`. Link/runtime/source inputs are hashed before/after. Shared clean read lock was held through each build/runtime. Current source pins are intentionally strict; future changes require explicit successor verification rather than silently resealing old evidence.

Meson test `painter-time-sequences` accepts ordinary CI monitor dimensions. Our proof runner opts into `GIMP_TEST_REQUIRE_PORTRAIT=1` and uses the repository authenticated1080×1920 Xvfb runner; geometry checks remain conditional, never forcing desktop layout in ordinary CI. `validate.py` checks receipts/pins; `test_validator.py` checks rejecting semantic controls. Original23.009 and its prerequisite remain open.

CI geometry controls use the same final normal binary under authenticated1920×1080 Xvfb: ordinary case passes without the portrait opt-in; the same case with `GIMP_TEST_REQUIRE_PORTRAIT=1` rejects width1920≠1080. The original7+7 portrait receipts are unchanged.
