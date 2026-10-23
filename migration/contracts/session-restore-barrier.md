# Native session restoration barrier

This bounded follow-up to `29.010/multiwindow-toolbar-host` addresses genuinely
changed mode/Painter intent while native session dock restoration is incomplete.
The earlier host receipt proves post-startup conversions and same-value feedback,
not this interruption boundary. Targeted native acceptance is recorded in
`migration/tests/session-restore-barrier.json`.

## Required behavior

- The native dialog factory tracks batch enumeration and each synchronous or
  queued dock-fill restoration. A restore lease begins before constructors and
  aux-data callbacks can reenter, and ends only after native cleanup completes.
- A session record cannot be enrolled twice while its saved dock lists are still
  owned by a restoration. Completion notification is not inferred from
  `Gimp::restored` or merely returning from `session_restore()`.
- Destructive mode/Painter topology conversion waits until native restoration is
  complete. Deferred replay reads the latest configuration, preserves net-zero
  requests, and resumes interrupted conversions without dropping native docks.
- Image-window session ownership follows the settled topology rather than each
  early mode notification. Surviving windows are reconciled before readiness,
  including unchanged/net-zero release paths and a newly realized native empty
  window. Deferred participants cannot claim the single-window singleton before
  their merge is complete.
- Readiness includes completed UI replay. A nested main loop cannot consume the
  only replay while conversion is active and leave save/Quit waiting forever.
- Session saving cannot clear saved records still used by a restore callback.
  Save requests are coalesced and performed only after native restoration and UI
  replay settle.
- After ordinary dirty-image decisions permit Quit, an accepted exit waits for
  the same readiness condition, preserving the caller's force argument. Dirty
  Quit cancellation leaves restoration and later replay usable.
- Real disposal cancels queued sources and suppresses replay while allowing
  running native constructors/fills to unwind before their payload or widgets
  are released. Pending restoration must not overwrite saved profiles during
  teardown.

## Native validation

The 33 dedicated native tests change configuration before the first dock-fill
idle and during actual fill callbacks, including nested main loops, overlapping
restore requests, net-zero/latest-wins changes, later new-info/recent-dock
restoration, and
restore beginning inside an active conversion. Assertions preserve real
saved dockable identifiers, book/page order, Tool Options editor/control
identity, parent membership, relevant geometry, and final topology.

Accepted Quit before/during fill, dirty Quit cancellation, direct save requests,
factory disposal, and true finalization have explicit coverage. Real
writer/fresh-process round trips establish that saved content and final mode
survive restart. Existing host, dock, compact-options, session, strict-header and
ordinary native aggregate gates remain intact. No baseline exemptions, ignored
child stderr, skipped native dock callback, or weakened identity assertions may
substitute for a repair.

## Scope boundaries

Overlapping restore waves require duplicate-enrollment protection, including a
peer completed inside a nested loop before outer enumeration reaches it. This
does not claim arbitrary repeated startup restoration of already-consumed live
records is idempotent; that broader preexisting behavior has no production caller
in the bounded startup/recent-dock paths.

The already-open-menu/direct-global-action transaction during a transient shell
transfer remains a separate task. Sanitizers, other operating systems, tablet
input, and complete legacy UI parity are separate acceptance gates. The frozen
9ff9 host aggregate passed 155/155 registrations against its own exact source;
it does not establish aggregate acceptance for this later follow-up.

## Application-owned Welcome lifecycle boundary

The real application activation path can load the installed system session even
when a test creates an empty private user profile. A temporary read-only state
probe on the unchanged implementation established native restoration pending
and session readiness false both before and immediately after ordinary Quit.
No accepted exit had yet occurred. The previous Welcome fixture asserted
destruction inside activation immediately after that request; it must observe
accepted exit before making those same destruction, configuration finalization,
and response-count assertions. A test-only exit-after observer must also prove
exactly one accepted nonforce exit. This does not justify forcing synchronous
teardown while native dock payloads remain in use.

## Acceptance composition

The final default build passed. Fourteen normal registrations passed 271 TAP
cases plus three exact legacy-session comparisons covering 176 scenarios; the
separately corrected Welcome executable passed all eight cases.
The entire 22-case host and 33-case barrier suites also passed at scale 2. Five
previously failing owner/Quit cases passed a focused rerun. Strict classification
found no skips, crashes, or unexpected diagnostics. The compact-options suite
retains its four expected negative diagnostics.

The 16 production/prior-test source hashes were unchanged between these runs.
Only the standalone Welcome fixture changed before its final eight-case run.
This composed targeted evidence does not claim a later full aggregate pass.
The failed build, initial 28/33 sweep, broader 14/15 batch, and diagnostic probes
remain retained and summarized in the receipt. Independent review found no
remaining blocker in the final 17-file source seal.

The installed private-prefix application also passed an actual 1080×1920
portrait smoke test over authenticated loopback TCP X11: the focused host
Paintbrush shortcut worked, horizontal wheel scrolling exposed Expand Layers,
and ordinary GUI Quit returned zero. The capture was visually inspected.
