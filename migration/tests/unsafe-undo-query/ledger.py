from pathlib import Path
import subprocess,difflib,json
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');W=Path('/workspace/shared/unsafe-undo-query');base=subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip();diff=[]
for p in ['tasks.md','migration/progress.tsv','app/tests/meson.build']:
 before=subprocess.check_output(['git','show',base+':'+p],cwd=R,text=True);after=before
 if p=='tasks.md':
  anchor=next(x for x in before.splitlines(True)if x.startswith('| 09.011/paired-fixed-bounds |'))
  after=before.replace(anchor,anchor+'| 09.011/cancelable-undo-admission | [x] | cancel可能paintとpaired transactionのUndo事前判定を分離する | 09.011/paired-fixed-bounds | native148・five-unit ASan32・header4・causal mutant2。Smudge既存26、Fill40、Save/export13を保持し、MyPaint half-sample/open strokeを別typed queryで拒否。portrait/oncanvas4枚。migration/tests/unsafe-undo-query。親・旧ABI・wholeHEAD aggregate・別persistence source admissionは未完了 |\n')
 elif p=='migration/progress.tsv':
  after=before+'09.011/cancelable-undo-admission\tDONE\tCodex\tmigration/tests/unsafe-undo-query\tnative148; five-production-unit ASan32; C11/C++14 headers4; linked mutants2; source controls4\tPASS: cancelable pending-only Undo preserved, paired half-sample/open/batch veto independent, original Save predicate unchanged; actual export prerequisite built and Save13 passed\t-\tExact503 plus bounded source overlay, not current HEAD aggregate; old proofs immutable and persistence successor separately required; no held diagnostics; native allocator and original parent/ABI limits unchanged\n'
 else:
  old="test('undo-pop-guard', undo_pop_guard, suite: 'app', timeout: 120)"
  new="test('undo-pop-guard', undo_pop_guard,\n  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),\n        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root()],\n  suite: 'app', timeout: 120)";assert before.count(old)==1;after=before.replace(old,new)
 diff.extend(difflib.unified_diff(before.splitlines(True),after.splitlines(True),'a/'+p,'b/'+p));(W/(p.replace('/','__')+'.after')).write_text(after)
(W/'shared.patch').write_text(''.join(diff));(W/'ledger-base.json').write_text(json.dumps({'base':base,'original_acceptance_changed':False,'original_source_duties_changed':False},indent=2)+'\n')
