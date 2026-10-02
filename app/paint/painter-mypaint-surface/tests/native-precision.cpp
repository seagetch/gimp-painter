/* SPDX-License-Identifier: GPL-3.0-or-later
 * Defined extension expectations; the historical application is byte-only.
 */
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
#include "paint/painter-mypaint-surface/native-pixels.hpp"
#include <gegl.h>
#include <lcms2.h>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace GimpPainter::MyPaint;
template<class T>T scalar(const guchar*p){T v;std::memcpy(&v,p,sizeof v);return v;}
template<class T>void scalar(guchar*p,T v){std::memcpy(p,&v,sizeof v);}
static std::vector<guchar> get(GeglBuffer*b){auto*r=gegl_buffer_get_extent(b);const auto*f=gegl_buffer_get_format(b);std::vector<guchar>p(std::size_t(r->width)*r->height*babl_format_get_bytes_per_pixel(f));gegl_buffer_get(b,r,1,f,p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return p;}
static void set(GeglBuffer*b,const std::vector<guchar>&p){gegl_buffer_set(b,gegl_buffer_get_extent(b),0,gegl_buffer_get_format(b),p.data(),GEGL_AUTO_ROWSTRIDE);}
static void shape(GeglSurface&s){s.set_shape_provider([](float,float,float,float){return ShapeMask{3,1,{255,0,255}};});}
static void dab(GeglSurface&s,float opaque=.5,float alpha=1,float lock=0){s.draw_dab(2,1,2,0,0,0,opaque,1,alpha,1,0,lock);}
static double component(const guchar*p,const std::string&type){if(type=="u8")return *p/255.;if(type=="u16")return scalar<std::uint16_t>(p)/65535.;if(type=="u32")return scalar<std::uint32_t>(p)/4294967295.;if(type=="float")return scalar<float>(p);if(type=="double")return scalar<double>(p);const auto h=scalar<std::uint16_t>(p);const unsigned e=(h>>10)&31,m=h&1023;return std::ldexp(e?1024.+m:double(m),e?int(e)-25:-24)*(h&32768?-1:1);}
static void initialize(guchar*p,const std::string&type,double value){if(type=="u8")scalar(p,std::uint8_t(std::floor(value*255+.5)));else if(type=="u16")scalar(p,std::uint16_t(std::floor(value*65535+.5)));else if(type=="u32")scalar(p,std::uint32_t(std::floor(value*4294967295.+.5)));else if(type=="float")scalar(p,float(value));else if(type=="double")scalar(p,value);else scalar(p,std::uint16_t(value==1?0x3c00:value==.5?0x3800:0x3400));}
static void format_matrix()
{
 unsigned count=0;
 for(const char*t:{"u8","u16","u32","half","float","double"})for(const char*m:{"RGB","RGBA","R'G'B'","R'G'B'A","R~G~B~","R~G~B~A","Y","YA","Y'","Y'A","Y~","Y~A"}){
  const std::string type=t,model=m,name=model+" "+t;auto*f=babl_format(name.c_str());const int n=babl_format_get_n_components(f),bytes=babl_format_get_bytes_per_pixel(f),size=bytes/n;const bool alpha=model.back()=='A',legacy=type=="u8"&&model.find('\'')!=std::string::npos;
  auto*b=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),f);std::vector<guchar>before(3*bytes);
  for(int i=0;i<3;++i)for(int c=0;c<n;++c)initialize(before.data()+i*bytes+c*size,type,alpha&&c==n-1?1:.5);
  set(b,before);GeglSurface s(b);shape(s);s.begin_session();dab(s,0);g_assert_true(get(b)==before);dab(s);auto after=get(b);g_assert_cmpmem(after.data()+bytes,bytes,before.data()+bytes,bytes);
  if(!legacy){const double old=component(before.data(),type),actual=component(after.data(),type);const double epsilon=type=="u8"?1./255:type=="u16"?1./65535:type=="u32"?1./4294967295.:type=="half"?1e-4:type=="float"?1e-7:1e-15;g_assert_cmpfloat_with_epsilon(actual,old*.5,epsilon);if(alpha)g_assert_cmpmem(after.data()+(n-1)*size,size,before.data()+(n-1)*size,size);}
  s.cancel_session();g_assert_true(get(b)==before);
  if(!legacy){
   const double epsilon=type=="u8"?2./255:type=="u16"?2./65535:type=="u32"?2./4294967295.:type=="half"?5e-4:type=="float"?2e-7:2e-15;
   // Gray background ingress is an ICC/Babl color transform, independently
   // bounded at float color accuracy; native scalar/alpha/LSB tests stay exact.
   s.begin_session();dab(s,.5,.25);after=get(b);const double expected=alpha?component(before.data(),type)*.8:component(before.data(),type)*.5+.375;g_assert_cmpfloat_with_epsilon(component(after.data(),type),expected,!alpha&&model[0]=='Y'?std::max(epsilon,3e-7):epsilon);if(alpha)g_assert_cmpfloat_with_epsilon(component(after.data()+(n-1)*size,type),.625,epsilon);s.cancel_session();g_assert_true(get(b)==before);
   s.begin_session();dab(s,.5,1,1);after=get(b);g_assert_cmpfloat_with_epsilon(component(after.data(),type),component(before.data(),type)*.5,epsilon);if(alpha)g_assert_cmpmem(after.data()+(n-1)*size,size,before.data()+(n-1)*size,size);s.cancel_session();g_assert_true(get(b)==before);
   s.set_non_incremental(true);s.set_stroke_opacity(.5);s.begin_session();dab(s);dab(s);after=get(b);g_assert_cmpfloat_with_epsilon(component(after.data(),type),component(before.data(),type)*.625,epsilon);dab(s,1,0);g_assert_true(get(b)==before);s.cancel_session();
  }
  g_assert_true(gegl_buffer_get_format(b)==f);g_test_message("format=%s model=%s bytes=%d space=%s",name.c_str(),babl_get_name(babl_format_get_model(f)),bytes,babl_get_name(babl_format_get_space(f)));g_object_unref(b);++count;
 }
 g_assert_cmpuint(count,==,72);
}
static void quantization_and_native_lsb()
{
 for(const char*model:{"RGB","RGBA","R'G'B'","R'G'B'A","R~G~B~","R~G~B~A","Y","YA","Y'","Y'A","Y~","Y~A"}){
  const std::string m=model;const bool alpha=m.back()=='A';
  for(const char*t:{"u16","u32","double"}){
   const std::string type=t;auto*f=babl_format((m+" "+t).c_str());const int n=babl_format_get_n_components(f),bytes=babl_format_get_bytes_per_pixel(f),size=bytes/n;
   auto*b=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),f);std::vector<guchar>before(3*bytes);
   for(int i=0;i<3;++i)for(int c=0;c<n;++c){auto*p=before.data()+i*bytes+c*size;if(alpha&&c==n-1)initialize(p,type,1);else if(type=="u32")scalar(p,std::uint32_t(0x80000000u+i*4+c));else if(type=="u16")scalar(p,std::uint16_t(0x8000u+i*4+c));else scalar(p,std::nextafter(.5+i*1e-10,1.));}
   set(b,before);GeglSurface s(b);shape(s);s.begin_session();dab(s);s.end_session();auto after=get(b);
   for(int i:{0,2})for(int c=0;c<n-int(alpha);++c){auto*p=before.data()+i*bytes+c*size;auto*q=after.data()+i*bytes+c*size;if(type=="u32")g_assert_cmpuint(scalar<std::uint32_t>(q),==,(std::uint64_t(scalar<std::uint32_t>(p))+1)/2);else if(type=="u16")g_assert_cmpuint(scalar<std::uint16_t>(q),==,(unsigned(scalar<std::uint16_t>(p))+1)/2);else g_assert_cmpfloat(scalar<double>(q),==,scalar<double>(p)*.5);}
   g_object_unref(b);
  }
 }
}
static void half_boundaries()
{
 using namespace GimpPainter::MyPaint::NativePixel;
 for(unsigned bits=0;bits<65536;++bits){if((bits&0x7c00)==0x7c00)continue;g_assert_cmpuint(half_encode(half_decode(bits)),==,bits);}
 g_assert_cmpuint(half_encode(.5+std::ldexp(1.,-12)),==,0x3800);
 g_assert_cmpuint(half_encode(std::nextafter(.5+std::ldexp(1.,-12),1.)),==,0x3801);
 g_assert_cmpuint(half_encode(std::ldexp(1.,-25)),==,0);
 g_assert_cmpuint(half_encode(std::nextafter(std::ldexp(1.,-25),1.)),==,1);
 for(double value:{-.1,.1,.5,1e300})for(double t:{.1,.33,.9})g_assert_cmpfloat(mix(value,value,t),==,value);
 g_assert_cmpuint(half_encode(1e300),==,0x7bff);g_assert_cmpuint(half_encode(-1e300),==,0xfbff);
}
static void alpha_hdr_selection_nonincremental()
{
 for(bool floating:{false,true}){
  auto*f=babl_format("RGBA double");auto*b=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),f);std::vector<guchar>before(3*32);double original[]={-4.,3.,.5000000000000001,.5000000000000001};for(int i=0;i<3;++i)std::memcpy(before.data()+i*32,original,32);set(b,before);GeglSurface s(b);shape(s);s.set_non_incremental(floating);s.set_stroke_opacity(.5);s.begin_session();dab(s,.5,1,1);auto locked=get(b);if(!floating){g_assert_cmpfloat(scalar<double>(locked.data()),==,-2);g_assert_cmpfloat(scalar<double>(locked.data()+8),==,1.5);g_assert_cmpmem(locked.data()+24,8,before.data()+24,8);}else g_assert_true(locked==before);s.cancel_session();g_assert_true(get(b)==before);
  s.begin_session();dab(s,.5,0);auto erased=get(b);if(!floating){g_assert_cmpfloat(scalar<double>(erased.data()),==,-4.);g_assert_cmpfloat(scalar<double>(erased.data()+24),==,original[3]*.5);}else g_assert_true(erased==before);s.cancel_session();
  s.begin_session();dab(s,.5,1);auto painted=get(b);g_assert_true(painted!=before);g_assert_cmpmem(painted.data()+32,32,before.data()+32,32);s.cancel_session();g_assert_true(get(b)==before);g_object_unref(b);
 }
 // Selection precision survives sub-float values and drawable offsets.
 auto*f=babl_format("RGB double");auto*b=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),f);std::vector<guchar>before(3*24);for(int i=0;i<9;++i)scalar(before.data()+i*8,1.);set(b,before);
 auto*mask=gegl_buffer_new(GEGL_RECTANGLE(7,9,3,1),babl_format("Y double"));double values[]={1e-10,0,.5000000000000001};gegl_buffer_set(mask,gegl_buffer_get_extent(mask),0,babl_format("Y double"),values,GEGL_AUTO_ROWSTRIDE);
 GeglSurface s(b);shape(s);s.set_selection(mask,7,9);s.begin_session();dab(s,1);auto after=get(b);g_assert_cmpfloat(scalar<double>(after.data()),==,1.-values[0]);g_assert_cmpfloat(scalar<double>(after.data()+48),==,1.-values[2]);s.cancel_session();g_assert_true(get(b)==before);g_object_unref(mask);g_object_unref(b);
}
static void finite_and_hidden()
{
 auto*f=babl_format("RGBA double");auto*b=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),f);std::vector<guchar>before(96);double values[]={.3,.2,.4,1.};for(int i=0;i<3;++i)std::memcpy(before.data()+i*32,values,32);scalar(before.data()+32,std::numeric_limits<double>::quiet_NaN());set(b,before);
 GeglSurface s(b);shape(s);s.begin_session();dab(s);auto result=get(b);g_assert_cmpmem(result.data()+32,32,before.data()+32,32);s.cancel_session();g_assert_true(get(b)==before);
 scalar(before.data()+64,std::numeric_limits<double>::infinity());set(b,before);s.begin_session();bool rejected=false;try{dab(s);}catch(const std::invalid_argument&){rejected=true;}g_assert_true(rejected);g_assert_true(get(b)==before);s.cancel_session();
 scalar(before.data()+64,.5);scalar(before.data()+24,0.);scalar(before.data(),std::numeric_limits<double>::quiet_NaN());set(b,before);s.begin_session();dab(s,1,1,1);result=get(b);g_assert_cmpmem(result.data(),32,before.data(),32);s.cancel_session();g_object_unref(b);
}
static void background_exception_safety()
{
 // A rejected setter must not poison a live session or its previously valid
 // eraser background. Compare complete native output with an untouched control.
 for(const char*name:{"R'G'B' u8","R'G'B'A u8","Y' u8","Y'A u8","RGB double","Y double"}){
  const auto*f=babl_format(name);NativePixel::Format format(f);
  auto*b=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),f);
  std::vector<guchar>before(3*format.bytes);
  const double initial[4]={.75,.5,.25,1.};
  for(int i=0;i<3;++i)format.write(before.data()+i*format.bytes,initial);
  set(b,before);auto*reference=gegl_buffer_dup(b);
  GeglSurface s(b),control(reference);shape(s);shape(control);
  s.set_background(.25,.5,.75);control.set_background(.25,.5,.75);
  s.begin_session();control.begin_session();
  const double huge=std::numeric_limits<double>::max();
  for(const auto&bad:std::vector<std::vector<double>>{{huge,.2,.3},{.1,huge,.3},{.1,.2,huge}}){
   if(format.legacy){
    s.set_background(bad[0],bad[1],bad[2]);
    control.set_background(NativePixel::unit(bad[0]),NativePixel::unit(bad[1]),NativePixel::unit(bad[2]));
    dab(s,.5,0);dab(control,.5,0);g_assert_true(get(b)==get(reference));
    s.cancel_session();control.cancel_session();s.begin_session();control.begin_session();
    s.set_background(.25,.5,.75);control.set_background(.25,.5,.75);
   }else{
    bool rejected=false;try{s.set_background(bad[0],bad[1],bad[2]);}
    catch(const std::invalid_argument&){rejected=true;}g_assert_true(rejected);
   }
   g_assert_true(get(b)==before);
  }
  if(format.legacy)for(double value:{-.01,std::nextafter(1.,2.)}){
   s.set_background(.1,.2,value);control.set_background(.1,.2,NativePixel::unit(value));
   dab(s,.5,0);dab(control,.5,0);g_assert_true(get(b)==get(reference));
   s.cancel_session();control.cancel_session();s.begin_session();control.begin_session();
   s.set_background(.25,.5,.75);control.set_background(.25,.5,.75);
  }
  for(double value:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
   bool rejected=false;try{s.set_background(.1,.2,value);}
   catch(const std::invalid_argument&){rejected=true;}g_assert_true(rejected);
  }
  dab(s,.5,0);dab(control,.5,0);g_assert_true(get(b)==get(reference));
  s.cancel_session();control.cancel_session();g_assert_true(get(b)==before);
  s.begin_session();control.begin_session();dab(s);dab(control);
  s.end_session();control.end_session();g_assert_true(get(b)==get(reference));
  g_test_message("background recovery format=%s",name);
  g_object_unref(reference);g_object_unref(b);
 }
 // Wide same-domain doubles support finite HDR without writing unused floats.
 auto*b=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),babl_format("R'G'B' double"));
 GeglSurface s(b);shape(s);s.set_background(std::numeric_limits<double>::max(),-2.,.5);
 s.begin_session();dab(s,1,0);auto after=get(b);
 g_assert_cmpfloat(scalar<double>(after.data()),==,std::numeric_limits<double>::max());
 g_assert_cmpfloat(scalar<double>(after.data()+8),==,-2.);
 g_assert_cmpfloat(scalar<double>(after.data()+16),==,.5);
 s.cancel_session();g_object_unref(b);
}
static void shape_paper_sampling()
{
 auto*b=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),babl_format("R'G'B'A double"));double values[]={.2,.4,.6,.5, .8,.8,.8,1, .6,.4,.2,1};gegl_buffer_set(b,gegl_buffer_get_extent(b),0,gegl_buffer_get_format(b),values,GEGL_AUTO_ROWSTRIDE);
 GeglSurface s(b);shape(s);s.set_texture({3,1,{255,64,128}});float r,g,blue,a;s.get_color(2,1,2,&r,&g,&blue,&a,1);const double w=128./255,opacity=(.5+w)/(1+w);g_assert_cmpfloat_with_epsilon(a,opacity,1e-7);g_assert_cmpfloat_with_epsilon(r,(.2*.5+.6*w)/(.5+w),1e-7);g_assert_cmpfloat_with_epsilon(g,.4,1e-7);g_assert_cmpfloat_with_epsilon(blue,(.6*.5+.2*w)/(.5+w),1e-7);g_object_unref(b);
}
static const Babl*space(cmsHPROFILE profile){cmsUInt32Number length=0;cmsSaveProfileToMem(profile,nullptr,&length);std::vector<char>data(length);cmsSaveProfileToMem(profile,data.data(),&length);const char*error=nullptr;auto*s=babl_space_from_icc(data.data(),length,BABL_ICC_INTENT_RELATIVE_COLORIMETRIC,&error);g_assert_null(error);g_assert_nonnull(s);return s;}
static void profile_and_trc()
{
 cmsCIExyY white;cmsWhitePointFromTemp(&white,6504);cmsCIExyYTRIPLE primaries={{.64,.33,1},{.21,.71,1},{.15,.06,1}};auto*curve=cmsBuildGamma(nullptr,2.2);cmsToneCurve*curves[]={curve,curve,curve};auto rgb=cmsCreateRGBProfile(&white,&primaries,curves);auto*graycurve=cmsBuildGamma(nullptr,1.8);auto gray=cmsCreateGrayProfile(cmsD50_xyY(),graycurve);
 for(auto profile:{rgb,gray}){auto*sp=space(profile);const bool isgray=profile==gray;for(const char*trc:{"linear","nonlinear","perceptual"}){
  const bool linear=std::strcmp(trc,"linear")==0,perceptual=std::strcmp(trc,"perceptual")==0;
  const std::string model=isgray?(linear?"YA":perceptual?"Y~A":"Y'A"):(linear?"RGBA":perceptual?"R~G~B~A":"R'G'B'A");auto*f=babl_format_with_space((model+" double").c_str(),sp);auto*b=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),f);GeglSurface s(b);shape(s);s.begin_session();s.draw_dab(2,1,2,.5,.5,.5,1,1);auto result=get(b);const double linear_value=std::pow(.5,isgray?1.8:2.2),expected=linear?linear_value:perceptual?(1.055*std::pow(linear_value,1/2.4)-.055):.5;g_assert_cmpfloat_with_epsilon(scalar<double>(result.data()),expected,5e-6);float r,g,blue,a;s.get_color(2,1,2,&r,&g,&blue,&a,1);g_test_message("profile=%s trc=%s forward=%.17g expected=%.17g sampled=%.9g/%.9g/%.9g",isgray?"D50-gray-gamma1.8":"D65-adobe-primaries-gamma2.2",trc,scalar<double>(result.data()),expected,r,g,blue);g_assert_cmpfloat_with_epsilon(r,.5,3e-4);g_assert_cmpfloat_with_epsilon(g,.5,3e-4);g_assert_cmpfloat_with_epsilon(blue,.5,3e-4);s.cancel_session();g_object_unref(b);
 }
 }
 // Independent LCMS RGB→profile conversion checks the evaluator ingress space.
 auto srgb=cmsCreate_sRGBProfile();auto transform=cmsCreateTransform(srgb,TYPE_RGB_DBL,rgb,TYPE_RGB_DBL,INTENT_RELATIVE_COLORIMETRIC,cmsFLAGS_NOOPTIMIZE);double input[]={.8,.2,.4},expected[3],actual[4],rgba[]={.8,.2,.4,1};cmsDoTransform(transform,input,expected,1);babl_process(babl_fish(babl_format("R'G'B'A double"),babl_format_with_space("R'G'B'A double",space(rgb))),rgba,actual,1);for(int c=0;c<3;++c)g_assert_cmpfloat_with_epsilon(actual[c],expected[c],3e-4);cmsDeleteTransform(transform);
 // Compare colored Gray ingress+egress against a separate LCMS RGB→Gray transform.
 transform=cmsCreateTransform(srgb,TYPE_RGB_DBL,gray,TYPE_GRAY_DBL,INTENT_RELATIVE_COLORIMETRIC,cmsFLAGS_NOOPTIMIZE);double expected_gray=0;cmsDoTransform(transform,input,&expected_gray,1);
 auto*gray_space=space(gray);auto*gray_format=babl_format_with_space("Y'A double",gray_space);double evaluation[4];babl_process(babl_fish(babl_format("R'G'B'A double"),babl_format_with_space("R'G'B'A double",gray_space)),rgba,evaluation,1);
 auto*buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,3,1),gray_format);GeglSurface surface(buffer);shape(surface);surface.begin_session();surface.draw_dab(2,1,2,evaluation[0],evaluation[1],evaluation[2],1,1);auto output=get(buffer);g_test_message("LCMS colored Gray expected=%.17g actual=%.17g",expected_gray,scalar<double>(output.data()));g_assert_cmpfloat_with_epsilon(scalar<double>(output.data()),expected_gray,3e-4);surface.end_session();g_object_unref(buffer);cmsDeleteTransform(transform);
 cmsCloseProfile(srgb);cmsCloseProfile(rgb);cmsCloseProfile(gray);cmsFreeToneCurve(curve);cmsFreeToneCurve(graycurve);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gegl_init(&argc,&argv);g_test_add_func("/precision/72-formats",format_matrix);g_test_add_func("/precision/quantization-native-lsb",quantization_and_native_lsb);g_test_add_func("/precision/half-boundaries",half_boundaries);g_test_add_func("/precision/alpha-hdr-selection-nonincremental",alpha_hdr_selection_nonincremental);g_test_add_func("/precision/finite-hidden",finite_and_hidden);g_test_add_func("/precision/background-exception-safety",background_exception_safety);g_test_add_func("/precision/shape-paper-sampling",shape_paper_sampling);g_test_add_func("/precision/profile-trc",profile_and_trc);int result=g_test_run();gegl_exit();return result;}
