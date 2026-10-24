/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <cstring>
#include <memory>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpbrush.h"
#include "core/gimpbrush-private.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimppattern.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpcontext.h"
#include "core/gimpdata.h"
#include "core/gimptempbuf.h"
#include "paint/gimppaintoptions.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/gimp-resources.hpp"
using namespace GimpPainter::MyPaint;
static Gimp*gimp;
static const char*shape="late-proof-shape-新";
static const char*paper="late-proof-paper-紙";
static GimpBrush*brush(const char*name){return GIMP_BRUSH(gimp_brush_generated_new(name,GIMP_BRUSH_GENERATED_CIRCLE,5,2,.7,1,0));}
static GimpPattern*pattern(const char*name){auto*p=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name",name,nullptr));p->mask=gimp_temp_buf_new(2,2,babl_format("R'G'B' u8"));std::memset(gimp_temp_buf_get_data(p->mask),120,12);return p;}
static void released(gpointer data,GObject*){*static_cast<bool*>(data)=true;}
static Resource request(){Resource r;r.set_switch(BRUSH_USE_GIMP_BRUSHMARK,true);r.set_switch(BRUSH_BRUSHMARK_SPECIFIED,true);r.set_switch(BRUSH_USE_GIMP_TEXTURE,true);r.set_switch(BRUSH_TEXTURE_SPECIFIED,true);r.set_text(BRUSH_BRUSHMARK_NAME,shape);r.set_text(BRUSH_TEXTURE_NAME,paper);return r;}
static Resource reopen(const Resource&r){GError*e=nullptr;auto*out=g_memory_output_stream_new_resizable();g_assert_true(r.save(out,nullptr,&e));g_assert_no_error(e);auto*in=g_memory_input_stream_new_from_data(g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM(out)),g_memory_output_stream_get_data_size(G_MEMORY_OUTPUT_STREAM(out)),nullptr);auto copy=Resource::load(in);g_object_unref(in);g_object_unref(out);return copy;}
static void check(const GimpResources&o,bool missing,const char*b,const char*p){auto x=o.resolution();g_assert_cmpint(x.brush_missing,==,missing);g_assert_cmpint(x.texture_missing,==,missing);g_assert_true(x.requested_brush==shape);g_assert_true(x.requested_texture==paper);g_assert_true(x.effective_brush==b);g_assert_true(x.effective_texture==p);}
static void late_resolution(){
  auto*bc=gimp_data_factory_get_container(gimp->brush_factory);auto*pc=gimp_data_factory_get_container(gimp->pattern_factory);auto*mc=gimp_data_factory_get_container(gimp->mybrush_factory);
  const int bn=gimp_container_get_n_children(bc),pn=gimp_container_get_n_children(pc),mn=gimp_container_get_n_children(mc);
  auto*global=gimp_get_user_context(gimp);auto*global_b=gimp_context_get_brush(global);auto*global_p=gimp_context_get_pattern(global);
  auto*options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));auto*context=GIMP_CONTEXT(options);
  auto*a=brush("late-proof-fallback-shape");auto*ap=pattern("late-proof-fallback-paper");gimp_context_set_brush(context,a);gimp_context_set_pattern(context,ap);
  const auto original=request();auto r=reopen(original);const auto saved=r.encode();
  auto first=std::unique_ptr<GimpResources>(new GimpResources(context,r));check(*first,true,"late-proof-fallback-shape","late-proof-fallback-paper");g_assert_cmpint(a->priv->use_count,==,1);
  auto*b=brush(shape);auto*bp=pattern(paper);gimp_data_make_internal(GIMP_DATA(b),"late-proof-original-shape");gimp_data_make_internal(GIMP_DATA(bp),"late-proof-original-paper");gimp_container_add(bc,GIMP_OBJECT(b));gimp_container_add(pc,GIMP_OBJECT(bp));
  check(*first,true,"late-proof-fallback-shape","late-proof-fallback-paper");g_assert_cmpint(b->priv->use_count,==,0);
  auto second=std::unique_ptr<GimpResources>(new GimpResources(context,r));check(*second,false,shape,paper);g_assert_cmpint(b->priv->use_count,==,1);g_assert_cmpint(a->priv->use_count,==,1);
  // Explicit context reselection is allowed, but it does not rewrite immutable Resource.
  gimp_context_set_brush(context,b);gimp_context_set_pattern(context,bp);{GimpResources reselected(context,r);check(reselected,false,shape,paper);g_assert_cmpint(b->priv->use_count,==,2);}gimp_context_set_brush(context,a);gimp_context_set_pattern(context,ap);
  gimp_object_set_name(GIMP_OBJECT(b),"late-proof-renamed-shape");gimp_object_set_name(GIMP_OBJECT(bp),"late-proof-renamed-paper");
  auto third=std::unique_ptr<GimpResources>(new GimpResources(context,r));check(*third,true,"late-proof-fallback-shape","late-proof-fallback-paper");g_assert_cmpint(b->priv->use_count,==,1);
  auto*c=brush(shape);auto*cp=pattern(paper);gimp_data_make_internal(GIMP_DATA(c),"late-proof-replacement-shape");gimp_data_make_internal(GIMP_DATA(cp),"late-proof-replacement-paper");gimp_container_add(bc,GIMP_OBJECT(c));gimp_container_add(pc,GIMP_OBJECT(cp));
  auto fourth=std::unique_ptr<GimpResources>(new GimpResources(context,r));check(*fourth,false,shape,paper);g_assert_cmpint(c->priv->use_count,==,1);g_assert_cmpint(b->priv->use_count,==,1);
  bool bdead=false,bpdead=false,cdead=false,cpdead=false;g_object_weak_ref(G_OBJECT(b),released,&bdead);g_object_weak_ref(G_OBJECT(bp),released,&bpdead);g_object_weak_ref(G_OBJECT(c),released,&cdead);g_object_weak_ref(G_OBJECT(cp),released,&cpdead);
  gimp_container_remove(bc,GIMP_OBJECT(b));gimp_container_remove(pc,GIMP_OBJECT(bp));gimp_container_remove(bc,GIMP_OBJECT(c));gimp_container_remove(pc,GIMP_OBJECT(cp));g_object_unref(b);g_object_unref(bp);g_object_unref(c);g_object_unref(cp);
  g_assert_false(bdead);g_assert_false(bpdead);g_assert_false(cdead);g_assert_false(cpdead);
  auto fifth=std::unique_ptr<GimpResources>(new GimpResources(context,r));check(*fifth,true,"late-proof-fallback-shape","late-proof-fallback-paper");
  second.reset();g_assert_true(bdead);g_assert_true(bpdead);g_assert_false(cdead);fourth.reset();g_assert_true(cdead);g_assert_true(cpdead);
  first.reset();third.reset();fifth.reset();g_assert_cmpint(a->priv->use_count,==,0);
  g_assert_true(r.encode()==saved);auto final=reopen(r);g_assert_true(final.text_value(BRUSH_BRUSHMARK_NAME)==shape);g_assert_true(final.text_value(BRUSH_TEXTURE_NAME)==paper);
  g_object_unref(options);g_object_unref(a);g_object_unref(ap);
  g_assert_cmpint(gimp_container_get_n_children(bc),==,bn);g_assert_cmpint(gimp_container_get_n_children(pc),==,pn);g_assert_cmpint(gimp_container_get_n_children(mc),==,mn);g_assert_true(gimp_context_get_brush(global)==global_b);g_assert_true(gimp_context_get_pattern(global)==global_p);
}
static void flags_policy(){
  auto*options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));auto*ctx=GIMP_CONTEXT(options);auto*b=brush("policy-shape");auto*p=pattern("policy-paper");gimp_context_set_brush(ctx,b);gimp_context_set_pattern(ctx,p);
  for(int mode=0;mode<4;++mode){auto r=request();
    if(mode==0){r.set_switch(BRUSH_USE_GIMP_BRUSHMARK,false);r.set_switch(BRUSH_USE_GIMP_TEXTURE,false);}
    if(mode==1){r.set_switch(BRUSH_BRUSHMARK_SPECIFIED,false);r.set_switch(BRUSH_TEXTURE_SPECIFIED,false);}
    if(mode>=2){r.set_text(BRUSH_BRUSHMARK_NAME,mode==2?"":nullptr);r.set_text(BRUSH_TEXTURE_NAME,mode==2?"":nullptr);}
    {GimpResources o(ctx,r);auto x=o.resolution();g_assert_false(x.brush_missing);g_assert_false(x.texture_missing);g_assert_true(x.effective_brush==(mode==0?"":"policy-shape"));g_assert_true(x.effective_texture==(mode==0?"":"policy-paper"));g_assert_cmpint(b->priv->use_count,==,mode==0?0:1);}
    g_assert_cmpint(b->priv->use_count,==,0);
  }g_object_unref(options);g_object_unref(b);g_object_unref(p);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();g_test_add_func("/resource-late-resolution/lifecycle",late_resolution);g_test_add_func("/resource-late-resolution/flags",flags_policy);return g_test_run();}
