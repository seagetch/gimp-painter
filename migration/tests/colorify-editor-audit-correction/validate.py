from pathlib import Path
import hashlib,json,subprocess
p=Path(__file__).resolve().parent;r=p.parents[2];j=json.loads((p/'old-editor-audit.json').read_text());b=(p/'old-editor-source.cpp').read_bytes()
assert hashlib.sha256(b).hexdigest()==j['sha256']
assert b==subprocess.check_output(['git','show',j['old_ref']+':'+j['path']],cwd=r)
for anchor in [b'void update_filter_edit(',b'GIMP_IS_PARAM_SPEC_RGB(pspec)',b'filter_layer->set_procedure(this->proc_name, this->proc_args)']:assert anchor in b
assert not subprocess.check_output(['git','diff','a2eb200d4f','--','migration/tests/colorify-editor'],cwd=r)
print('PASS explicit old generic popup/RGB audit correction; a2eb200d4f product7+7 evidence unchanged')
