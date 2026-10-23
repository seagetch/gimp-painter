/* SPDX-License-Identifier: GPL-3.0-or-later
 * Independent placement oracle: explicit Babl boundary conversions, not the
 * production NativePixel helpers. Historical nonlinear-byte Gray is deliberate.
 */
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
#include <gegl.h>
#include <lcms2.h>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
using namespace GimpPainter::MyPaint;
static const Babl *profile_space(bool gray)
{
  cmsToneCurve *curve=cmsBuildGamma(nullptr,gray?1.8:2.2);
  cmsToneCurve *curves[]={curve,curve,curve};
  cmsCIExyY white;cmsWhitePointFromTemp(&white,6504);
  cmsCIExyYTRIPLE primaries={{.64,.33,1},{.21,.71,1},{.15,.06,1}};
  auto profile=gray?cmsCreateGrayProfile(cmsD50_xyY(),curve):cmsCreateRGBProfile(&white,&primaries,curves);
  cmsUInt32Number size=0;cmsSaveProfileToMem(profile,nullptr,&size);
  std::vector<char> data(size);cmsSaveProfileToMem(profile,data.data(),&size);
  const char *error=nullptr;auto *space=babl_space_from_icc(data.data(),size,BABL_ICC_INTENT_RELATIVE_COLORIMETRIC,&error);
  g_assert_null(error);g_assert_nonnull(space);cmsCloseProfile(profile);cmsFreeToneCurve(curve);return space;
}
static void shape(GeglSurface &s){s.set_shape_provider([](float,float,float,float){return ShapeMask{3,1,{255,0,255}};});}
static void read(GeglBuffer *b,const Babl *f,double *out){gegl_buffer_get(b,gegl_buffer_get_extent(b),1,f,out,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);}
static void placement()
{
  unsigned cells=0;
  for(bool gray:{false,true})for(bool custom:{false,true})for(const char *trc:{"linear","nonlinear","perceptual"})for(bool floating:{false,true}){
    const auto *space=custom?profile_space(gray):babl_space("sRGB");
    const bool linear=strcmp(trc,"linear")==0,perceptual=strcmp(trc,"perceptual")==0;
    std::string model=gray?(linear?"Y":perceptual?"Y~":"Y'"):(linear?"RGB":perceptual?"R~G~B~":"R'G'B'");
    const auto *native=babl_format_with_space((model+" double").c_str(),space);
    const auto *eval=babl_format_with_space("R'G'B'A double",space);
    const int n=gray?1:3;
    auto *buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),native);
    double initial[9];for(int i=0;i<3*n;++i)initial[i]=.17+.03*(i%n);
    gegl_buffer_set(buffer,gegl_buffer_get_extent(buffer),0,native,initial,GEGL_AUTO_ROWSTRIDE);
    // draw_dab is a float evaluator boundary; preserve that exact value.
    double input[]={double(.8f),double(.2f),double(.4f),1},converted[4]={};
    babl_process(babl_fish(eval,native),input,converted,1);
    if(!gray&&custom&&linear)for(int c=0;c<3;++c)
      g_assert_cmpfloat_with_epsilon(converted[c],std::pow(input[c],2.2),5e-6);
    GeglSurface surface(buffer);shape(surface);surface.set_non_incremental(floating);surface.set_stroke_opacity(1);
    surface.begin_session();surface.draw_dab(2,1,2,.8f,.2f,.4f,.25f,1);
    double actual[9];read(buffer,native,actual);
    for(int i:{0,2})for(int c=0;c<n;++c)
      g_assert_cmpfloat_with_epsilon(actual[i*n+c],initial[i*n+c]*.75+converted[c]*.25,2e-14);
    for(int c=0;c<n;++c)g_assert_cmpfloat(actual[n+c],==,initial[n+c]);
    surface.cancel_session();read(buffer,native,actual);g_assert_cmpmem(actual,3*n*sizeof(double),initial,3*n*sizeof(double));
    // Incremental eraser background converts once. Floating eraser removes
    // only this session paint, intentionally revealing the unchanged snapshot.
    surface.set_background(input[0],input[1],input[2]);surface.begin_session();surface.draw_dab(2,1,2,0,0,0,.25f,1,0);
    read(buffer,native,actual);for(int i:{0,2})for(int c=0;c<n;++c)
      g_assert_cmpfloat_with_epsilon(actual[i*n+c],floating?initial[i*n+c]:initial[i*n+c]*.75+converted[c]*.25,2e-14);
    surface.cancel_session();g_object_unref(buffer);++cells;
  }
  g_assert_cmpuint(cells,==,24);g_test_message("24 exact placement cells: native composition and background, builtin/custom RGB/Gray, all TRCs, incremental/floating");
}
static void sampling()
{
  unsigned cells=0,noncommuting=0;
  for(bool gray:{false,true})for(bool custom:{false,true})for(const char *model0:{"linear","nonlinear","perceptual"}){
    const auto *space=custom?profile_space(gray):babl_space("sRGB");
    const bool linear=strcmp(model0,"linear")==0,perceptual=strcmp(model0,"perceptual")==0;
    std::string model=gray?(linear?"YA":perceptual?"Y~A":"Y'A"):(linear?"RGBA":perceptual?"R~G~B~A":"R'G'B'A");
    auto *native=babl_format_with_space((model+" double").c_str(),space);auto *eval=babl_format_with_space("R'G'B'A double",space);
    const int n=gray?2:4;double pixels[12]={};
    for(int i=0;i<3;++i){for(int c=0;c<n-1;++c)pixels[i*n+c]=i==0?.04+.01*c:i==1?.9:.64-.05*c;pixels[i*n+n-1]=i==0?.5:1;}
    auto *b=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),native);gegl_buffer_set(b,gegl_buffer_get_extent(b),0,native,pixels,GEGL_AUTO_ROWSTRIDE);
    double mean[4]={},expected[4]={};for(int c=0;c<n-1;++c)mean[c]=(pixels[c]*.5+pixels[2*n+c])/1.5;mean[n-1]=.75;
    babl_process(babl_fish(native,eval),mean,expected,1);
    double individually[12]={};babl_process(babl_fish(native,eval),pixels,individually,3);
    if(std::abs(expected[0]-(individually[0]*.5+individually[8])/1.5)>.01)++noncommuting;
    GeglSurface surface(b);shape(surface);float r,g,blue,a;surface.get_color(2,1,2,&r,&g,&blue,&a,1);
    const float actual[]={r,g,blue,a};for(int c=0;c<4;++c)g_assert_cmpfloat_with_epsilon(actual[c],expected[c],2e-7);
    g_object_unref(b);++cells;
  }
  g_assert_cmpuint(cells,==,12);g_assert_cmpuint(noncommuting,>=,4);
  g_test_message("12 native alpha-weighted sampling cells; %u distinguish convert-after-average from premature conversion",noncommuting);
}
static void byte_gray_policy()
{
  unsigned cells=0;
  for(bool custom:{false,true})for(bool alpha:{false,true}){
    auto *space=custom?profile_space(true):babl_space("sRGB");auto *f=babl_format_with_space(alpha?"Y'A u8":"Y' u8",space);
    auto *b=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),f);GeglSurface s(b);shape(s);s.begin_session();s.draw_dab(2,1,2,.8f,.2f,.4f,1,1);
    guchar bytes[6]={};gegl_buffer_get(b,gegl_buffer_get_extent(b),1,f,bytes,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpuint(bytes[0],==,204);if(alpha)g_assert_cmpuint(bytes[1],==,255);
    float r,g,blue,a;s.get_color(2,1,2,&r,&g,&blue,&a,1);g_assert_cmpfloat_with_epsilon(r,204./255,1e-7);g_assert_cmpfloat(r,==,g);g_assert_cmpfloat(r,==,blue);
    s.cancel_session();g_object_unref(b);++cells;
  }
  g_assert_cmpuint(cells,==,4);g_test_message("4 historical byte-Gray red-component/replicated-sample policies preserved; no luminance reinterpretation");
}
int main(int argc,char **argv){g_test_init(&argc,&argv,nullptr);gegl_init(&argc,&argv);g_test_add_func("/color-placement/composition-background",placement);g_test_add_func("/color-placement/sample-after-native-average",sampling);g_test_add_func("/color-placement/intentional-byte-gray",byte_gray_policy);int status=g_test_run();gegl_exit();return status;}
