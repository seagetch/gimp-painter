from pathlib import Path
import subprocess,json,hashlib,re
r=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');p=r/'migration/tests/color-exchange-wire-foundation';p.mkdir(exist_ok=True);ref='afa43fae3e920210146abed514f136fd49f671b5'
def old(path):return subprocess.check_output(['git','show',ref+':'+path],cwd=r,text=True)
def function(text,name):
 m=re.search(r'^'+name+r'\s*\(',text,re.M);assert m;end=text.index('\n}',m.start())+2;return text[m.start():end]
s=old('app/xcf/xcf-save.c');w=old('app/xcf/xcf-write.c');v=old('app/core/gimpparamspecs.c')
parts={n:function(t,n)for t,n in [(s,'xcf_save_filter_spec_value'),(w,'xcf_write_int32'),(w,'xcf_write_string'),(v,'gimp_int8_get_type'),(v,'gimp_int32_get_type'),(v,'gimp_image_id_get_type'),(v,'gimp_drawable_id_get_type')]}
macros=s[s.index('#define xcf_write_int32_check_error'):s.index('#define xcf_write_prop_type_check_error')]
boiler=r'''#include <glib-object.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
/* Successful bounded host I/O and unused-kind adapters, not an old application. */
typedef struct {FILE *fp;guint32 cp;} XcfInfo;
typedef void GimpImage;
typedef enum {XCF_FILTER_END=0,XCF_FILTER_UNSUPPORTED=1,XCF_FILTER_INT32,XCF_FILTER_INT16,XCF_FILTER_INT8,XCF_FILTER_FLOAT,XCF_FILTER_INT8ARRAY,XCF_FILTER_FLOATARRAY,XCF_FILTER_STRING,XCF_FILTER_RGB,XCF_FILTER_DRAWABLE} XcfFilterArgType;
#define GIMP_VALUE_HOLDS_INT32(v) G_TYPE_CHECK_VALUE_TYPE((v),gimp_int32_get_type())
#define GIMP_VALUE_HOLDS_INT8(v) G_TYPE_CHECK_VALUE_TYPE((v),gimp_int8_get_type())
#define GIMP_VALUE_HOLDS_INT16(v) FALSE
#define GIMP_VALUE_HOLDS_INT8_ARRAY(v) FALSE
#define GIMP_VALUE_HOLDS_FLOAT_ARRAY(v) FALSE
#define GIMP_VALUE_HOLDS_DRAWABLE_ID(v) G_TYPE_CHECK_VALUE_TYPE((v),gimp_drawable_id_get_type())
#define g_print(...) ((void)0)
static guint xcf_write_int8(FILE *fp,const guint8 *d,gint n,GError **e){(void)e;assert(fwrite(d,1,n,fp)==(size_t)n);return n;}
static guint xcf_write_float(FILE *fp,const gfloat *d,gint n,GError **e){(void)fp;(void)d;(void)n;(void)e;g_assert_not_reached();return 0;}
'''
main=r'''
static guint criticals=0;
static void critical(const gchar *domain,GLogLevelFlags flags,const gchar *message,gpointer data){
 (void)flags;(void)data;assert(!strcmp(domain,"GLib-GObject"));
 assert(strstr(message,"g_value_get_int")&&strstr(message,"G_VALUE_HOLDS_INT"));criticals++;
}
int main(void){
 g_log_set_always_fatal(G_LOG_FATAL_MASK);
 guint handler=g_log_set_handler("GLib-GObject",G_LOG_LEVEL_CRITICAL,critical,NULL);
 assert(g_type_parent(gimp_int8_get_type())==G_TYPE_UINT);
 for(guint k=0;k<256;k++){
  FILE *f=tmpfile();assert(f);XcfInfo info={f,0};GError *error=NULL;gchar *name="plug-in-exchange";
  info.cp+=xcf_write_string(f,&name,1,&error);assert(!error);
  GValue args[12]={G_VALUE_INIT};g_value_init(args,gimp_int32_get_type());g_value_set_int(args,-123);
  g_value_init(args+1,gimp_image_id_get_type());g_value_init(args+2,gimp_drawable_id_get_type());
  for(guint i=3;i<12;i++){g_value_init(args+i,gimp_int8_get_type());g_value_set_uint(args+i,(k+(i-3)*29)&255);assert(g_value_get_uint(args+i)==((k+(i-3)*29)&255));}
  for(guint i=0;i<12;i++){assert(xcf_save_filter_spec_value(&info,NULL,&error,args+i));g_value_unset(args+i);}
  guint32 end[2]={0,0};info.cp+=xcf_write_int32(f,end,2,&error);assert(!error&&info.cp==138);
  rewind(f);guint8 bytes[138];assert(fread(bytes,1,138,f)==138);fclose(f);
  printf("WIRE %u ",k);for(guint i=0;i<138;i++)printf("%02x",bytes[i]);puts("");
  assert(criticals==(k+1)*9);
 }
 g_log_remove_handler("GLib-GObject",handler);
 printf("HOST GLib %u.%u.%u INT8 UINT-derived, signed getter criticals=%u;256 frames,2304 channel values\n",glib_major_version,glib_minor_version,glib_micro_version,criticals);
 return 0;
}
'''
code=boiler+''.join('\nstatic GType\n'+parts[n]for n in ['gimp_int8_get_type','gimp_int32_get_type','gimp_image_id_get_type','gimp_drawable_id_get_type'])+'\nstatic guint\n'+parts['xcf_write_int32']+'\nstatic guint\n'+parts['xcf_write_string']+'\n'+macros+'\nstatic gboolean\n'+parts['xcf_save_filter_spec_value']+'\n'+main
(p/'writer-oracle.c').write_text(code)
(p/'reference.json').write_text(json.dumps({'old_ref':ref,'sources':{path:{'blob':subprocess.check_output(['git','rev-parse',ref+':'+path],cwd=r,text=True).strip(),'sha256':hashlib.sha256(old(path).encode()).hexdigest()}for path in ['app/xcf/xcf-save.c','app/xcf/xcf-write.c','app/xcf/xcf-private.h','app/core/gimpparamspecs.c','app/core/gimpparamspecs.h','app/pdb/gimp-pdb-compat.c']},'exact_functions':parts,'scope':'Literal old emitter/BE/string/GimpInt8 type functions on current GLib host; successful byte IO stub, exact original INT8/INT32/ImageID/DrawableID GTypes, unused types disabled and debug print suppressed. Not old application capture or universal GLib-build behavior.'},indent=2)+'\n')
