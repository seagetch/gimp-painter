#!/usr/bin/env python3
"""Source/build/runner contracts only, not native renderer proof."""
import hashlib,json,pathlib,re,unittest
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[1]
TEST='app/tests/test-painter-canvas-ui.c'
PROD='app/widgets/gimppainterlayertiles.cpp'
class MaskIntegration(unittest.TestCase):
 def test_old_fixture_exact(self):
  self.assertEqual(hashlib.sha256((ROOT/'app/tests/color-ownership/fixtures/gimppainterlayertiles-e25.cpp').read_bytes()).hexdigest(),'8c6addf0e246a1cb9800e3eb515f4af9797dabdd6b51e72a83c4ca2bc8d7ea6d')
 def test_patched_test_exact(self):
  self.assertEqual(hashlib.sha256((ROOT/TEST).read_bytes()).hexdigest(),'32cc6ad0b168f902591e713eeabb8d46526969e59c89e2225e81abe0059f321f')
 def test_production_exact(self):
  self.assertEqual(hashlib.sha256((ROOT/PROD).read_bytes()).hexdigest(),'e92dfef13a7f6cdcf7a2cddcd0f06e6c6c6304b641cf877cfa355ddfabe23dd3')
 def test_original_test_reconstructed_without_removing_any_old_assertion(self):
  text=(ROOT/TEST).read_text();start=text.index('  gimp_layer_apply_mask(target,GIMP_MASK_DISCARD,FALSE);\n');end=text.index('  g_assert_null(gimp_layer_get_mask(target));g_assert_cmpuint(mask_finalized,==,1);',start)
  old=text[:start]+'  gimp_layer_apply_mask(target,GIMP_MASK_DISCARD,FALSE);drain();\n'+text[end:]
  self.assertEqual(hashlib.sha256(old.encode()).hexdigest(),'6724e3e0a2cbe3a0ab73fb31a0b88c87a9727bce09d2779e3af9c8b2cff523dc')
 def test_immediate_snapshot_before_drain_then_release(self):
  text=(ROOT/TEST).read_text();start=text.index('  gimp_layer_apply_mask(target,GIMP_MASK_DISCARD,FALSE);\n');block=text[start:text.index('  close_tiles(w);',start)]
  tokens=['gimp_layer_apply_mask','g_assert_null(gimp_layer_get_mask(target))','mask_finalized,==,0','canvas_snapshot(layer_canvas(w))','(red>>16)&255,>,240','red&255,<,15','drain();','mask_finalized,==,1']
  positions=[block.index(x) for x in tokens];self.assertEqual(positions,sorted(positions));self.assertEqual(block.count('drain();'),1)
 def test_full107_registration_ids_preserved(self):
  j=json.loads((HERE/'canvas-mask-full-cases.json').read_text());ids=j['case_ids'];self.assertEqual(len(ids),107);self.assertEqual(len(set(ids)),107);self.assertEqual(j['source_group'],'original')
  text=(ROOT/TEST).read_text()
  for case in ids:self.assertIn('ADD('+case.rsplit('/',1)[-1]+');',text)
 def test_same_test_sources_direct_old_only(self):
  text=(ROOT/'app/tests/color-ownership/meson.build').read_text();section=text[text.index('# Bounded mask-window'):]
  self.assertEqual(section.count("'../test-painter-canvas-ui.c', '../tests.c'"),2)
  self.assertEqual(section.count("'fixtures/gimppainterlayertiles-e25.cpp'"),1)
  self.assertIn("if host_machine.system() == 'linux'",section);self.assertIn("if get_option('b_sanitize') == 'none'",section)
  self.assertEqual(section.count('should_fail: true'),1);self.assertIn('GIMP_TEST_PAINTER_CANVAS_GROUP=original',section)
 def test_shared_icon_fixture_root_explicit_for_nested_providers(self):
  text=(ROOT/'app/tests/color-ownership/meson.build').read_text()
  self.assertIn("'G_TEST_BUILDDIR=' + meson.project_build_root()/'app'/'tests'",text)
  self.assertEqual(text.count('env: mask_proof_env'),2)
  self.assertEqual(text.count('depends: gimp_test_icon_dependencies'),2)
 def test_additive_targets_and_after_existing_runtime(self):
  j=json.loads((HERE/'build-targets.json').read_text());self.assertEqual(j['candidate_union_count'],35);self.assertEqual(len(j['candidate_union']),35);self.assertEqual(len(j['normal_negative_control_targets']),3)
  text=(HERE/'run_brush_ci.sh').read_text();self.assertLess(text.index('stage=brush-runtime'),text.index('stage=canvas-mask-provider-proof'));self.assertLess(text.index('stage=frame-style-old-provider-red'),text.index('stage=canvas-mask-provider-proof'))
  self.assertIn('"$out" "$mask_mode" || failed=1',text);self.assertIn('if [[ "$mode" != baseline-red ]]; then\n  stage=canvas-mask-provider-proof',text)
if __name__=='__main__':unittest.main()
