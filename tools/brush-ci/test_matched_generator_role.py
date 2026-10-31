import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import matched_provenance as m

class GeneratorRole(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
  self.root=Path(self.tmp.name);self.s=self.root/'source';self.b=self.root/'baseline-sanitizer'/'build'
  self.roots={'source':self.s,'build':self.b};self.obj=m.GENERATOR['object'];self.exe=m.GENERATOR['executable']
  self.git_output=m.SUBMODULE+'\n'
  self.targets=[{'name':'colorsvg2png','type':'executable','installed':False,'defined_in':str(self.s/m.GENERATOR['declaration']),
   'filename':[str(self.b/self.exe)],'target_sources':[{'language':'c','machine':'build','sources':[str(self.s/m.GENERATOR['source'])],'generated_sources':[],
   'parameters':['-I'+str(self.s),'compile-flag']},{'linker':['cc'],'parameters':['-Wl,--as-needed']}]}]
  self.targets += [{'type':'custom','filename':[str(self.b/p)]} for p in m.GENERATOR_RESOURCES]
  self.object_outputs=[self.exe];self.exe_outputs=list(m.GENERATOR_RESOURCES[:2])+['all']
  self.link=f'cc -o {self.exe} {self.obj} -Wl,--as-needed\n'
  self.commands=f'cc -c {self.s/m.GENERATOR["source"]} -o {self.obj}\n'+self.link
  pin=m.digest(m.normalize('\0'.join(m.shlex.split(self.commands.splitlines()[0])),self.roots).encode())
  change=patch.dict(m.GENERATOR,recipe_sha256=pin);change.start();self.addCleanup(change.stop)
 def query(self,target,outputs):return target+':\n  input: c_LINKER\n    input\n  outputs:\n'+''.join('    '+x+'\n' for x in outputs)
 def fake_run(self,args,**kw):
  if args[-1] in m.GENERATOR_RESOURCES[2:]:
   png=m.GENERATOR_RESOURCES[m.GENERATOR_RESOURCES.index(args[-1])-2]
   return args[-1]+':\n  input: CUSTOM_COMMAND\n    resource.xml\n    | '+png+'\n  outputs:\n    resource.c.o\n'
  if args[-2:]==['commands',self.exe]:return self.commands
  if args[-1]==self.obj:return self.query(self.obj,self.object_outputs)
  if args[-1]==self.exe:return self.query(self.exe,self.exe_outputs)
  raise AssertionError('unexpected command')
 def regular(self,path):
  for k in ('source','declaration'):
   if path==self.s/m.GENERATOR[k]:return m.GENERATOR[k+'_sha256']
  return m.digest(str(path.relative_to(self.b)).encode())
 def invoke(self):
  with patch.object(m,'git',return_value=self.git_output),patch.object(m,'regular',side_effect=self.regular),patch.object(m,'run',side_effect=self.fake_run):
   return m.generator_role(self.s,self.b,self.targets,self.roots)
 def bad(self,code):
  with self.assertRaisesRegex(m.EvidenceError,code):self.invoke()
 def test_git_mismatch_diagnostic_public_sha_only(self):
  with self.assertRaises(m.GeneratorSubmoduleError) as caught:m.validate_generator_submodule('0'*40+'\n')
  d=caught.exception.diagnostic;self.assertEqual(d['observed_commit'],'0'*40);self.assertEqual(d['expected_commit'],m.SUBMODULE);self.assertEqual(d['output_shape'],'one-lf-hash')
 def test_git_malformed_diagnostic_hides_output(self):
  secret='PRIVATE_TOKEN=/private/path '+('x'*5000)
  with self.assertRaises(m.GeneratorSubmoduleError) as caught:m.validate_generator_submodule(secret)
  d=caught.exception.diagnostic;self.assertIsNone(d['observed_commit']);self.assertEqual(d['output_shape'],'other');self.assertEqual(d['output_bytes_capped'],4096);self.assertNotIn('PRIVATE',json.dumps(d));self.assertNotIn('/private',json.dumps(d))
 def test_git_mismatch_cli_keeps_failure_and_context(self):
  output=self.root/'failure.json'
  argv=['matched_provenance.py','build-identity','--source',str(self.s),'--build',str(self.b),'--deps',str(self.root/'deps'),'--cell','baseline-normal','--output',str(output)]
  with patch('sys.argv',argv),patch.object(m,'build_identity',side_effect=m.GeneratorSubmoduleError('0'*40+'\n')):
   self.assertEqual(m.main(),1)
  data=json.loads(output.read_text());self.assertFalse(data['verified']);self.assertEqual(data['errors'],['generator-submodule-mismatch']);self.assertEqual(data['generator_submodule_failure']['observed_commit'],'0'*40)
 def test_git_single_lf_matches(self):self.invoke()
 def test_git_wrong_commit_rejected(self):self.git_output='0'*40+'\n';self.bad('generator-submodule-mismatch')
 def test_git_extra_line_rejected(self):self.git_output=m.SUBMODULE+'\nextra\n';self.bad('generator-submodule-mismatch')
 def test_git_double_lf_rejected(self):self.git_output=m.SUBMODULE+'\n\n';self.bad('generator-submodule-mismatch')
 def test_git_surrounding_whitespace_rejected(self):self.git_output=' '+m.SUBMODULE+'\n';self.bad('generator-submodule-mismatch')
 def test_git_crlf_rejected(self):self.git_output=m.SUBMODULE+'\r\n';self.bad('generator-submodule-mismatch')
 def test_valid_role_and_private_paths_absent(self):
  r=self.invoke();self.assertEqual(r['target_machine'],'build');self.assertNotIn(str(self.root),json.dumps(r))
  self.assertEqual(set(r['generated_resources']),set(m.GENERATOR_RESOURCES))
 def test_all_only_consumer_rejected(self):self.exe_outputs=['all'];self.bad('generator-host-consumer')
 def test_missing_png_resource_edge(self):
  original=self.fake_run
  self.fake_run=lambda args,**kw: original(args,**kw).replace('    | '+m.GENERATOR_RESOURCES[0],'    | unrelated.png')
  self.bad('generator-resource-edge-missing')
 def test_malformed_target_block(self):self.targets[0]['target_sources']=[7];self.bad('generator-target-mismatch')
 def test_shell_text_link_rejected(self):self.commands=self.commands.replace('-Wl,--as-needed','-Wl,--as-needed;touch');self.bad('generator-link-shape')
 def test_host_machine_rejected(self):self.targets[0]['target_sources'][0]['machine']='host';self.bad('generator-machine-source-mismatch')
 def test_installed_rejected(self):self.targets[0]['installed']=True;self.bad('generator-target-mismatch')
 def test_duplicate_target_rejected(self):self.targets.append(copy.deepcopy(self.targets[0]));self.bad('generator-target-ambiguous')
 def test_wrong_source_rejected(self):self.targets[0]['target_sources'][0]['sources']=['wrong.c'];self.bad('generator-machine-source-mismatch')
 def test_generator_obj_host_consumer(self):self.object_outputs.append('app/tests/matched-preclick');self.bad('generator-object-owner-mismatch')
 def test_generator_binary_host_consumer(self):self.exe_outputs.append('libgimpwidgets/libgimpwidgets.so');self.bad('generator-host-consumer')
 def test_host_link_injection(self):
  self.targets.append({'target_sources':[{'linker':['cc'],'parameters':[self.exe]}]});self.bad('generator-host-link')
 def test_opaque_host_link(self):
  self.targets.append({'target_sources':[{'linker':['cc'],'parameters':['@link.rsp']}]});self.bad('opaque-link-input')
 def test_opaque_generator_link(self):self.commands=self.commands.replace('-Wl,--as-needed','@link.rsp');self.bad('generator-link-shape')
 def test_actual_compile_recipe_mismatch(self):self.commands=self.commands.replace('cc -c','cc -DCHANGED -c');self.bad('generator-command-recipe-mismatch')
 def test_wrapped_linker(self):self.commands=self.commands.replace(self.link,'wrapper '+self.link);self.bad('generator-link-shape')
 def test_wrong_link_object(self):self.commands=self.commands.replace(self.link,self.link.replace(self.obj,'wrong.o'));self.bad('generator-link-object')
 def test_extra_link_object(self):self.commands=self.commands.replace('-Wl,--as-needed','extra.o');self.bad('generator-link-object')
 def test_wrong_link_output(self):self.commands=self.commands.replace(self.link,self.link.replace('-o '+self.exe,'-o bad'));self.bad('generator-link-output')
 def test_generator_sanitizer_flag_rejected(self):self.commands=self.commands.replace('-Wl,--as-needed','-fsanitize=address');self.bad('generator-link-sanitizer')
 def test_same_named_unpinned_source(self):
  old=self.regular
  self.regular=lambda p: '0'*64 if p==self.s/m.GENERATOR['source'] else old(p)
  self.bad('generator-source-mismatch')
 def test_query_unknown_shape(self):
  self.query=lambda target,outputs:target+':\n  input: c_LINKER\n  validations:\n    unknown\n'
  self.bad('generator-query-shape')
 def test_no_generic_native_exemption(self):
  with self.assertRaises(m.ObjectSanitizerError):m.validate_object_sanitizer(['cc'],'cc','baseline-sanitizer','tools/other.p/other.c.o','source/tools/other.c',1,2)

if __name__=='__main__':unittest.main()
