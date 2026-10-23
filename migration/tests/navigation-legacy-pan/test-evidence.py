#!/usr/bin/env python3
import importlib.util,json,pathlib,shutil,tempfile,hashlib,sys
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('evidence',HERE/'check-evidence.py');v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
def main():
    results=[]
    with tempfile.TemporaryDirectory(prefix='pan-evidence-') as tmp:
        root=pathlib.Path(tmp);e=root/'migration/tests/navigation-legacy-pan';m=json.loads((HERE/'manifest.json').read_text())
        for p in m['source_sha256']:
            target=root/p;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(v.ROOT/p,target)
        inventory='migration/inventory/additional-c-header-routes.json'
        (root/inventory).parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(v.ROOT/inventory,root/inventory)
        shutil.copytree(HERE,e,dirs_exist_ok=True)
        baseline=(e/'manifest.json').read_bytes()
        def trial(name,mutate,expected=False):
            (e/'manifest.json').write_bytes(baseline)
            for p in ['native.json','expected-red.json','portrait-oncanvas-pan-m0.png']:
                shutil.copyfile(HERE/p,e/p)
            data=json.loads(baseline);mutate(data);(e/'manifest.json').write_text(json.dumps(data))
            try:v.check(root,e);ok=True
            except ValueError:ok=False
            if ok!=expected:raise AssertionError(name)
            results.append({'name':name,'expected':'PASS' if expected else 'REJECT','observed':'PASS' if ok else 'REJECT'})
        trial('valid receipt',lambda m:None,True)
        trial('wrong portrait',lambda m:m['xvfb'].update(height=1080))
        trial('oncanvas disabled',lambda m:m.update(new_ui_oncanvas=False))
        trial('assertions disabled',lambda m:m['build_policy'].update(b_ndebug=True))
        trial('inflated native cases',lambda m:m.update(native_cases=36))
        trial('oracle point count',lambda m:m.update(legacy_point_pairs=5761))
        def receipt(m):
            p=e/'native.json';r=json.loads(p.read_text());r['result']='FAIL';p.write_text(json.dumps(r));m['artifact_sha256']['native.json']=v.sha(p.read_bytes())
        trial('failed receipt with matching digest',receipt)
        def tap(m):
            p=e/'native.json';r=json.loads(p.read_text());r['stdout']=r['stdout'].replace('/navigation-events/legacy/transitions','/navigation-events/not-the-case');p.write_text(json.dumps(r));m['artifact_sha256']['native.json']=v.sha(p.read_bytes())
        trial('missing transition with matching digest',tap)
        def red(m):
            p=e/'expected-red.json';r=json.loads(p.read_text());r[0]['returncode']=0;p.write_text(json.dumps(r));m['artifact_sha256']['expected-red.json']=v.sha(p.read_bytes())
        trial('fake expected red with matching digest',red)
        def png(m):
            p=e/'portrait-oncanvas-pan-m0.png';b=bytearray(p.read_bytes());b[16:20]=(1920).to_bytes(4,'big');p.write_bytes(b);m['artifact_sha256'][p.name]=v.sha(b)
        trial('landscape screenshot with matching digest',png)
        p=root/'app/display/gimpdisplayshell-rotate.c';original=p.read_bytes();p.write_bytes(original+b'\n/* changed */\n');trial('changed production source',lambda m:None);p.write_bytes(original)
        print(json.dumps({'status':'PASS','controls':results},indent=2))
if __name__=='__main__':main()
