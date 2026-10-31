#!/usr/bin/env python3
"""Fail-closed compatibility proof; never a common306 control/runtime PASS."""
from __future__ import annotations
import argparse,hashlib,importlib.util,json
from pathlib import Path
from prepare_brush_baseline import commits,git

BASELINE306='d4ce384f708090facbf6b08edc38b4ddcf84cff0'
ROUTES='migration/inventory/additional-c-header-routes.json'
PROTECTED=(
 'app/tests/test-common-layer-popup.cpp',
 'app/tests/test-painter-layer-ui.c',
 'app/tests/meson.build',
 'tools/run-painter-ui-ci.sh',
 'tools/check_painter_ui_ci.py',
 'tools/painter-common-layer-popup-cases.json',
 'tools/prepare-painter-popup-baseline.py',
 'tools/check_painter_stack_diagnostic.py',
 'tools/tests/test_painter_ui_ci.py',
 'tools/tests/test_painter_popup_baseline.py',
 'tools/prepare-linux-build-deps.py',
 'tools/linux-debian13-env.sh',
 'tools/run-painter-frozen-aggregate.py',
 'migration/baseline/debian13-package-lock.json',
 'app/tests/test-overlay-paint-gate.cpp',
 'app/widgets/gimpoverlaychild.c',
 'app/widgets/gimpoverlaychild.h',
 'tools/painter-overlay-paint-gate-cases.json',
 'tools/tests/test_painter_overlay_gate_ci.py',
 'tools/prepare-painter-dockbook-diagnostic.py',
 'tools/check_painter_dockbook_diagnostic.py',
 'tools/check_painter_controlled_order_diagnostic.py',
 'tools/painter-ci-root-capture.c',
 'tools/tests/test_painter_dockbook_diagnostic.py',
 'tools/tests/test_painter_dockbook_stack.py',
 'tools/tests/test_painter_controlled_order_stack.py',
)
def sha(data):return hashlib.sha256(data).hexdigest()
def regular(root,name):
 p=root/name
 if p.is_symlink() or not p.is_file():raise ValueError('Missing/symlink source: '+name)
 return p.read_bytes()
def payload(candidate,donor,pins,baseline_routes):
 expected=pins.get('common_protected_file_sha256',{})
 if set(expected)!=set(PROTECTED):raise ValueError('Protected common file set changed; renewed review required')
 proofs={}
 for name in PROTECTED:
  a,b=regular(candidate,name),regular(donor,name)
  if a!=b or sha(b)!=expected[name]:
   raise ValueError('Common harness/helper drift; renewed control design required: '+name)
  proofs[name]=sha(b)
 before=json.loads(baseline_routes);accepted=json.loads(regular(donor,ROUTES));current=json.loads(regular(candidate,ROUTES))
 if sha(regular(donor,ROUTES))!=pins['common_donor_header_routes_sha256']:
  raise ValueError('Accepted non-Brush prerequisite inventory changed')
 # Load only after the unchanged helper's exact reviewed identity was checked.
 spec=importlib.util.spec_from_file_location('verified_common_overlay',donor/'tools/prepare-painter-popup-baseline.py')
 helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
 if helper.BASELINE!=BASELINE306 or helper.TEST_SHA256!=proofs['app/tests/test-common-layer-popup.cpp']:
  raise ValueError('Wrong306 baseline or stale common final-test identity')
 helper.check_routes(before,accepted) # Exact task306 + accepted two prerequisites.
 header_names={'app/tools/gimppaintertogglepopup.h','app/widgets/gimpblobeditor.h'}
 header_pins=pins.get('common_compile_prerequisite_headers_sha256',{})
 if set(header_pins)!=header_names:raise ValueError('Common prerequisite header proof set changed')
 addition_file=Path(__file__).with_name('brush-toggle-header-insertion.txt')
 addition=addition_file.read_bytes()
 if sha(addition)!=pins['brush_toggle_header_insertion_sha256']:
  raise ValueError('Reviewed Brush-only header insertion changed')
 header_proofs={}
 for name in sorted(header_names):
  donor_header=regular(donor,name);candidate_header=regular(candidate,name)
  if sha(donor_header)!=header_pins[name]:raise ValueError('Accepted prerequisite header drift: '+name)
  expected_header=donor_header
  if name=='app/tools/gimppaintertogglepopup.h':
   anchor=b'G_END_DECLS\n#endif\n'
   if donor_header.count(anchor)!=1:raise ValueError('Shared header insertion anchor changed')
   expected_header=donor_header.replace(anchor,addition+anchor,1)
  if candidate_header!=expected_header:
   raise ValueError('Candidate prerequisite header differs beyond reviewed Brush-only insertion: '+name)
  header_proofs[name]={'donor_sha256':sha(donor_header),'candidate_sha256':sha(candidate_header)}
 route_file=Path(__file__).with_name('brush-header-route.json')
 if sha(route_file.read_bytes())!=pins['brush_header_route_sha256']:
  raise ValueError('Reviewed Brush route pin changed')
 brush=json.loads(route_file.read_text())
 if brush.get('path')!='app/tools/gimppainterbrushpopup.h':raise ValueError('Wrong Brush route')
 if set(current)!=set(accepted) or set(accepted)!={'format','routes'}:
  raise ValueError('Header inventory schema drift')
 wanted={'format':accepted['format'],'routes':accepted['routes']+[brush]}
 if current!=wanted:raise ValueError('Candidate non-Brush prerequisites differ or extra routes were imported')
 spec=importlib.util.spec_from_file_location('verified_historical_diagnostic',donor/'tools/prepare-painter-dockbook-diagnostic.py')
 diagnostic=importlib.util.module_from_spec(spec);spec.loader.exec_module(diagnostic)
 historical=diagnostic.expected_receipt('controlled-order')
 historical={key:historical[key] for key in ('source_commit','source_tree','original_test_sha256',
  'donor_commit','donor_tree','donor_test_blob','instrumented_test_sha256','diagnostic_only','acceptance_eligible')}
 historical['mode']='dockbook-controlled-order-3d6f'
 if pins.get('historical_dockbook_diagnostic')!=historical:
  raise ValueError('Immutable historical diagnostic identity drift; renew review')
 cases=json.loads(regular(donor,'tools/painter-common-layer-popup-cases.json'))['case_ids']
 if pins.get('common_positive_case_count')!=58 or len(cases)!=58 or len(set(cases))!=58:
  raise ValueError('Common58 contract changed; renew review')
 gate_cases=json.loads(regular(donor,'tools/painter-overlay-paint-gate-cases.json'))
 wanted_gate=['/overlay-paint-gate/'+name for name in (
  'native-insertion','pending-teardown','late-layout','late-paint','redraw-token-balance','hide-remap')]
 if (gate_cases.get('test')!='painter-overlay-paint-gate' or gate_cases.get('case_ids')!=wanted_gate or
     pins.get('common_overlay_gate_case_count')!=6 or pins.get('common_overlay_gate_scales')!=[1,2]):
  raise ValueError('Overlay paint gate6/both-scales contract changed; renew review')
 return {'protected_file_sha256':proofs,'donor_inventory_sha256':sha(regular(donor,ROUTES)),
         'candidate_inventory_sha256':sha(regular(candidate,ROUTES)),
         'non_brush_prerequisite_entries_identical':True,'donor_has_only_two_306_prerequisite_additions':True,
         'candidate_only_additional_route':'app/tools/gimppainterbrushpopup.h',
         'common_prerequisite_header_proofs':header_proofs,
         'common_case_count':58,'required_common_png_count':16,
         'overlay_paint_gate_case_ids':wanted_gate,'overlay_paint_gate_scales':[1,2],
         'adjacent_registrations_required':13,'adjacent_recorded_case_count':360,
         'historical_diagnostic_source':historical} 
def verify(candidate,donor,pins):
 accepted,final=commits(pins)
 if accepted!='e25a94e21efca6e6604ddac6f15570c049f421e7' or pins.get('accepted_task307_tree')!='8634eeef49c81aaa5c25192cb76d749ff680683f':
  raise ValueError('Accepted307 immutable source/tree changed; renew review')
 if git(donor,'rev-parse','HEAD^{tree}').decode().strip()!=pins['accepted_task307_tree']:
  raise ValueError('Accepted307 donor tree mismatch')
 if accepted==final:raise ValueError('Accepted pre-Brush donor and Brush candidate must be distinct')
 if pins.get('common306_baseline_commit')!=BASELINE306:raise ValueError('Do not replace the task306 control')
 for repo,expected in ((candidate,final),(donor,accepted)):
  if git(repo,'rev-parse','HEAD').decode().strip()!=expected:raise ValueError('Wrong pinned HEAD: '+str(repo))
  if git(repo,'status','--porcelain=v1','--untracked-files=all').strip():raise ValueError('Clean checkout required: '+str(repo))
 proofs=payload(candidate,donor,pins,git(donor,'show',BASELINE306+':'+ROUTES))
 return {'status':'SOURCE_COMPATIBILITY_VERIFIED_RUNTIME_UNRUN','acceptance_eligible':False,
  'common_positive_candidate_commit':final,'common306_production_commit':BASELINE306,
  'common306_harness_donor_commit':accepted,'brush_negative_production_commit':accepted,
  'brush_negative_final_test_commit':final,'proofs':proofs,
  'scope':'Source equality only;306 RED, candidate overlay6x2/common58x2/16PNG/13adjacent360 and historical diagnostic must still execute separately'}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--donor',type=Path,required=True);p.add_argument('--pins',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 report=verify(a.candidate.resolve(),a.donor.resolve(),json.loads(a.pins.read_text()))
 a.output.write_text(json.dumps(report,indent=2)+'\n');print(report['status'])
if __name__=='__main__':main()
