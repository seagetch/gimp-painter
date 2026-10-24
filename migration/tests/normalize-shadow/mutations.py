from pathlib import Path
import json,subprocess,tempfile,os,hashlib
R=Path(__file__).resolve().parents[3];E=Path(__file__).resolve().parent
base=json.loads((E/'results.json').read_text());rows=[]
with tempfile.TemporaryDirectory(prefix='normalize-shadow-mutants-',dir='/tmp')as tmp:
 D=Path(tmp)
 for p,h in base['source_sha256'].items():
  data=(R/p).read_bytes();assert hashlib.sha256(data).hexdigest()==h;pout=D/p;pout.parent.mkdir(parents=True,exist_ok=True);pout.write_bytes(data)
 obj=D/'oracle.o';cc=[str(v).replace(str(R),str(D)) for v in base['runs']['normal']['oracle_compile']];cc[cc.index('-o')+1]=str(obj);subprocess.run(cc,check=True)
 target=D/'app/painter/filter-point-shadow.cpp';old=target.read_text();at=old.index('FilterPointShadowOutcome filter_normalize_shadow_raster(');prefix=old[:at];body=old[at:]
 for name,a,b in [('borrow-region','const auto captured=region; // snapshot before storage callbacks','const auto& captured=region; // wrong borrowed region'),('wrong-area-policy','shadow_impl(input,width,height,captured,0,','shadow_impl(input,width,height,captured,3,'),('omit-normalization','return filter_normalize_raw_raster(source,destination,w,h,stop)==FilterNormalizeOutcome::complete;','(void)source;(void)destination;(void)w;(void)h;(void)stop;return true;'),('whole-image-statistics','const auto captured=region; // snapshot before storage callbacks','(void)region;const FilterSelectionRegion captured{false,true,0,0,static_cast<std::int32_t>(width),static_cast<std::int32_t>(height)};'),('publish-undefined','return filter_normalize_raw_raster(source,destination,w,h,stop)==FilterNormalizeOutcome::complete;','try { return filter_normalize_raw_raster(source,destination,w,h,stop)==FilterNormalizeOutcome::complete; } catch(const std::domain_error&) { return true; }')]:
  assert body.count(a)==1;target.write_text(prefix+body.replace(a,b));cmd=[str(v).replace(str(R),str(D))for v in base['runs']['normal']['compile']];cmd[cmd.index('-o')+1]=str(D/name);cmd=[str(obj)if v.endswith('normal-oracle.o')else v for v in cmd];build=subprocess.run(cmd,capture_output=True,text=True);assert build.returncode==0,build.stderr
  q=subprocess.run([str(D/name),str(R/'migration/fixtures/legacy-filter-points/fixtures.tsv')],capture_output=True,text=True,timeout=120);rows.append({'name':name,'command':cmd,'exit_code':q.returncode,'stdout':q.stdout,'stderr':q.stderr,'mutant_sha256':hashlib.sha256(target.read_bytes()).hexdigest()});assert q.returncode!=0,q;print('REJECT',name,flush=True)
(E/'mutations.json').write_text(json.dumps(rows,indent=2)+'\n')
