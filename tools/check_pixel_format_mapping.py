#!/usr/bin/env python3
"""Verify 09.001 source mapping and execute extracted real legacy/native format code.
No full GIMP, rendering, threading, or color-management parity claim.
"""
import argparse,csv,hashlib,json,os,re,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
MANIFEST=ROOT/'migration/contracts/pixel-format-mapping.json'
def run(args,**kwargs):
    return subprocess.check_output(args,text=True,**kwargs)
def function(source,name):
    match=re.search(r'^'+re.escape(name)+r'\s*\([^;]*?\)\s*\{',source,re.M)
    if not match: raise ValueError('Function missing: '+name)
    start=source.rfind('\n',0,match.start()-1)+1
    pos=source.index('{',match.start()); depth=1; end=pos+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]+'\n'
def enum(source,name):
    matches=re.findall(r'typedef enum\b[^{}]*\{[^{}]*\}\s*'+name+r'\s*;',source,re.S)
    assert len(matches)==1,name
    return matches[0]
def validate(manifest):
    duties=manifest['duties'];assert len(duties)==13
    with (ROOT/'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        ledger={r['work_id']:r for r in csv.DictReader(stream,delimiter='\t') if r['wbs_task']=='09.001'}
    assert set(ledger)=={d['work_id'] for d in duties}
    legacy={}
    for d in duties:
        for key in ('source_id','path','source_blob','source_range','source_sha256'):assert d[key]==ledger[d['work_id']][key],key
        source=run(['git','show',manifest['legacy_commit']+':'+d['path']],cwd=ROOT)
        assert hashlib.sha256(source.encode()).hexdigest()==d['blob_sha256']
        assert run(['git','rev-parse',manifest['legacy_commit']+':'+d['path']],cwd=ROOT).strip()==d['source_blob']
        assert d['mapping'] and d['check'] in ('dispatch-shape','legacy-pixel','legacy-temp')
        legacy[d['path']]=source
    proc=legacy['app/base/pixel-processor.c']
    for token in ('(* p5_func)', 'PixelRegion         *regions[5]', 'case 5:', 'processor.regions[4]'):
        assert token in proc,token
    assert proc.count('((p5_func) processor->func)')==2
    assert 'memcpy (&tr[i], processor->regions[i], sizeof (PixelRegion))' in proc
    assert manifest['indexed']['precision']=='U8_NON_LINEAR'
    assert manifest['indexed']['bytes']==[1,2]
    colormap=(ROOT/'app/core/gimpimage-colormap.c').read_text()
    assert 'babl_new_palette_with_space (format_name, space,' in colormap
    assert 'space = gimp_image_get_layer_space (image)' in colormap
    for f in manifest['formats']:
        component,trc=f['precision'].split('_',1)
        marker={'LINEAR':'','NON_LINEAR':"'",'PERCEPTUAL':'~'}[trc]
        model=''.join(c+marker for c in ('RGB' if f['base']=='RGB' else 'Y'))
        assert f['base'] in ('RGB','GRAY') and type(f['alpha']) is bool
        assert f['encoding']==model+('A' if f['alpha'] else '')+' '+component.lower()
        assert f['bytes']==({'RGB':3,'GRAY':1}[f['base']]+f['alpha'])*{'U8':1,'U16':2,'U32':4,'HALF':2,'FLOAT':4,'DOUBLE':8}[component]
    assert len(manifest['formats'])==72
    assert len({(f['base'],f['precision'],f['alpha']) for f in manifest['formats']})==72
    return legacy

def executable(manifest,legacy,sanitize):
    native=(ROOT/'app/gegl/gimp-babl.c').read_text()
    base=(ROOT/'libgimpbase/gimpbaseenums.h').read_text();core=(ROOT/'app/core/core-enums.h').read_text()
    headers='#include <glib-object.h>\n#include <babl/babl.h>\n#include <string.h>\n#include <stdio.h>\n'
    headers+='\n'.join(enum(base,n) for n in ['GimpImageBaseType','GimpComponentType','GimpPrecision'])+'\n'+enum(core,'GimpTRCType')+'\n'
    code=headers+'\n'.join(function(native,n) for n in ['gimp_babl_component_type','gimp_babl_trc','gimp_babl_precision','gimp_babl_is_valid','gimp_babl_format'])
    code+='int main(void){babl_init(); int count=0; const char *spaces[]={"sRGB","Rec2020"}; for(int s=0;s<2;s++){const Babl *space=babl_space(spaces[s]);\n'
    for f in manifest['formats']:
        code+=f'''{{ const Babl *f=gimp_babl_format(GIMP_{f['base']},GIMP_PRECISION_{f['precision']},{int(f['alpha'])},space);
g_assert_cmpstr(babl_format_get_encoding(f),==,{json.dumps(f['encoding'])});
g_assert_cmpint(babl_format_get_bytes_per_pixel(f),==,{f['bytes']});
g_assert_cmpint(babl_format_has_alpha(f),==,{int(f['alpha'])});
g_assert_true(babl_format_get_space(f)==space);g_assert_true(gimp_babl_is_valid(GIMP_{f['base']},GIMP_PRECISION_{f['precision']}));
g_assert_cmpint(gimp_babl_precision(gimp_babl_component_type(GIMP_PRECISION_{f['precision']}),gimp_babl_trc(GIMP_PRECISION_{f['precision']})),==,GIMP_PRECISION_{f['precision']});count++;}}\n'''
    code+='''const Babl *index,*indexa; babl_new_palette_with_space(s?"format-map-2020":"format-map-srgb",space,&index,&indexa);
g_assert_true(babl_format_is_palette(index));g_assert_true(babl_format_is_palette(indexa));
g_assert_cmpint(babl_format_get_bytes_per_pixel(index),==,1);g_assert_cmpint(babl_format_get_bytes_per_pixel(indexa),==,2);
g_assert_false(babl_format_has_alpha(index));g_assert_true(babl_format_has_alpha(indexa));
g_assert_true(babl_format_get_space(index)==space);g_assert_true(babl_format_get_space(indexa)==space);
}'''
    for p in sorted({f['precision'] for f in manifest['formats']}):
        code+=f'g_assert_cmpint(gimp_babl_is_valid(GIMP_INDEXED,GIMP_PRECISION_{p}),==,{int(p=="U8_NON_LINEAR")});\n'
    code+='printf("native-format-cases=%d indexed-palettes=4 indexed-precision-checks=18\\n",count);babl_exit();return 0;}\n'
    pixel=legacy['app/base/pixel.hpp'];pixel=pixel[pixel.index('struct Pixel {'):pixel.index('template<typename T1, typename T2>\ninline')]
    h=legacy['app/base/temp-buf.h'];struct=re.search(r'struct _TempBuf\s*\{.*?\};',h,re.S).group()
    temp=legacy['app/base/temp-buf.c']
    old='#include <glib.h>\n#include <string.h>\n#include <stdio.h>\n'+pixel+'\ntypedef struct _TempBuf TempBuf;\n'+struct+'\n'
    rgb=run(['git','show',manifest['legacy_commit']+':libgimpcolor/gimprgb.h'],cwd=ROOT)
    start=rgb.index('#define GIMP_RGB_LUMINANCE_RED')
    old+=rgb[start:rgb.index('#ifndef GIMP_DISABLE_DEPRECATED',start)]+'\n'
    old+='gint temp_buf_get_rowstride(TempBuf*);\n'
    old+='guchar *temp_buf_get_data(const TempBuf*); gsize temp_buf_get_data_size(TempBuf*);\n'
    old+='static void temp_buf_to_color(TempBuf*,TempBuf*); static void temp_buf_to_gray(TempBuf*,TempBuf*);\n' 
    for name in ['temp_buf_new','temp_buf_get_rowstride','temp_buf_subwindow','temp_buf_free','temp_buf_get_data','temp_buf_get_data_size','temp_buf_to_color','temp_buf_to_gray','temp_buf_copy']:
        old+=function(temp,name)
    old+='''int main(){for(int i=0;i<256;i++){g_assert_cmpint(Pixel::from_f(Pixel::to_f(i)),==,i);g_assert_cmpint(Pixel::to_u8(Pixel::from_u8(i)),==,i);}
g_assert_cmpint(Pixel::DEPTH,==,8);g_assert_cmpint(Pixel::from_f(-1),==,0);g_assert_cmpint(Pixel::from_f(2),==,255);
for(int b=1;b<=4;b++){guchar color[]={11,22,33,44};TempBuf *p=temp_buf_new(7,5,b,10,20,color);g_assert_cmpint(temp_buf_get_rowstride(p),==,7*b);
for(int i=0;i<35*b;i++)g_assert_cmpint(p->data[i],==,color[i%b]);
TempBuf *child=temp_buf_subwindow(p,12,21,3,2);g_assert_true(child->parent==p);g_assert_cmpint(temp_buf_get_rowstride(child),==,7*b);g_assert_true(child->data==p->data+7*b+2*b);
temp_buf_free(child);g_assert_cmpint(p->data[0],==,11);g_slice_free(TempBuf,child);temp_buf_free(p);}
for(int alpha=0;alpha<2;alpha++){guchar rgb[]={11,22,33,44};TempBuf *color=temp_buf_new(2,1,3+alpha,0,0,rgb);TempBuf *gray=temp_buf_new(2,1,1+alpha,0,0,NULL);
temp_buf_copy(color,gray);for(int i=0;i<2;i++){g_assert_cmpint(gray->data[i*(1+alpha)],==,20);if(alpha)g_assert_cmpint(gray->data[i*2+1],==,44);}
temp_buf_copy(gray,color);for(int i=0;i<2;i++){for(int c=0;c<3;c++)g_assert_cmpint(color->data[i*(3+alpha)+c],==,20);if(alpha)g_assert_cmpint(color->data[i*4+3],==,44);}
TempBuf *copy=temp_buf_copy(color,NULL);g_assert_cmpmem(copy->data,copy->bytes*2,color->data,color->bytes*2);temp_buf_free(copy);temp_buf_free(gray);temp_buf_free(color);}
printf("legacy-byte-roundtrips=256 packed-and-subwindow-layouts=4 gray-conversion-directions=4\\n");}'''
    results=[]
    with tempfile.TemporaryDirectory(prefix='gimp-format-map-') as directory:
        directory=Path(directory)
        for name,source,compiler,std,packages in [('native',code,'cc','c11',['gobject-2.0','babl-0.1']),('legacy',old,'c++','c++14',['glib-2.0'])]:
            path=directory/(name+('.cpp' if compiler=='c++' else '.c'));path.write_text(source);output=directory/name
            command=shlex.split(os.environ.get('CXX' if compiler=='c++' else 'CC',compiler))+['-std='+std,'-Wall','-Wextra','-Werror','-Wno-implicit-fallthrough','-g']
            if sanitize:command+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
            command+=[str(path),'-o',str(output)]+shlex.split(run(['pkg-config','--cflags','--libs']+packages))
            run(command)
            env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1'
            results.append(run([str(output)],env=env).strip())
    return results

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--sanitize',action='store_true');parser.add_argument('--report',type=Path);args=parser.parse_args()
    m=json.loads(MANIFEST.read_text());legacy=validate(m)
    outputs=executable(m,legacy,args.sanitize)
    report={'status':'PASS','task':'09.001','duties':13,'outputs':outputs,'sanitizers':['address','undefined'] if args.sanitize else [],'limits':'Extracted source and format mapping only; no whole-GIMP, conversion parity, threading, LSan or platform claim','source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [MANIFEST,Path(__file__),ROOT/'app/gegl/gimp-babl.c',ROOT/'libgimpbase/gimpbaseenums.h',ROOT/'app/core/core-enums.h',ROOT/'app/core/gimpimage-colormap.c',ROOT/'tools/tests/test_pixel_format_mapping.py',ROOT/'migration/contracts/pixel-format-mapping.md']}}
    if args.report:args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
