/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_NATIVE_PIXELS_HPP
#define GIMP_PAINTER_NATIVE_PIXELS_HPP
#include "../../painter/gimp-painter-visibility.h"
#include <babl/babl.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint { namespace NativePixel {
inline double unit(double v){return std::max(0.,std::min(1.,v));}
inline void finite(double v){if(!std::isfinite(v))throw std::invalid_argument("Nonfinite consumed paint pixel");}
template<class T>T load(const unsigned char*p){T v;std::memcpy(&v,p,sizeof v);return v;}
template<class T>void store(unsigned char*p,T v){std::memcpy(p,&v,sizeof v);}
inline double half_decode(std::uint16_t bits)
{
  const unsigned exp=(bits>>10)&31, mantissa=bits&1023;
  const double v=exp==31?(mantissa?std::numeric_limits<double>::quiet_NaN():std::numeric_limits<double>::infinity()):
    exp?std::ldexp(double(1024+mantissa),int(exp)-25):std::ldexp(double(mantissa),-24);
  return bits&0x8000?-v:v;
}
inline std::uint16_t half_encode(double v)
{
  const unsigned sign=std::signbit(v)?0x8000:0;v=std::min(std::abs(v),65504.);
  if(!v)return sign;
  int exponent;std::frexp(v,&exponent);
  const int shift=std::max(-24,exponent-11);
  const double scaled=std::ldexp(v,-shift),floor=std::floor(scaled),fraction=scaled-floor;
  unsigned mantissa=unsigned(floor)+(fraction>.5||(fraction==.5&&(unsigned(floor)&1)));
  if(!mantissa)return sign;
  if(shift==-24&&mantissa<1024)return sign|mantissa;
  if(mantissa==2048){mantissa=1024;++exponent;}
  const unsigned e=unsigned(std::max(1,exponent+14));
  return sign|(e<<10)|(mantissa-1024);
}
struct Format {
  enum Type{U8,U16,U32,Half,Float,Double}type;
  const Babl*native;const Babl*working;const Babl*evaluation;
  int colors=0,components=0,scalar_bytes=0,bytes=0;bool alpha=false,legacy=false;
  explicit Format(const Babl*f):native(f)
  {
    const std::string model=babl_get_name(babl_format_get_model(f));
    for(const char*m:{"RGB","R'G'B'","R~G~B~"})if(model==m||model==std::string(m)+"A"){colors=3;alpha=model!=m;}
    for(const char*m:{"Y","Y'","Y~"})if(model==m||model==std::string(m)+"A"){colors=1;alpha=model!=m;}
    if(!colors||babl_format_is_palette(f))throw std::invalid_argument("Painter requires native RGB/RGBA/Gray/Gray-alpha storage");
    components=colors+alpha;
    const std::string name=babl_get_name(babl_format_get_type(f,0));
    if(name=="u8"){type=U8;scalar_bytes=1;}
    else if(name=="u16"){type=U16;scalar_bytes=2;}
    else if(name=="u32"){type=U32;scalar_bytes=4;}
    else if(name=="half"){type=Half;scalar_bytes=2;}
    else if(name=="float"){type=Float;scalar_bytes=4;}
    else if(name=="double"){type=Double;scalar_bytes=8;}
    else throw std::invalid_argument("Unsupported native Painter component type");
    const Babl*space=babl_format_get_space(f);bytes=components*scalar_bytes;
    if(f!=babl_format_with_space((model+" "+name).c_str(),space)||babl_format_get_bytes_per_pixel(f)!=bytes)
      throw std::invalid_argument("Painter requires packed homogeneous native components");
    working=babl_format_with_space((model+" double").c_str(),space);
    evaluation=babl_format_with_space("R'G'B'A double",space);
    legacy=type==U8&&(model=="R'G'B'"||model=="R'G'B'A"||model=="Y'"||model=="Y'A");
  }
  const Babl*with_alpha_double()const
  {return babl_format_with_space((std::string(babl_get_name(babl_format_get_model(working)))+(alpha?"":"A")+" double").c_str(),babl_format_get_space(native));}
  double decode(const unsigned char*p)const
  {
    switch(type){case U8:return *p/255.;case U16:return load<std::uint16_t>(p)/65535.;case U32:return load<std::uint32_t>(p)/4294967295.;case Half:return half_decode(load<std::uint16_t>(p));case Float:return load<float>(p);case Double:return load<double>(p);}return 0;
  }
  void encode(unsigned char*p,double v)const
  {
    finite(v);
    switch(type){case U8:store(p,std::uint8_t(std::floor(unit(v)*255.+.5)));break;
      case U16:store(p,std::uint16_t(std::floor(unit(v)*65535.+.5)));break;
      case U32:store(p,std::uint32_t(std::floor(unit(v)*4294967295.+.5)));break;
      case Half:store(p,half_encode(v));break;
      case Float:store(p,float(std::max(-double(std::numeric_limits<float>::max()),std::min(double(std::numeric_limits<float>::max()),v))));break;
      case Double:store(p,v);break;}
  }
  void read(const unsigned char*p,double rgba[4])const
  {
    rgba[0]=decode(p);rgba[1]=colors==1?rgba[0]:decode(p+scalar_bytes);rgba[2]=colors==1?rgba[0]:decode(p+scalar_bytes*2);
    rgba[3]=alpha?decode(p+scalar_bytes*colors):1.;
  }
  bool write(unsigned char*p,const double rgba[4])const
  {
    bool changed=false;
    for(int c=0;c<components;++c){const double value=rgba[c==colors?3:c];auto*q=p+c*scalar_bytes;
      const double old=decode(q);
      if(value==old||(std::isnan(value)&&std::isnan(old)))continue;
      unsigned char encoded[8];encode(encoded,value);
      if(std::memcmp(q,encoded,scalar_bytes)){std::memcpy(q,encoded,scalar_bytes);changed=true;}}
    return changed;
  }
  void color(double r,double g,double b,double output[4])const
  {
    double input[4]={r,g,b,1.},result[4]={0,0,0,1.};
    babl_process(babl_fish(evaluation,working),input,result,1);
    output[0]=result[0];output[1]=colors==1?result[0]:result[1];output[2]=colors==1?result[0]:result[2];output[3]=1.;
    for(int c=0;c<3;++c)finite(output[c]);
  }
  void sample(const double input[4],double output[4])const
  {
    double pixel[4]={input[0],input[1],input[2],input[3]};if(colors==1&&alpha)pixel[1]=input[3];
    babl_process(babl_fish(working,evaluation),pixel,output,1);
    for(int c=0;c<4;++c)finite(output[c]);
  }
};
// A weighted sum avoids overflow from subtracting opposite-sign finite HDR colors.
inline double mix(double base,double color,double t){if(!t||base==color)return base;if(t==1)return color;const double result=base*(1-t)+color*t;finite(result);return result;}
inline void blend(double pixel[4],const double color[4],const double background[4],double t,double source_alpha,bool alpha)
{
  if(!t)return;
  source_alpha=unit(source_alpha);
  for(int c=0;c<4;++c)finite(pixel[c]);
  if(alpha){const double old=unit(pixel[3]),a=mix(old,source_alpha,t);if(a>0){const double weight=t*source_alpha/a;for(int c=0;c<3;++c)pixel[c]=mix(pixel[c],color[c],weight);}else pixel[0]=pixel[1]=pixel[2]=0;pixel[3]=a;}
  else for(int c=0;c<3;++c)pixel[c]=mix(pixel[c],mix(background[c],color[c],source_alpha),t);
}
inline void lock(double pixel[4],const double color[4],double t,bool alpha)
{
  if(!t)return;
  if(alpha){finite(pixel[3]);if(unit(pixel[3])==0)return;}
  for(int c=0;c<3;++c){finite(pixel[c]);pixel[c]=mix(pixel[c],color[c],t);}
}
} } }
#endif
