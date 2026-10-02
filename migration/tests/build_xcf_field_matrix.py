#!/usr/bin/env python3
"""Deterministic field coverage ledger. Coverage is never inferred from pixels.

This is an index of independently stated assertions, including explicit remaining
edges. It does not turn source inspection or missing tests into pass/fail claims.
"""
from pathlib import Path
import csv, re
ROOT=Path(__file__).resolve().parents[2]
OUTPUT=ROOT/'migration/inventory/xcf-field-acceptance.tsv'
rows=[]
columns=['id','owner','feature','field','wire','type','default_or_absence','default_evidence','value_test','fixture','acceptance','remaining','reader_writer']
def add(owner,feature,field,wire,typ,default,test,fixture,status='application-assertion',remaining='',source='app/xcf/xcf-load.c;app/xcf/xcf-save.c',default_evidence='source-audited; absence not independently mutated'):
 rows.append(dict(zip(columns,[f'XCF-F{len(rows)+1:03}',owner,feature,field,wire,typ,default,default_evidence,test,fixture,status,remaining,source])))
attrs='/painter-xcf-fields/all_owner_attributes'
text='/painter-xcf-fields/legacy_text_and_units'
meta='/painter-xcf-fields/profiles_palettes_metadata'
ordinary='/painter-xcf-roundtrip/ordinary_exact_roundtrip'
upstream='/gimp-xcf/write_and_read_gimp_2_6_format_unusual'
fixtures={'fields':'generated native scene in app/tests/test-painter-xcf-fields.inc','legacy':'migration/fixtures/legacy-runtime/ordinary-layers.xcf','text':'migration/fixtures/legacy-xcf-fields/rgb-fields.xcf','palette':'migration/fixtures/legacy-xcf-fields/indexed-fields.xcf','upstream':'generated native mainimage in app/tests/test-xcf.c'}
defaults={'VISIBLE':'TRUE','LINKED':'FALSE; old links migrate to named item sets','LOCK_ALPHA':'FALSE','LOCK_CONTENT':'FALSE','LOCK_POSITION':'FALSE','LOCK_VISIBILITY':'FALSE','APPLY_MASK':'TRUE (load local)','EDIT_MASK':'FALSE (load local)','SHOW_MASK':'FALSE (load local)','SHOW_MASKED':'TRUE from channel/mask constructor; base channel init alone is FALSE','OFFSETS':'0,0','GROUP_ITEM':'ordinary GimpLayer','GROUP_ITEM_FLAGS':'0; not expanded','TEXT_LAYER_FLAGS':'0; auto-rename allowed and unmodified; text parasite selects editable subtype','OPACITY':'layer=1; channel/mask=black alpha1','MODE':'modern Normal; shared historical v0–3 uses explicit Painter Normal policy','TATTOO':'image state0; item constructor assigns unique image tattoo; saved collisions repaired','PARASITES':'no persisted entries; native constructor state separate','UNIT':'configured image/template unit (normally inch); user units use value-based lookup','COLORMAP':'NULL palette until indexed initialization/record','GUIDES':'empty list','SAMPLE_POINTS':'empty list','RESOLUTION':'native image constructor/template; invalid stored values fall back to default template','COMPRESSION':'NONE','ACTIVE_LAYER':'post-load first layer if no selection marker','ACTIVE_CHANNEL':'none','SELECTION':'empty image selection mask','FLOATING_SELECTION':'none','PATHS':'empty','VECTORS':'empty','USER_UNIT':'built-in unit unless user-unit record supplied','END':'required property-list terminator','COLOR':'black with opacity1'}
with (ROOT/'migration/inventory/xcf-wire-records.tsv').open() as f:
 for r in csv.DictReader(f,delimiter='\t'):
  owner,field=r['owner'],r['field'];t='';fix='source inventory only';status='source-audited';remain='Dedicated field assertion/fixture remains; not evidence of data loss'
  if field in ['signature/version','width/height/base-type','layer/channel-offset-tables','width/height/type/name','hierarchy/mask offsets','width/height/name/hierarchy','END','COMPRESSION']:
   t='/painter-xcf-open/ordinary';fix=fixtures['legacy'];status='application-structural';remain='Malformed/recursive offset graph audit remains distinct'
  if field in ['VISIBLE','LOCK_ALPHA','LOCK_CONTENT','APPLY_MASK','EDIT_MASK','SHOW_MASK','OFFSETS','TATTOO','PARASITES','OPACITY','MODE','SHOW_MASKED']:
   t=attrs;fix=fixtures['fields'];status='application-assertion';remain='Modern nondefault owner values asserted; legacy property-order permutations remain'
  if field == 'GROUP_ITEM_FLAGS':t=attrs;fix=fixtures['fields'];status='application-assertion';remain=''
  if field in ['GROUP_ITEM','ITEM_PATH']:
   t='/painter-xcf-open/ordinary';fix=fixtures['legacy'];status='application-assertion';remain=''
  if field in ['GUIDES','RESOLUTION','UNIT','SAMPLE_POINTS','PATHS','VECTORS','FLOATING_SELECTION','ACTIVE_LAYER','ACTIVE_CHANNEL','SELECTION','COLOR']:
   t=upstream;fix=fixtures['upstream'];status='upstream-application-assertion';remain='Separate old custom-type/property-order permutations not claimed'
  if field=='TEXT_LAYER_FLAGS':t=text;fix=fixtures['text'];status='application-editability';remain='Modified/rasterized text flags and gdyntext variants remain separate cases'
  if field=='USER_UNIT':t=text;fix=fixtures['text'];status='genuine-old-writer-application';remain='Old singular/plural fields archived; modern public unit exposes name/symbol/abbreviation'
  if field=='COLORMAP':t=meta;fix=fixtures['palette'];status='genuine-old-writer-application';remain='v0 historical malformed colormap framing remains source-only'
  if field=='LINKED':t=text;fix=fixtures['text'];status='genuine-old-writer-application';remain='Legacy layer/channel/path links migrate into native named sets; genuine linked path controls and membership asserted'
  if field=='FILTER_SPEC':t='/painter-xcf-roundtrip/filter_cache_and_arguments';fix='migration/fixtures/legacy-runtime/filter-edge.xcf';status='genuine-old-writer-application';remain='Old reader crashes on this genuine saved file; new reader restores raw/current definition separately'
  if field=='CLONE_SPEC':t='/painter-xcf-roundtrip/clones_edit_save_reopen';fix='migration/fixtures/legacy-runtime/clone-normal-in-group.xcf;clone-group.xcf';status='genuine-old-writer-application';remain=''
  if owner=='filter-arg':
   t='painter-xcf-compat tests; /painter-xcf-roundtrip/typed_nested_expired_arguments';fix='synthetic wire quirks; genuine filter-edge.xcf (only tags2,1,10,5,0)';status='raw-wire-and-typed-model';remain='Old writer omitted data cannot be recreated; typed native arguments are a separate tested representation'
  if field=='UNKNOWN':t='/painter-xcf-roundtrip/duplicate_unknown_records';fix='synthetic ordered duplicate properties in genuine Clone container';status='raw-preservation';remain='Known malformed properties and graph/resource limits remain separate'
  add(owner,'legacy-wire',field,r['wire_id'],r['wire_type'],defaults.get(field,r['absence_or_missing_data']),t,fix,status,remain,r['legacy_reader']+'; '+r['legacy_writer']+'; '+r['port_location'])
# Explicit modern fields absent from the historical wire inventory.
for owner in ['layer','channel/mask','path']:
 for field,tag,typ,default in [('COLOR_TAG',34,'u32 enum','NONE'),('LOCK_POSITION',32,'u32 boolean','FALSE'),('LOCK_VISIBILITY',42,'u32 boolean','FALSE')]:
  add(owner,'modern-attributes',field,tag,typ,default,attrs,fixtures['fields'])
for owner in ['layer','channel/mask']:
 add(owner,'modern-attributes','FLOAT_OPACITY',33,'f32','constructor opacity; supersedes integer opacity in encounter order',attrs,fixtures['fields'])
for field,tag,default in [('BLEND_SPACE',37,'AUTO'),('COMPOSITE_SPACE',36,'AUTO'),('COMPOSITE_MODE',35,'AUTO')]:
 add('layer','modern-compositing',field,tag,'u32 enum',default,attrs,fixtures['fields'])
add('channel/mask','modern-color','FLOAT_COLOR',38,'3*f32 sRGB; alpha is separate FLOAT_OPACITY','black alpha1',attrs,fixtures['fields'],'application-assertion','Nondefault sRGB+float alpha asserted; channel-specific ICC color encoding does not exist in this record')
add('image','sampling','SAMPLE_POINTS',39,'x:i32,y:i32,pick-mode:u32','none',meta,fixtures['fields'],'application-assertion','Nondefault LCh pick mode and coordinates asserted')
for owner in ['layer','channel','path']:
 add('image','item-sets',owner+' set definition',40,'item-type,selection-method,name','empty sets',attrs,fixtures['fields'])
 add(owner,'item-sets','ITEM_SET_ITEM',41,'u32 set index','not a member',attrs,fixtures['fields'])
add('path','selection','SELECTED_PATH',43,'empty','none',upstream,fixtures['upstream'])
for field,default in [('icc-profile','built-in profile derived from image base type and precision'),('simulation-icc-profile','NULL'),('simulation-rendering-intent','relative colorimetric'),('simulation-bpc','FALSE'),('gimp-image-metadata','NULL')]:
 add('image','metadata',field,21,'persistent native parasite',default,meta,fixtures['fields'])
for field,default in [('gimp-image-grid','native configured grid'),('gimp-image-symmetry','identity/no stored nonidentity symmetry')]:
 add('image','metadata',field,21,'serialized GimpConfig parasite',default,upstream if field.endswith('grid') else meta,fixtures['upstream'] if field.endswith('grid') else fixtures['fields'],'upstream-application-assertion' if field.endswith('grid') else 'application-assertion','Complete nondefault mirror config and active type asserted; other symmetry classes remain' if field.endswith('symmetry') else '')
# Every saved GimpText config property, including old/new representation facts.
s=(ROOT/'app/text/gimptext.c').read_text()
text_defaults={'text':'NULL','markup':'NULL','font':'context fallback font','font-size':'24','font-size-unit':'pixel','antialias':'TRUE','hint-style':'MEDIUM','kerning':'FALSE','language':'locale default','base-direction':'LTR','color':'black','outline':'NONE','justify':'LEFT','indent':'0','line-spacing':'0','letter-spacing':'0','box-mode':'DYNAMIC','box-width':'0','box-height':'0','box-unit':'pixel','transformation':'identity2x2','offset-x':'0','offset-y':'0','hinting':'legacy compatibility alias'}
for typ,field in re.findall(r'GIMP_CONFIG_PROP_(\w+)\s*\(object_class,\s*\w+,\s*"([^"]+)"',s):
 add('text-layer','editable-text',field,21,'GimpText config '+typ,text_defaults.get(field,'see GParamSpec in app/text/gimptext.c'),text,fixtures['text'],'config-equality-and-editability','Complete configured text compares after Save; many optional fields retain defaults rather than every nondefault variation','app/text/gimptext.c;app/text/gimptext-parasite.c;app/text/gimptextlayer-xcf.c')
# Native effects remain independent from Painter FilterLayer definitions.
for field,wire,typ,default in [('name','header','XCF string','required'),('icon','header','XCF string','gimp-gegl fallback'),('operation','header','XCF string','required installed safe operation'),('operation-version','v22+ header','XCF string','NULL on prior versions'),('VISIBLE',8,'u32','FALSE in zero-initialized load carrier'),('FLOAT_OPACITY',33,'f32','0 in zero-initialized load carrier'),('MODE',7,'u32','0 legacy Normal in zero-initialized load carrier'),('BLEND_SPACE',37,'u32','AUTO'),('COMPOSITE_SPACE',36,'u32','AUTO'),('COMPOSITE_MODE',35,'u32','AUTO'),('FILTER_REGION',44,'u32','0 selection'),('FILTER_CLIP',46,'u32','FALSE'),('mask-offset','tail','u32/u64','0 no stored mask'),('UNKNOWN','other','tag/size/bytes','none')]:
 add('modern-effect','effect',field,wire,typ,default,'/painter-xcf-fields/modern_effect_records',fixtures['fields'],'application-assertion','Icon and operation-version parsed structurally; explicit string mutation/absence cases not yet asserted' if field in ['icon','operation-version'] else '')
for typ in ['INT','BOOL','FLOAT','STRING','ENUM','CONFIG','UINT','COLOR','UNKNOWN']:
 add('modern-effect','effect-argument',typ,45,'property-name, type-tag, typed-value','installed GEGL operation default','/painter-xcf-fields/modern_effect_records' if typ=='UNKNOWN' else '/painter-xcf-fields/modern_effect_argument_families',fixtures['fields'],'application-assertion','Installed scalar/config/color/string/enum families are asserted; unknown and mismatched records stay inert and byte-exact')
# All current Painter namespace keys emitted by the sole serialization adapter.
s=(ROOT/'app/xcf/painter-xcf-preserve.cpp').read_text(); keys=sorted(set(re.findall(r'dict\.put\s*\(\s*"([^"]+)"',s)))
keys+=['procedure','pending-name','source-name','original-source-name','external-reference-origins']
for key in sorted(set(keys)):
 owner='image/item archive' if key.startswith('original-') or key=='opaque-record-sequences' else 'image' if key=='dialect' else 'Painter item'
 default='absent unless stored; validated schema may require explicit presence'
 t='/painter-xcf-roundtrip/filter_cache_and_arguments' if key in ['procedure','definition','has-definition','arguments','has-arguments'] else '/painter-xcf-roundtrip/malformed_filter_cache_stays_opaque' if key in ['generation','cache-generation','cache-complete','saved-state'] else '/painter-xcf-roundtrip/malformed_clone_reference_stays_opaque' if key in ['source-id','source-state','source-expired','allow-name-lookup','pending-name','source-name'] else '/painter-xcf-roundtrip/external_references_remain_unresolved' if key=='external-reference-origins' else '/painter-xcf-roundtrip/duplicate_unknown_records'
 add(owner,'Painter-v1',key,'PARASITES21','canonical little-endian a{sv}; see xcf-painter-v1.md',default,t,'existing roundtrip synthetic/genuine scenes','application-schema-and-preservation','Field schema described in xcf-painter-v1.md; no claim that each unknown nested future shape is executable','app/xcf/painter-xcf-preserve.cpp;app/xcf/painter-xcf-arguments.cpp')
add('group/channel/mask/path/text','owner-validation','clone/filter capsule on incompatible owner',21,'unchanged capsule bytes','inert; never promoted or rewritten as ordinary','/painter-xcf-fields/wrong_owner_capsules',fixtures['fields'],'application-assertion','Ten cases cover both Clone and Filter on group/channel/mask/path/text through repeated save/open')
add('image/item archive','resource-bound','retained metadata >256MiB',21,'GBytes backed by actual256MiB+1 sparse file','maximum capsule256MiB-4096 including12-byte header','/painter-xcf-fields/retained_metadata_bound',fixtures['fields'],'explicit-preflight-refusal','No chunked representation; Save refuses before replacement and retains in-memory origin. Valid large files remain readable; saving such retained records unsupported')
add('Painter FilterLayer','active-save','RUNNING/IMPORTING cache lineage','v1 capsule+pixels','committed cache snapshot plus generation relationship','stale cache reruns in a new runtime epoch','/painter-xcf-active/running_save_close_reopen;/painter-xcf-active/importing_save_close_reopen','generated actual1025x1025 spill Gaussian','application-lifecycle-assertion','No claim of GUI sustained stress or fatal crash-handler lifecycle')
add('image-header','native-precision','precision and compression','header/17','u32 precision; u8 compression','version-dependent precision; NONE without compression property','/painter-xcf-fields/native_precision_and_compression',fixtures['fields'],'application-assertion','RGB/Gray x18precision/TRC x2compression exact bytes; not a big-endian-platform assertion')
add('native effect owner','unsupported-operation','SAVE_REFUSAL','typed provenance text','static diagnostic (not serialized)','absent for supported native effects','/painter-xcf-fields/unsupported_effect_refuses_save',fixtures['fields'],'preflight-refusal','Missing/unsafe/version-mismatched operation remains nonexecuting; original stream retained and lossy Save refused. Dedicated missing-operation+image-duplicate test; not editable opaque effects')
add('CloneLayer','positive-ID-reference','same-name later source','source-id','nonzero file-local ID','no guessed name binding','/painter-xcf-fields/clone_duplicate_name_identity',fixtures['fields'],'application-assertion','Insertion/load may uniquify names; saved duplicate-name scene still binds by exact later source identity and updates pixels')
for row in rows:
 if row['owner']=='layer' and row['field'] in ['VISIBLE','OPACITY','MODE','OFFSETS','LOCK_ALPHA','LOCK_CONTENT','LOCK_POSITION','LOCK_VISIBILITY','COLOR_TAG','BLEND_SPACE','COMPOSITE_SPACE','COMPOSITE_MODE']:
  row['default_evidence']='/painter-xcf-fields/absent_native_properties (synthetic standard v10 with optional properties absent)'
assert len(rows)>=150
with OUTPUT.open('w') as f:
 w=csv.DictWriter(f,columns,delimiter='\t',lineterminator='\n');w.writeheader();w.writerows(rows)
print(f'{len(rows)} explicit field/owner rows; remaining column is authoritative for unclosed edges')
