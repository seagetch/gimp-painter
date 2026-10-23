/* SPDX-License-Identifier: GPL-3.0-or-later
 * Test-only GEGL adapter: NOT the production registered layer-mode operator. */
#include <gegl-plugin.h>
#include <math.h>
#include <stdio.h>
#include "gimppainterfloatkernel.h"
typedef struct { GeglOperationPointComposer3 parent; int mode; float opacity; } FloatProbe;
typedef struct { GeglOperationPointComposer3Class parent; } FloatProbeClass;
G_DEFINE_TYPE(FloatProbe,float_probe,GEGL_TYPE_OPERATION_POINT_COMPOSER3)
static void prepare(GeglOperation*op){
 gegl_operation_set_format(op,"input",babl_format("R'G'B'A float"));
 gegl_operation_set_format(op,"aux",babl_format("R'G'B'A float"));
 gegl_operation_set_format(op,"aux2",babl_format("Y float"));
 gegl_operation_set_format(op,"output",babl_format("R'G'B'A float"));
}
static gboolean process(GeglOperation*op,void*in,void*aux,void*mask,void*out,glong n,const GeglRectangle*r,gint level){
 FloatProbe*p=(FloatProbe*)op; (void)r;(void)level;
 for(glong i=0;i<n;i++)if(!gimp_painter_float_composite((float*)in+4*i,(float*)aux+4*i,(float*)out+4*i,p->opacity,mask?((float*)mask)[i]:1,p->mode))return FALSE;
 return TRUE;
}
static void float_probe_init(FloatProbe*p){p->opacity=1;}
static void float_probe_class_init(FloatProbeClass*k){
 GeglOperationClass*op=GEGL_OPERATION_CLASS(k);op->prepare=prepare;
 GEGL_OPERATION_POINT_COMPOSER3_CLASS(k)->process=process;
 gegl_operation_class_set_keys(op,"name","painter:test-float-probe",NULL);
}
int main(int argc,char**argv){
 const char*path=argv[1];FILE*f=fopen(path,"rb");int mode;float opacity,a[256],b[256],m[64],expected[256],actual[256];unsigned count=0;
 GeglRectangle r={-3,-2,8,8};const Babl*fmt;
 gegl_init(&argc,&argv);g_type_class_ref(float_probe_get_type());fmt=babl_format("R'G'B'A float");
 while(fread(&mode,4,1,f)==1){
  if(fread(&opacity,4,1,f)!=1||fread(a,sizeof a,1,f)!=1||fread(b,sizeof b,1,f)!=1||fread(m,sizeof m,1,f)!=1||fread(expected,sizeof expected,1,f)!=1)return 2;
  GeglBuffer*ba=gegl_buffer_new(&r,fmt),*bb=gegl_buffer_new(&r,fmt),*bm=gegl_buffer_new(&r,babl_format("Y float"));
  gegl_buffer_set(ba,&r,0,fmt,a,GEGL_AUTO_ROWSTRIDE);gegl_buffer_set(bb,&r,0,fmt,b,GEGL_AUTO_ROWSTRIDE);gegl_buffer_set(bm,&r,0,babl_format("Y float"),m,GEGL_AUTO_ROWSTRIDE);
  GeglNode*root=gegl_node_new(),*na=gegl_node_new_child(root,"operation","gegl:buffer-source","buffer",ba,NULL),*nb=gegl_node_new_child(root,"operation","gegl:buffer-source","buffer",bb,NULL),*nm=gegl_node_new_child(root,"operation","gegl:buffer-source","buffer",bm,NULL),*blend=gegl_node_new_child(root,"operation","painter:test-float-probe",NULL);
  FloatProbe*p=(FloatProbe*)gegl_node_get_gegl_operation(blend);p->mode=mode;p->opacity=opacity;
  gegl_node_connect_to(na,"output",blend,"input");gegl_node_connect_to(nb,"output",blend,"aux");gegl_node_connect_to(nm,"output",blend,"aux2");
  gegl_node_blit(blend,1,&r,fmt,actual,GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_DEFAULT);
  for(int i=0;i<256;i++)if(!isfinite(actual[i])||fabs((double)actual[i]-expected[i])>2e-6*fmax(1,fabs(expected[i]))){fprintf(stderr,"graph %u mode%d component%d got%g expected%g\n",count,mode,i,actual[i],expected[i]);return 3;}
  g_object_unref(root);g_object_unref(ba);g_object_unref(bb);g_object_unref(bm);count++;
 }
 fclose(f);gegl_exit();printf("%u test-adapter GEGL graphs x64 RGBA pixels passed\n",count);return 0;
}
