# C39: finish test icon-theme setup before restoring windows

Local candidate only, based on published C38
c525f5e5a03f8ebe125a5ae1f2efe54d6c9553c2. Task308 remains DOING / NOT ACCEPTED.
No native latency fix, leak repair or publication is claimed.

## Run56 observation and correction of the earlier hypothesis

Run41570069455 completed five successful and three failed jobs. Normal Brush21/21
passes at both scales; the isolated sanitizer first wait still fails after6.591s,
stable0. Other20 leaf assertions pass but processes leak198738–201378 bytes.
The phase trace covers one original GimpImageWindow,20 events, no missing hooks,
clipping, destruction or clock replacement. It becomes active (0→1), while flags
remain128 and no state-flags-changed event occurs. Backdrop/deactivation is therefore
not supported by this trace. Icon-theme changed, layout, style-updated and resize
are delivered during the private fixture's idle drain. Another expensive original
window layout occurs in the first wait without another theme/style event.
Ordering is not causation; RUN_FIRST hooks run after their class closures.

## Concrete test setup defect

The activation callback in app/tests/tests.c calls gui_init first. That initializes
themes; themes_apply_theme calls icon_themes_current_prefer_symbolic, whose icon
lookups load the theme. The test then prepends its build-tree icon search path,
invalidating that already-loaded theme. It next initializes/restores windows and
runs g_test_run synchronously, with no main-context delivery between the late path
change and construction. Thus a test-only queued theme invalidation can reach newly
constructed UI during the first isolated case.

GTK3's do_theme_change invalidates lookup caches and queues a high-priority idle.
That idle emits changed and resets widget styles. Source reference:
https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkicontheme.c
This source mechanism and a real headless GtkIconTheme regression establish the
ordering defect; they do not yet prove it accounts for the complete run56 delay.

## Minimal harness repair and explicit bounded phase

After the same icon-path setup, before gimp_initialize/gimp_restore, finish pending
setup work through a default-context idle marker. The path order and real fixture
icons remain unchanged. The implementation uses a dedicated small helper with a
high-priority3second timeout, balanced source cleanup and a post-dispatch elapsed
check. GLib cannot preempt a synchronous callback: expiry is enforced when it
returns, not as a strict wall-clock interruption. Missing idle completion, timeout, or nonpreemptive overrun fails setup.
There is no retry, sleep, frame wait, prewarm, focus change, icon substitution or
suppression of later theme changes. No startup image window is required or created
by the helper, and it does not query window state. This pumps the default context, not only
theme callbacks; early reentrancy is an explicit consequence. Source review found
external open/activation consumers guard gimp_is_restored(), still false here.

This is an additional explicitly bounded initialization phase, not proof of faster
whole-application startup. It is earlier than window restoration, unlike C36's
unsuccessful post-startup frame barrier. All private test construction, original
3second readiness deadline, stable assertions and leak guards remain unchanged.
All26 protected common files are unchanged. app/tests/tests.c is shared test harness
source, so all GUI test activation paths using this callback receive the fix; it
is not a production application behavior change. Its safety relies on ordinary GUI
initialization having completed, while application restoration has not started.

## Proof required and evidence limits

The local proof uses real GtkIconTheme/GLib objects without GTK display or GIMP.
Old ordering delivers changed after a simulated construction sentinel; corrected
ordering delivers it first. Coalesced and no-pending cases are tested. A subsequent
genuine path change still emits changed after construction, proving it is not
suppressed. The exact helper is compiled against GLib ABI declarations; high-priority
starvation and a3.1second nonpreemptive callback both fail its bounded setup.
Source assertions retain exact original popup helpers/private source and protected26.
This is not native GIMP build, widget-rendering or sanitizer acceptance evidence.

Hosted C39 must still show unchanged normal gates and isolated case14 behavior.
The existing phase diagnostic should show whether changed still first appears after
private construction; if so, this explanation for that observed event is falsified.
Do not add more waits to cover a remaining directory-mtime rescan or other cause.
Even an improved private wait cannot erase remaining leaks, matched-comparison
limitations or separate babl pkg-config failure. Publication remains root-controlled
and requires exact authorization under the accepted-parent seal.

## Completed local review and tests

Full Brush source/synthetic suite:1140 tests pass. Focused theme setup suite:6 tests
pass, including real headless GTK/GLib old/new ordering and failure negatives.
The four pure source contracts also run in CI bootstrap; this adds no GTK package
requirement to bootstrap. Existing workflow prefix integrity hash is refreshed for
that one additional source-check command; all native jobs and policies are unchanged.
WBS, diff whitespace and protected-source checks pass. Independent review found
no required correction and independently repeated the focused6 tests.
Fresh native GIMP build/runtime, primary latency and leak acceptance remain pending.
