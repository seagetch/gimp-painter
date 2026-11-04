#!/usr/bin/env python3
"""Display-free orchestration checks; stubs never run Xvfb, Meson or native code."""
import hashlib,json,subprocess,tempfile,unittest
from pathlib import Path
HERE=Path(__file__).resolve().parent
DRAFT=HERE.parent
SOURCE=HERE.parents[1]
WORKFLOW=(DRAFT/'workflow/painter-native-ui.yml' if (DRAFT/'workflow/painter-native-ui.yml').is_file()
          else SOURCE/'.github/workflows/painter-native-ui.yml')
COMMON=(DRAFT/'ci-donor/tools/run-painter-ui-ci.sh' if (DRAFT/'ci-donor/tools/run-painter-ui-ci.sh').is_file()
        else SOURCE/'tools/run-painter-ui-ci.sh')
def native_route_source():
 p=DRAFT/'candidate-root/meson.build'
 if not p.is_file():p=SOURCE/'meson.build'
 source=p.read_text()
 section=source[source.index('# Make GIMP runnable'):source.index("gimp_exe = find_program('tools'/'in-build-gimp.py')")]
 start=section.index('if meson.can_run_host_binaries()')
 return section[start:]
def route_trace(runnable,gi,console=False,windows=False,osx=False,native_found=False):
 # Display-independent source branch guard, NOT a Meson/platform runtime test.
 values={'meson.can_run_host_binaries()':runnable,'have_gobject_introspection':gi,
         'enable_console_bin':console,'platform_windows':windows,'platform_osx':osx,
         'not native_gimp_exe.found()':not native_found}
 stack=[];active=True;selected=[]
 for raw in native_route_source().splitlines():
  line=raw.strip()
  if line.startswith('if '):
   condition=values[line[3:]] # Unknown changed source condition fails closed.
   stack.append({'parent':active,'seen':condition});active=active and condition
  elif line.startswith('elif '):
   condition=values[line[5:]];frame=stack[-1]
   active=frame['parent'] and not frame['seen'] and condition;frame['seen']|=condition
  elif line=='else':
   frame=stack[-1];active=frame['parent'] and not frame['seen'];frame['seen']=True
  elif line=='endif':active=stack.pop()['parent']
  elif active:selected.append(line)
 if stack:raise AssertionError('Unbalanced native route block')
 return '\n'.join(selected)
class Orchestration(unittest.TestCase):
 def test_native_without_gi_uses_own_binary_no_gir_objects(self):
  for console in (False,True):
   for windows,osx in ((False,False),(True,False),(False,True)):
    trace=route_trace(True,False,console,windows,osx)
    self.assertIn('gimp_real_exe = '+('gimpconsole_exe' if console else 'gimpmain_exe'),trace)
    self.assertIn("gimp_run_env.set('GIMP_SELF_IN_BUILD', gimp_real_exe.full_path())",trace)
    self.assertIn('gimp_exe_depends = [gimp_real_exe] + plugin_custom_targets',trace)
    for text in ('libgimp_gir','libgimpui_gir','gi.get_variable','native_gimp_exe = find_program','error('):self.assertNotIn(text,trace)
    if osx:self.assertIn("gimp_run_env.set('GIMP_TEMP_UPDATE_RPATH', gimp_real_exe.full_path())",trace)
 def test_native_with_gi_preserves_normal_and_osx_dependencies(self):
  for osx in (False,True):
   trace=route_trace(True,True,osx=osx)
   self.assertIn('gimp_exe_depends += [libgimp_gir, libgimpui_gir]',trace)
   self.assertNotIn('native_gimp_exe = find_program',trace)
   if osx:
    self.assertIn('gimp_exe_depends += [patched_libgimp_gir, patched_libgimpui_gir]',trace)
    self.assertIn("gi.get_variable('libdir')",trace)
 def test_nonrunnable_cross_keeps_native_install_requirement(self):
  for gi in (False,True):
   for found in (False,True):
    trace=route_trace(False,gi,native_found=found)
    self.assertIn('native_gimp_exe = find_program',trace)
    self.assertIn("gimp_run_env.set('GIMP_SELF_IN_BUILD', native_gimp_exe.full_path())",trace)
    self.assertNotIn('gimp_real_exe =',trace)
    self.assertEqual('error(\'When cross-compiling' in trace,not found)
 def test_strict_sanitizer_diagnostics_override_inherited_suppressions(self):
  source=(HERE/'run_brush_runtime.sh').read_text()
  block=source[source.index('  unset LSAN_OPTIONS'):source.index('\nfi\nexport GDK_BACKEND')]
  self.assertEqual(block.count('export ASAN_OPTIONS='),1)
  for forbidden in ('detect_leaks=0','halt_on_error=0','exitcode=0','suppressions='):
   self.assertNotIn(forbidden,block)
  with tempfile.TemporaryDirectory() as tmp:
   # Execute only exact settings/allowlisted-receipt shell, never Meson or GUI.
   prefix='set -euo pipefail\nout='+repr(tmp)+'\nexport ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=0 LSAN_OPTIONS=exitcode=0:suppressions=untrusted\n'
   subprocess.run(['bash','-c',prefix+block],check=True)
   report=json.loads((Path(tmp)/'sanitizer-runtime-options.json').read_text())
   options=report['ASAN_OPTIONS'].split(':')
   self.assertEqual(len(options),5)
   self.assertEqual(dict(x.split('=',1)for x in options),{'halt_on_error':'1','detect_leaks':'1','fast_unwind_on_malloc':'0','malloc_context_size':'40','log_exe_name':'1'})
   self.assertEqual(report['UBSAN_OPTIONS'],'halt_on_error=1:print_stacktrace=1')
   self.assertTrue(report['LSAN_OPTIONS_unset']);self.assertFalse(report['raw_environment_exported'])
 def test_shell_syntax(self):
  for name in ('run_brush_ci.sh','run_brush_runtime.sh','run_common_306_control.sh'):
   subprocess.run(['bash','-n',str(HERE/name)],check=True)
 def test_common_matrix_and_driver_remain_exact(self):
  workflow=WORKFLOW.read_text();self.assertIn('target: [candidate, baseline-carrier-red, dockbook-controlled-order-3d6f]',workflow)
  pins=json.loads((HERE/'pins.json').read_text())
  self.assertEqual(hashlib.sha256(COMMON.read_bytes()).hexdigest(),pins['common_protected_file_sha256']['tools/run-painter-ui-ci.sh'])
 def test_additive_modes_and_automatic_seal(self):
  w=WORKFLOW.read_text().split('  matched-preclick-diagnostic:',1)[0];self.assertIn('target: [positive, baseline-red, sanitizer]',w)
  self.assertEqual(w.count('fetch --no-tags --depth=2 origin "$GITHUB_SHA"'),2)
  self.assertEqual(w.count('--source "$PAINTER_CI_SOURCE"'),2)
  self.assertEqual(w.count('seal=$(mktemp -d "$RUNNER_TEMP/painter-brush-seal.XXXXXXXX")'),2)
  self.assertEqual(w.count("printf 'BRUSH_CI_RUNTIME_PINS=%s\\n'"),2)
  self.assertNotIn('secrets.',w);self.assertNotIn('root-approved external runtime pins required',w)
  brush=w.split('  brush-native-popup:',1)[1]
  self.assertLess(brush.index('seal_runtime_pins.py'),brush.index('bash tools/brush-ci/run_brush_ci.sh'))
 def test_diagnostic_unit_evidence_routes_remain_separate(self):
  workflow=WORKFLOW.read_text()
  for name,log in (('test_capture_apt_failure.py','apt-diagnostic-unit-tests.log'),('test_capture_protocol_stderr.py','protocol-diagnostic-unit-tests.log')):
   self.assertEqual(workflow.count('python3 -B tools/brush-ci/'+name+' 2>&1 | tee "$GITHUB_WORKSPACE/ci-results/'+log+'"'),2)
 def test_targets_preserve_closure(self):
  t=json.loads((HERE/'build-targets.json').read_text())
  self.assertEqual(len(t['current_common_candidate_targets']),24)
  self.assertEqual(t['candidate_union'],t['current_common_candidate_targets']+['painter-compact-options','painter-toolbar-window']+['libgimp/tests/plug-in-protocol-fixture:executable','libgimp/tests/test-plug-in-protocol:executable','libgimpbase/test-choice-lifecycle:executable','app/tests/painter-brush-geometry:executable','libgimpconfig/color-config-ownership:executable','app/tests/color-ownership/display-renderer-ownership:executable','app/tests/color-ownership/canvas-ownership:executable','libgimpwidgets/test-frame-style-idempotence:executable','app/tests/color-ownership/canvas-mask-fixed-provider:executable'])
  self.assertEqual(len(set(t['candidate_union'])),35)
  self.assertEqual(t['brush_baseline_targets'],['painter-compact-options','menus/image-menu.ui:custom','menus/dockable-menu.ui:custom'])
  d=(HERE/'run_brush_ci.sh').read_text();self.assertIn('test "${#targets[@]}" = "$expected_targets"',d)
 def test_locale_helper_is_exact_common_implementation(self):
  common=COMMON.read_text();start=common.index('locale_state() {');end=common.index('\nif [[ "$mode" == candidate ]]; then',start)
  self.assertIn(common[start:end],(HERE/'run_brush_ci.sh').read_text())
 def test_baseline_and_sanitizer_are_not_relabelled(self):
  d=(HERE/'run_brush_ci.sh').read_text()
  self.assertIn('--pins "$pins" --output "$out/baseline-overlay.json"',d)
  self.assertIn('--baseline "$src" --verify "$out/baseline-overlay.json"',d)
  self.assertIn('sanitize+=(-Db_sanitize=address,undefined)',d)
  self.assertIn('sanitizer-compile-commands.json',d)
  self.assertIn('sanitizer-build-commands.txt',d)
  self.assertNotIn('cp "$repo/app/tools/',d)
  self.assertIn('stage=source-integrity\nverify_source || failed=1',d)
  self.assertIn('locale_state verify || failed=1',d)
 def test_runtime_requires_seal_not_committed_null_pins(self):
  d=(HERE/'run_brush_runtime.sh').read_text()
  self.assertIn('${BRUSH_CI_RUNTIME_PINS:?automatic runtime seal required}',d)
  self.assertIn('pins=json.loads(pathlib.Path(runtime_pins).read_text())',d)
  self.assertNotIn("(pathlib.Path(draft)/'pins.json').read_text()",d)
  pins=json.loads((HERE/'pins.json').read_text());self.assertIsNone(pins['candidate_final_commit'])
 def exercise_group(self,checker_status=0):
  temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup);root=Path(temporary.name)
  raw=(HERE/'run_brush_runtime.sh').read_text()
  sanitation=raw[raw.index('unset GIMP_TEST_PROTOCOL_FIXTURE'):raw.index('run_group() {')]
  function=raw[raw.index('run_group() {'):raw.index('\nfailed=0\n')]
  script='''set -euo pipefail
run="$1/private";out="$1/out";build="$1/build";draft="$1/bundle";src="$1/source";mode=positive
mkdir -p "$run" "$out"
export NO_AT_BRIDGE=stale GIMP_TEST_LAYER_UI_FILTER=stale GIMP_TEST_OVERLAY_PAINT_GATE=stale
export GIMP_TEST_TOOLBAR_SESSION_DIRECTORY=stale GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES=stale
xvfb-run() { printf '%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\\n' "$HOME" "$GDK_SCALE" "${NO_AT_BRIDGE:-UNSET}" "${GIMP_TEST_LAYER_UI_FILTER:-UNSET}" "${GIMP_TEST_OVERLAY_PAINT_GATE:-UNSET}" "${GIMP_TEST_TOOLBAR_SESSION_DIRECTORY:-UNSET}" "${GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES:-UNSET}" "${GIMP_BRUSH_POPUP_CAPTURE:-UNSET}" "${GIMP_BRUSH_POPUP_BOTTOM_CAPTURE:-UNSET}" "$*" >> "$run/calls"; }
python3() { if [[ "$*" == *check_brush_ci.py* ]]; then return CHECKER; fi; return 0; }
'''.replace('CHECKER',str(checker_status))+sanitation+function+'''
failed=0
run_group first 1 /brush-thumbnail-native-popup/actual-entry fake "$out/first.png" || failed=1
run_group second 2 /brush-thumbnail-native-popup/lower-controls-actual-input fake "$out/second.png" || failed=1
exit "$failed"
'''
  r=subprocess.run(['bash','-c',script,'control',str(root)],capture_output=True,text=True)
  return r,(root/'private/calls').read_text().splitlines()
 def test_real_shell_sanitizes_and_isolates_capture_invocations(self):
  r,calls=self.exercise_group();self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(len(calls),2)
  rows=[x.split('|') for x in calls];self.assertNotEqual(rows[0][0],rows[1][0]);self.assertEqual([r[1] for r in rows],['1','2'])
  for row in rows:
   self.assertEqual(row[2:7],['UNSET']*5);self.assertIn('-screen 0 1080x1920x24 -nolisten tcp',row[9])
  self.assertTrue(rows[0][7].endswith('/first.png'));self.assertEqual(rows[0][8],'UNSET')
  self.assertEqual(rows[1][7],'UNSET');self.assertTrue(rows[1][8].endswith('/second.png'))
 def test_checker_failure_cannot_become_success(self):
  r,calls=self.exercise_group(1);self.assertNotEqual(r.returncode,0);self.assertEqual(len(calls),2)
 def protocol_run(self,mode='positive',meson_status=0,checker_status=0,capture_status=0,choice_status=0,geometry_status=0,color_status=0,canvas_status=0,frame_status=0):
  temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup);root=Path(temporary.name)
  raw=(HERE/'run_brush_runtime.sh').read_text()
  sanitation=raw[raw.index('unset GIMP_TEST_PROTOCOL_FIXTURE'):raw.index('run_group() {')]
  function=raw[raw.index('run_group() {'):raw.index('\nfailed=0\n')]
  dispatch=raw[raw.index('failed=0\n'):raw.index('\nif [[ "$mode" == baseline-red ]]; then\n  run_group brush-baseline-red')]
  script='''set -euo pipefail
run="$1/private";out="$1/out";build="$1/build";draft="$1/bundle";src="$1/source";mode="$2";RUNNER_TEMP="$1"
mkdir -p "$run" "$out"
export DISPLAY=wrong WAYLAND_DISPLAY=wrong GIMP_TEST_PROTOCOL_FIXTURE=wrong GIMP_TEST_PROTOCOL_SCENARIO=wrong GIMP_TEST_PROTOCOL_LOG_DIR=wrong GIMP_TEST_EXPECT_REDUNDANT_QUIT=1
xvfb-run() { printf 'XVFB|%s\\n' "$*" >> "$run/calls"; if [[ "$*" == *gimp-frame-style-idempotence* ]]; then return FRAME; fi; return CANVAS; }
meson() { printf '%s|%s|%s|%s|%s|%s|%s\\n' "${DISPLAY:-UNSET}" "${WAYLAND_DISPLAY:-UNSET}" "${GIMP_TEST_PROTOCOL_FIXTURE:-UNSET}" "${GIMP_TEST_PROTOCOL_SCENARIO:-UNSET}" "${GIMP_TEST_PROTOCOL_LOG_DIR:-UNSET}" "${GIMP_TEST_EXPECT_REDUNDANT_QUIT:-UNSET}" "$*" >> "$run/calls"; if [[ "$*" == *color-config-ownership* || "$*" == *display-renderer-ownership* ]]; then return COLOR; fi; if [[ "$*" == *painter-brush-geometry* ]]; then return GEOMETRY; fi; if [[ "$*" == *choice-lifecycle* ]]; then return CHOICE; fi; return MESON; }
python3() { if [[ "$*" == *capture_protocol_stderr.py* ]]; then printf 'capture:%s\\n' "$*" >> "$run/checks"; return CAPTURE; fi; if [[ "$*" == *check_brush_ci.py* ]]; then printf '%s\\n' "$*" >> "$run/checks"; return CHECKER; fi; return 0; }
'''.replace('MESON',str(meson_status)).replace('CHECKER',str(checker_status)).replace('CAPTURE',str(capture_status)).replace('CHOICE',str(choice_status)).replace('GEOMETRY',str(geometry_status)).replace('COLOR',str(color_status)).replace('CANVAS',str(canvas_status)).replace('FRAME',str(frame_status))+sanitation+function+'\n'+dispatch+'\nexit "$failed"\n'
  result=subprocess.run(['bash','-c',script,'control',str(root),mode],capture_output=True,text=True)
  return result,((root/'private/calls').read_text() if (root/'private/calls').exists() else ''),((root/'private/checks').read_text() if (root/'private/checks').exists() else '')
 def test_ownership_failure_remains_failure_and_all_seven_run(self):
  for mode in ('positive','sanitizer'):
   result,calls,checks=self.protocol_run(mode,color_status=1)
   self.assertNotEqual(result.returncode,0);self.assertEqual(len(calls.splitlines()),7)
   self.assertIn('/color-config-cases.json',checks);self.assertIn('/display-renderer-cases.json',checks)
 def test_display_renderer_runs_whole_binary_not_shallow_parent_filter(self):
  # The only two cases are nested below /ownership/display and /ownership/renderer.
  # GLib -p /ownership selects zero leaves; exact-case validation must stay strict.
  cases=json.loads((HERE/'display-renderer-cases.json').read_text())['case_ids']
  self.assertEqual(cases,['/ownership/display/parity','/ownership/renderer/public-smoke'])
  for mode in ('positive','sanitizer'):
   result,calls,checks=self.protocol_run(mode)
   self.assertEqual(result.returncode,0,result.stderr)
   command=calls.splitlines()[4].split('|')[6]
   self.assertIn('--logbase display-renderer-'+mode,command)
   self.assertTrue(command.endswith(' display-renderer-ownership'))
   self.assertNotIn('--test-args',command)
   self.assertIn('/display-renderer-cases.json',checks)
 def test_canvas_requires_portrait_display_whole_binary_and_strict_manifest(self):
  self.assertEqual(json.loads((HERE/'canvas-cases.json').read_text())['case_ids'], ['/ownership/canvas/unrealized-padding','/ownership/canvas/realized-padding'])
  for mode in ('positive','sanitizer'):
   result,calls,checks=self.protocol_run(mode)
   self.assertEqual(result.returncode,0,result.stderr)
   canvas=calls.splitlines()[5]
   self.assertTrue(canvas.startswith('XVFB|'))
   self.assertIn('-screen 0 1080x1920x24 -nolisten tcp',canvas)
   self.assertIn('dbus-run-session -- meson test',canvas)
   self.assertTrue(canvas.endswith(' canvas-ownership'))
   self.assertNotIn('--test-args',canvas)
   self.assertIn('/canvas-cases.json',checks)
 def test_canvas_failed_exit_or_gate_cannot_be_waived(self):
  for mode in ('positive','sanitizer'):
   for native,gate in ((1,0),(0,1),(77,0)):
    result,calls,checks=self.protocol_run(mode,canvas_status=native,checker_status=gate)
    self.assertNotEqual(result.returncode,0)
    self.assertEqual(len(calls.splitlines()),7)
    self.assertIn('/canvas-cases.json',checks)
 def test_frame_requires_portrait_whole_binary_exact_seven(self):
  manifest=json.loads((HERE/'frame-style-cases.json').read_text())
  self.assertEqual(len(manifest['case_ids']),7)
  self.assertEqual(len(set(manifest['case_ids'])),7)
  for mode in ('positive','sanitizer'):
   result,calls,checks=self.protocol_run(mode)
   self.assertEqual(result.returncode,0,result.stderr)
   frame=calls.splitlines()[6]
   self.assertTrue(frame.startswith('XVFB|'))
   self.assertIn('-screen 0 1080x1920x24 -nolisten tcp',frame)
   self.assertTrue(frame.endswith(' gimp-frame-style-idempotence'))
   self.assertNotIn('--test-args',frame)
   self.assertIn('/frame-style-cases.json',checks)
 def test_frame_failed_exit_skip_or_checker_failure_remains_failure(self):
  for mode in ('positive','sanitizer'):
   for native,gate in ((1,0),(77,0),(0,1)):
    result,calls,checks=self.protocol_run(mode,frame_status=native,checker_status=gate)
    self.assertNotEqual(result.returncode,0)
    self.assertEqual(len(calls.splitlines()),7)
    self.assertIn('/frame-style-cases.json',checks)
 def test_frame_old_control_follows_actual_candidate_log_and_retains_failure(self):
  raw=(HERE/'run_brush_ci.sh').read_text()
  begin=raw.index('stage=brush-runtime\n');end=raw.index('stage=source-integrity\n',begin)
  block=raw[begin:end]
  self.assertLess(block.index('run_brush_runtime.sh'),block.index('run_frame_style_red.sh'))
  self.assertIn('$build/meson-logs/frame-style-positive.json',block)
  for mode in ('positive','sanitizer','baseline-red'):
   for previous in (0,1):
    script='set -euo pipefail\nhere=ci;src=/private/source;build=/private/build;run=/private;out=/artifacts;mode='+mode+';failed='+str(previous)+'\nbash() { printf "%s\\n" "$*"; return 0; }\n'+block+'exit "$failed"\n'
    result=subprocess.run(['bash','-c',script],capture_output=True,text=True)
    self.assertEqual(result.returncode,previous,result.stderr)
    self.assertEqual('run_frame_style_red.sh' in result.stdout,mode=='positive')
    if mode=='positive':self.assertIn('/private/build/meson-logs/frame-style-positive.json',result.stdout)
 def test_frame_old_failure_cannot_be_hidden_by_candidate_success(self):
  raw=(HERE/'run_brush_ci.sh').read_text()
  begin=raw.index('stage=brush-runtime\n');end=raw.index('stage=source-integrity\n',begin)
  for failed_command in ('run_brush_runtime.sh','run_frame_style_red.sh'):
   script='set -euo pipefail\nhere=ci;src=/source;build=/build;run=/private;out=/out;mode=positive;failed=0\nbash() { printf "%s\\n" "$*"; [[ "$1" != "ci/'+failed_command+'" ]]; }\n'+raw[begin:end]+'exit "$failed"\n'
   result=subprocess.run(['bash','-c',script],capture_output=True,text=True)
   self.assertEqual(result.returncode,1,result.stderr)
   self.assertIn('run_frame_style_red.sh',result.stdout)
 def test_frame_target_namespace_and_old_only_control(self):
  targets=json.loads((HERE/'build-targets.json').read_text())
  self.assertEqual(targets['supplementary_frame_style_case_count'],7)
  self.assertEqual(targets['candidate_union_count'],35)
  self.assertEqual(len(targets['normal_negative_control_targets']),3)
  text=(SOURCE/'libgimpwidgets/meson.build').read_text()
  before,old=text.split("frame_style_old_provider = executable",1)
  self.assertNotIn('-Dgimp_frame_new=',before)
  self.assertTrue(before.rstrip().endswith("if host_machine.system() == 'linux'"))
  self.assertTrue(old.rstrip().endswith('endif'))
  self.assertIn("['test-frame-style-idempotence.c', 'fixtures/gimpframe-c14.c']",old)
  self.assertIn('-Dgimp_frame_new=gimp_frame_c14_new',old)
  self.assertIn('-Dgimp_frame_get_type=gimp_frame_c14_get_type',old)
  self.assertIn('should_fail: true',old)
  self.assertIn('-Wl,--cref,-Map,',old)
 def test_canvas_target_and_normal_old_control_scope(self):
  targets=json.loads((HERE/'build-targets.json').read_text())
  self.assertEqual(targets['supplementary_canvas_case_count'],2)
  self.assertEqual(targets['normal_negative_control_targets'],['app/tests/color-ownership/canvas-ownership-old-provider:executable','libgimpwidgets/frame-style-old-provider:executable','app/tests/color-ownership/canvas-mask-old-provider:executable'])
  text=(SOURCE/'app/tests/color-ownership/meson.build').read_text()
  self.assertIn("['test-canvas.c', 'fixtures/gimpcanvas-c9.c']",text)
  self.assertIn('-Wl,--cref,-Map,',text)
  self.assertIn("suite: 'painter-negative-control'",text)
 def test_protocol_dispatch_native_and_sanitizer_no_display(self):
  for mode in ('positive','sanitizer'):
   result,calls,checks=self.protocol_run(mode)
   self.assertEqual(result.returncode,0,result.stderr)
   rows=calls.strip().splitlines();self.assertEqual(len(rows),7);fields=rows[0].split('|');self.assertEqual(fields[:4],['UNSET']*4);self.assertTrue(fields[4].endswith('/protocol-'+mode+'-child-logs'));self.assertEqual(fields[5],'UNSET')
   self.assertIn('--no-rebuild --num-processes 1',fields[6]);self.assertIn('--test-args -p /plug-in-protocol plug-in-protocol',fields[6])
   self.assertIn('--logbase protocol-'+mode,fields[6]);self.assertNotIn('--timeout-multiplier',fields[6])
   self.assertIn('/protocol-cases.json',checks);self.assertNotIn('--screenshot',checks)
   self.assertIn('capture:',checks);self.assertIn('--runner-temp',checks);self.assertIn('--mode '+mode,checks)
   choice=rows[1].split('|');self.assertEqual(choice[:6],['UNSET']*6);self.assertIn('--logbase choice-'+mode,choice[6]);self.assertIn('--test-args -p /choice-lifecycle choice-lifecycle',choice[6]);self.assertNotIn('--timeout-multiplier',choice[6]);self.assertIn('/choice-cases.json',checks);self.assertEqual(checks.count('capture:'),1)
   geometry=rows[2].split('|');self.assertEqual(geometry[:6],['UNSET']*6);self.assertIn('--logbase geometry-'+mode,geometry[6]);self.assertIn('--test-args -p /painter/geometry painter-brush-geometry',geometry[6]);self.assertNotIn('--timeout-multiplier',geometry[6]);self.assertIn('/geometry-cases.json',checks)
   for index,registration in [(3,'color-config-ownership'),(4,'display-renderer-ownership')]:
    owned=rows[index].split('|');self.assertEqual(owned[:6],['UNSET']*6);self.assertIn(registration,owned[6]);self.assertNotIn('--timeout-multiplier',owned[6])

 def test_protocol_not_imported_into_baseline(self):
  result,calls,checks=self.protocol_run('baseline-red')
  self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(calls,'');self.assertEqual(checks,'')
 def test_protocol_native_or_checker_failure_propagates(self):
  for native,checker in ((1,0),(0,1),(1,1)):
   result,calls,checks=self.protocol_run(meson_status=native,checker_status=checker)
   self.assertNotEqual(result.returncode,0);self.assertTrue(calls);self.assertTrue(checks);self.assertIn('capture:',checks)
 def test_protocol_capture_refusal_cannot_waive_gate(self):
  result,calls,checks=self.protocol_run(capture_status=2)
  self.assertNotEqual(result.returncode,0);self.assertTrue(calls);self.assertIn('capture:',checks);self.assertIn('/protocol-cases.json',checks)
 def test_choice_only_native_failure_cannot_be_hidden_by_protocol_pass(self):
  for mode in ('positive','sanitizer'):
   result,calls,checks=self.protocol_run(mode=mode,choice_status=1)
   self.assertNotEqual(result.returncode,0);self.assertEqual(len(calls.splitlines()),7);self.assertIn('/choice-cases.json',checks)
 def test_geometry_only_native_failure_cannot_be_hidden_by_other_passes(self):
  for mode in ('positive','sanitizer'):
   result,calls,checks=self.protocol_run(mode=mode,geometry_status=1)
   self.assertNotEqual(result.returncode,0);self.assertEqual(len(calls.splitlines()),7);self.assertIn('/geometry-cases.json',checks)

class SanitizerPartition(unittest.TestCase):
 def setUp(self):
  import sanitizer_cases,shutil
  self.partition=sanitizer_cases;self.gate=sanitizer_cases.gate
  self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
  self.bundle=self.root/'bundle';self.bundle.mkdir()
  shutil.copyfile(HERE/'brush-cases.json',self.bundle/'brush-cases.json')
  shutil.copytree(HERE/'sanitizer-case-manifests',self.bundle/'sanitizer-case-manifests')
  self.logs=self.root/'logs';self.logs.mkdir();self.reports=self.root/'reports';self.reports.mkdir()
  self.common=COMMON.with_name('check_painter_ui_ci.py')
 def write(self,path,value):path.write_text(json.dumps(value)+'\n')
 def make_case(self,case):
  return {'name':'gimp:app / painter-compact-options','result':'OK','returncode':0,'duration':52.0,'stdout':'TAP version 14\nok 1 '+case['case_id']+'\n1..1\n','stderr':'','command':['/owned/build/app/tests/painter-compact-options','--tap','-p',case['case_id']]}
 def report(self,row,case_id):
  path=self.logs/'temporary.json';self.write(path,row)
  report=self.gate.load_common(self.common).read_results(path,['painter-compact-options'],case_ids=[case_id])
  return self.gate.reject_sanitizer_diagnostics(report)
 def complete(self):
  for case in self.partition.cases(self.bundle):
   row=self.make_case(case);self.write(self.logs/(case['label']+'.json'),row)
   self.write(self.reports/(case['label']+'-results.json'),self.report(row,case['case_id']))
 def aggregate(self):return self.partition.aggregate(self.bundle,self.logs,self.reports,self.common)
 def change_raw(self,update):
  path=self.logs/'brush-sanitizer-case01.json';row=json.loads(path.read_text());update(row);self.write(path,row)
 def test_exact21_unique_ordered_single_case_manifests(self):
  rows=self.partition.cases(self.bundle)
  self.assertEqual(len(rows),21)
  self.assertEqual([x['case_id']for x in rows],json.loads((HERE/'brush-cases.json').read_text())['case_ids'])
  self.assertEqual(len({x['label']for x in rows}),21)
 def test_manifest_missing_duplicate_reordered_or_extra_fails_closed(self):
  path=self.bundle/'sanitizer-case-manifests/01.json';original=json.loads(path.read_text())
  second=json.loads(path.with_name('02.json').read_text())
  for value in (dict(original,case_ids=[]),dict(original,case_ids=original['case_ids']*2),second):
   self.write(path,value)
   with self.assertRaises(ValueError):self.partition.cases(self.bundle)
  self.write(path,original);self.write(path.with_name('22.json'),original)
  with self.assertRaises(ValueError):self.partition.cases(self.bundle)
 def test_aggregate_exact21_with_individual_and_total_durations(self):
  self.complete();report=self.aggregate()
  self.assertTrue(report['passed'],report['errors']);self.assertEqual(len(report['executed_case_ids']),21)
  self.assertEqual(len(report['tests']),21);self.assertEqual(len(report['cases']),21)
  self.assertEqual([row['duration_seconds']for row in report['cases']],[52.0]*21)
  self.assertEqual(report['total_observed_duration_seconds'],1092.0)
  self.assertIn('not a single full21 process',report['scope'])
 def test_missing_result_or_log_fails_and_keeps_later_evidence(self):
  for folder,suffix in ((self.logs,'.json'),(self.reports,'-results.json')):
   self.complete();(folder/('brush-sanitizer-case01'+suffix)).unlink()
   report=self.aggregate();self.assertFalse(report['passed']);self.assertEqual(len(report['cases']),21);self.assertTrue(report['cases'][-1]['passed'])
 def test_unexpected_extra_invocation_evidence_cannot_pass_union(self):
  self.complete();self.write(self.logs/'brush-sanitizer-case22.json',{})
  self.assertFalse(self.aggregate()['passed'])
 def test_duplicate_across_invocations_cannot_pass_union(self):
  self.complete();case=self.partition.cases(self.bundle)[1]
  self.change_raw(lambda row:row.update(self.make_case(case)))
  self.assertFalse(self.aggregate()['passed'])
 def test_parent_fail_timeout_or_nonzero_even_with_tap_pass_fails(self):
  for result,status in (('FAIL',1),('TIMEOUT',-15),('OK',1),('SKIP',0)):
   self.complete();self.change_raw(lambda row:row.update(result=result,returncode=status))
   self.assertFalse(self.aggregate()['passed'])
 def test_raw_child_sanitizer_diagnostic_rejected_even_with_forged_saved_pass(self):
  for text in ('ERROR: LeakSanitizer: detected memory leaks','SUMMARY: AddressSanitizer: heap-use-after-free','unit.c:4:2: runtime error: overflow'):
   self.complete();self.change_raw(lambda row:row.update(stderr=text))
   self.assertFalse(self.aggregate()['passed'])
 def test_wrong_prefix_selector_extra_selector_or_skip_controls_fail(self):
  for tail in (['-s','/brush-thumbnail-native-popup'],['-x','/brush-thumbnail-native-popup'],['-r','/brush-thumbnail-native-popup'],['-p','/brush-thumbnail-native-popup'],['--GTestSkipCount=1'],['-m','quick']):
   self.complete();self.change_raw(lambda row:row.update(command=row['command']+tail));self.assertFalse(self.aggregate()['passed'])
  self.complete();self.change_raw(lambda row:row['command'].__setitem__(-1,'/brush-thumbnail-native-popup'))
  self.assertFalse(self.aggregate()['passed'])
 def test_skip_wrong_plan_extra_or_malformed_tap_results_fail(self):
  for transform in (lambda s:s.replace('ok 1 ','not ok 1 '),lambda s:s.replace('1..1','1..2'),lambda s:s+'ok 2 /extra # ordinary comment\n',lambda s:s+'ok 2 malformed\n',lambda s:s.replace('\n1..1',' # SKIP reason\n1..1')):
   self.complete();self.change_raw(lambda row:row.update(stdout=transform(row['stdout'])));self.assertFalse(self.aggregate()['passed'])
 def test_bad_duration_cannot_be_silently_omitted(self):
  for value in (None,-1,float('nan'),'52',True):
   self.complete();self.change_raw(lambda row:row.update(duration=value));self.assertFalse(self.aggregate()['passed'])
 def test_failed_saved_gate_cannot_become_success(self):
  self.complete();path=self.reports/'brush-sanitizer-case02-results.json';row=json.loads(path.read_text());row['passed']=False;row['errors']=['failure'];self.write(path,row)
  self.assertFalse(self.aggregate()['passed'])
 def test_plan_cli_refuses_missing_manifest_without_partial_plan(self):
  (self.bundle/'sanitizer-case-manifests/21.json').unlink()
  result=subprocess.run(['python3','-B',str(HERE/'sanitizer_cases.py'),'--bundle',str(self.bundle),'--plan'],capture_output=True,text=True)
  self.assertNotEqual(result.returncode,0);self.assertNotIn('\t',result.stdout)
 def test_real_shell_runs_all21_after_first_failure_fresh_profiles_same_cap(self):
  raw=(HERE/'run_brush_runtime.sh').read_text()
  function=raw[raw.index('run_group() {'):raw.index('\nfailed=0\n')]
  start=raw.index('elif [[ "$mode" == sanitizer ]]; then\n',raw.index('failed=0\n'))
  dispatch=raw[start+len('elif [[ "$mode" == sanitizer ]]; then\n'):raw.index('\nelse\n  for scale in 1 2;',start)]
  script='''set -euo pipefail
run="$1/private";out="$1/out";build="$1/build";draft="$2";src="$1/source";mode=sanitizer
mkdir -p "$run" "$out"
xvfb-run() { printf '%s|%s\\n' "$HOME" "$*" >> "$run/calls"; if [[ "$*" == *case01* ]]; then return 1; fi; }
python3() { if [[ "$*" == *sanitizer_cases.py* ]]; then if [[ "$*" == *--plan* ]]; then command python3 "$@"; else echo aggregate >> "$run/aggregate"; return 0; fi; fi; return 0; }
'''+function+'\nfailed=0\n'+dispatch+'\nexit "$failed"\n'
  result=subprocess.run(['bash','-c',script,'control',str(self.root),str(HERE)],capture_output=True,text=True)
  self.assertNotEqual(result.returncode,0,result.stderr)
  calls=(self.root/'private/calls').read_text().splitlines();self.assertEqual(len(calls),21)
  self.assertEqual(len({line.split('|')[0]for line in calls}),21)
  for line in calls:
   self.assertIn('--timeout-multiplier 3',line);self.assertEqual(line.count(' -p '),1)
   self.assertIn('-screen 0 1080x1920x24 -nolisten tcp',line)
  self.assertEqual((self.root/'private/aggregate').read_text().strip(),'aggregate')
 def test_normal_full21_baseline_and_all_timeouts_unchanged(self):
  raw=(HERE/'run_brush_runtime.sh').read_text()
  self.assertIn('run_group "brush-scale$scale" "$scale" /brush-thumbnail-native-popup "$draft/brush-cases.json" || failed=1',raw)
  self.assertIn("run_group brush-baseline-red 1 /brush-thumbnail-native-popup/actual-entry '' || failed=1",raw)
  self.assertEqual(raw.count('--timeout-multiplier 3'),1)
  self.assertEqual(WORKFLOW.read_text().count('timeout-minutes: 90'),2)


class WireRedOrchestration(unittest.TestCase):
 def test_old_control_is_sanitizer_only_and_failure_cannot_hide_candidate_runs(self):
  raw=(HERE/'run_brush_ci.sh').read_text()
  begin=raw.index('failed=0\n');end=raw.index('if [[ "$mode" != baseline-red ]]; then\n',begin)
  block=raw[begin:end]
  self.assertIn('stage=wire-lsan-old-provider-red',block)
  self.assertIn('bash "$here/run_brush_runtime.sh" "$src" "$build" "$out" "$run/runtime" "$mode" || failed=1',raw[end:])
  for mode in ('positive','baseline-red','sanitizer'):
   script='set -euo pipefail\nhere=ci;repo=source;src=detached-source;build=build;run=private;out=out;mode='+mode+'\nbash() { echo old-control:$1; return 1; }\n'+block+'echo candidate-runtime-still-required\nexit "$failed"\n'
   result=subprocess.run(['bash','-c',script],capture_output=True,text=True)
   self.assertIn('candidate-runtime-still-required',result.stdout)
   self.assertEqual(result.returncode,1 if mode in ('positive','sanitizer') else 0)
   self.assertEqual('old-control:ci/run_wire_lsan_red.sh'in result.stdout,mode=='sanitizer')
   self.assertEqual('old-control:ci/run_canvas_ownership_red.sh'in result.stdout,mode=='positive')
 def test_old_control_fetches_immutable_c5_and_builds_actual_meson_provider(self):
  raw=(HERE/'run_wire_lsan_red.sh').read_text()
  self.assertIn('old=da070df3717723e75f0757113c2da2768949d676',raw)
  self.assertIn('fetch --no-tags --depth=1 origin "$old"',raw)
  self.assertIn('worktree add --detach "$source" "$old"',raw)
  self.assertIn('544a91ca352614a58f024d1e3cd813ae0226df31',raw)
  self.assertIn('meson setup "$build" "$source"',raw)
  self.assertIn('-Db_sanitize=address,undefined',raw)
  self.assertIn('meson compile -C "$build" -j 2 libgimpbase/test-choice-lifecycle:executable',raw)
  self.assertIn("--test-args '-p /choice-lifecycle/wire-roundtrip' choice-lifecycle",raw)
  self.assertNotIn('--timeout-multiplier',raw);self.assertNotIn('xvfb-run',raw);self.assertNotIn('dbus-run-session',raw)
  self.assertIn("'Provider/test source not actually instrumented'",raw)
  self.assertIn("'Not the actual old-provider library'",raw)
  self.assertIn('post-run-source-integrity.log',raw)
  subprocess.run(['bash','-n',str(HERE/'run_wire_lsan_red.sh')],check=True)
 def test_old_control_runtime_options_exactly_match_strict_candidate_flags(self):
  raw=(HERE/'run_wire_lsan_red.sh').read_text()
  candidate=(HERE/'run_brush_runtime.sh').read_text()
  expected='export ASAN_OPTIONS=halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0:malloc_context_size=40:log_exe_name=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1'
  self.assertIn(expected,raw);self.assertIn(expected,candidate)
  self.assertIn('unset DISPLAY WAYLAND_DISPLAY LSAN_OPTIONS',raw)
  for forbidden in ('detect_leaks=0','halt_on_error=0','exitcode=0','suppressions='):
   self.assertNotIn(forbidden,raw)
  self.assertIn('(( status != 0 && gate == 0 ))',raw)
 def test_old_control_rejects_unowned_or_nonprivate_run_path_before_source_work(self):
  raw=(HERE/'run_wire_lsan_red.sh').read_text()
  code=raw.split("<<'PYOWNED'\n",1)[1].split('\nPYOWNED',1)[0]
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);run=root/'painter-brush-ci.ABCDEFGH';run.mkdir(mode=0o700)
   def check(path):return subprocess.run(['python3','-B','-c',code,str(path),str(root)],capture_output=True,text=True).returncode
   self.assertEqual(check(run),0)
   run.chmod(0o777);self.assertNotEqual(check(run),0)
   wrong=root/'wrong-prefix';wrong.mkdir(mode=0o700);self.assertNotEqual(check(wrong),0)

class CanvasRedOrchestration(unittest.TestCase):
 def test_canvas_provider_source_is_exact_meson_worktree_not_original_checkout(self):
  raw=(HERE/'run_brush_ci.sh').read_text()
  self.assertIn('src="$run/source"; build="$run/build"',raw)
  self.assertIn('meson setup "$build" "$src"',raw)
  begin=raw.index('failed=0\n');end=raw.index('if [[ "$mode" != baseline-red ]]; then\n',begin)
  script='set -euo pipefail\nhere=ci;repo=/original-checkout;src=/private/source;build=/private/build;run=/private;out=/artifacts;mode=positive\nbash() { printf "%s\\n" "$*"; }\n'+raw[begin:end]
  result=subprocess.run(['bash','-c',script],capture_output=True,text=True)
  self.assertEqual(result.returncode,0,result.stderr)
  self.assertEqual(result.stdout.splitlines(),['ci/run_canvas_ownership_red.sh /private/source /private/build /private /artifacts'])

 def run_control(self,provider=0,meson=0,checker=0):
  temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup);root=Path(temporary.name)
  raw=(HERE/'run_canvas_ownership_red.sh').read_text();body=raw[raw.index('evidence="$out/canvas-old-provider-red"'):]
  pre='''set -euo pipefail
here=/bundle;src=/source;build=/build;run="$1/run";out="$1/out"
mkdir "$run" "$out"
python3() { printf '%s\\n' "$*" >> "$run/checks"; if [[ "$*" == *'--provider '* ]]; then return PROVIDER; fi; if [[ "$*" == *'--log '* || "$*" == *'--aggregate '* ]]; then return CHECKER; fi; return 0; }
xvfb-run() { printf '%s|%s\\n' "$HOME" "$*" >> "$run/calls"; return MESON; }
'''.replace('PROVIDER',str(provider)).replace('CHECKER',str(checker)).replace('MESON',str(meson))
  result=subprocess.run(['bash','-c',pre+body,'stub',str(root)],text=True,capture_output=True)
  calls=(root/'run/calls').read_text().splitlines() if (root/'run/calls').exists() else []
  return result,calls,(root/'run/checks').read_text()
 def test_exact_two_fresh_display_old_leaves(self):
  result,calls,checks=self.run_control();self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(len(calls),2)
  self.assertNotEqual(calls[0].split('|')[0],calls[1].split('|')[0])
  for call,leaf in zip(calls,['unrealized-padding','realized-padding']):
   self.assertIn('-screen 0 1080x1920x24 -nolisten tcp',call)
   self.assertIn('--test-args -p /ownership/canvas/'+leaf+' canvas-ownership-old-provider',call)
   self.assertNotIn('--timeout-multiplier',call)
  self.assertIn('--aggregate',checks);self.assertIn('--provider-receipt',checks)
 def test_provider_refusal_prevents_old_runtime(self):
  result,calls,checks=self.run_control(provider=1);self.assertNotEqual(result.returncode,0);self.assertFalse(calls);self.assertNotIn('--aggregate',checks)
 def test_wrong_wrapper_or_strict_gate_failure_retained_both_run(self):
  for meson,checker in [(1,0),(0,1),(77,0)]:
   result,calls,checks=self.run_control(meson=meson,checker=checker);self.assertNotEqual(result.returncode,0);self.assertEqual(len(calls),2);self.assertIn('--aggregate',checks)
 def test_only_old_target_is_expected_failure(self):
  meson=(SOURCE/'app/tests/color-ownership/meson.build').read_text()
  self.assertEqual(meson.count('should_fail: true'),2)
  self.assertIn("suite: 'painter-negative-control', is_parallel: false, timeout: 120,\n  should_fail: true",meson)

if __name__=='__main__':unittest.main(verbosity=2)
