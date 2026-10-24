from pathlib import Path
import os,sys,json,subprocess,resource,time,hashlib
mode=sys.argv[1];O=Path('/tmp/colorify-editor-private')/mode;B=Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source';env=dict(os.environ);env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(S),GIMP_TESTING_ABS_TOP_BUILDDIR=str(O/'runtime'),GIMP_TESTING_PLUGINDIRS=str(B/'plug-ins/common'),UI_TEST='yes',GIMP_TEST_COLORIFY_ONCANVAS='1',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');(O/'runtime/app/tests/gimpdir-output').mkdir(parents=True,exist_ok=True)
menus=O/'runtime/menus'
if not menus.exists():menus.symlink_to(B/'menus',target_is_directory=True)
private=json.loads((O.parent/'private-libraries.json').read_text()) if (O.parent/'private-libraries.json').exists() else {}
if private:env['LD_LIBRARY_PATH']=str(O.parent/'runtime-libraries')+':'+env.get('LD_LIBRARY_PATH','')
cases=['colorify_editor_create_pixels']if mode=='red'else['colorify_editor_create_pixels','colorify_editor_noop_and_edit','colorify_editor_unknown_keep','colorify_editor_invalid_and_stale','colorify_editor_closed_controls','colorify_editor_creation_close_reentry','colorify_editor_draft_reentry'];results={}
if mode=='green':(O/'screenshots').mkdir(exist_ok=True);env['GIMP_TEST_COLORIFY_SCREENSHOT_DIR']=str(O/'screenshots')
for case in cases:
 cmd=[str(O/'painter-layer-ui'),'-p','/painter-layer-ui/'+case];start=time.monotonic();p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=180,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)));results[case]={'command':cmd,'exit_code':p.returncode,'stdout':p.stdout,'stderr':p.stderr,'seconds':time.monotonic()-start};(O/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(case,p.returncode,p.stdout[-1600:],p.stderr[-2500:],flush=True)
 if mode=='red':assert p.returncode!=0 and 'gtk_combo_box_set_active_id' in p.stdout+p.stderr
 else:assert p.returncode==0
j=json.loads((O/'build.json').read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert sha(O/'painter-layer-ui')==j['binary_sha256'];assert all(sha(private[p]['private_path']if p in private else p)==h for p,h in j['runtime_libraries'].items());(O/'runtime-seal.json').write_text(json.dumps({'binary_unchanged':True,'runtime_libraries_unchanged':True,'case_count':len(results)},indent=2)+'\n')
