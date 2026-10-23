/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <float.h>
#include <math.h>
#include "gimppainterfloatkernel.h"
static double unit (float x)
{ return !isfinite(x) || x <= 0 ? 0 : x >= 1 ? 1 : x; }
static float color (double x)
{ return x > FLT_MAX ? FLT_MAX : x < -FLT_MAX ? -FLT_MAX : (float)x; }
int
gimp_painter_float_composite (const float backdrop[4], const float source[4],
                              float out[4], float opacity, float mask,
                              GimpPainterFloatKernelMode mode)
{
  double a[4], b[4], alpha, weight=unit(opacity)*unit(mask), ratio;
  int k;
  if (mode < GIMP_PAINTER_FLOAT_ERASE || mode > GIMP_PAINTER_FLOAT_MULTIPLY)
    return 0;
  for(k=0;k<3;++k) {
    a[k]=isfinite(backdrop[k])?backdrop[k]:0;
    b[k]=isfinite(source[k])?source[k]:0;
  }
  a[3]=unit(backdrop[3]); b[3]=unit(source[3]);
  if(mode==GIMP_PAINTER_FLOAT_DST_IN || mode==GIMP_PAINTER_FLOAT_DST_OUT)
    for(k=0;k<4;++k) { double t=a[k];a[k]=b[k];b[k]=t; }
  if(mode==GIMP_PAINTER_FLOAT_ERASE || mode==GIMP_PAINTER_FLOAT_ANTI_ERASE) {
    alpha=mode==GIMP_PAINTER_FLOAT_ERASE ? a[3]*(1-b[3]*weight) :
                                         a[3]+(1-a[3])*b[3]*weight;
    for(k=0;k<3;++k)out[k]=color(a[k]);
  } else if(mode==GIMP_PAINTER_FLOAT_NORMAL || mode==GIMP_PAINTER_FLOAT_MULTIPLY) {
    if(mode==GIMP_PAINTER_FLOAT_MULTIPLY) {
      for(k=0;k<3;++k)b[k]*=a[k];
      b[3]=fmin(a[3],b[3]);
    }
    double s=b[3]*weight;
    alpha=a[3]+(1-a[3])*s;
    ratio=alpha>0?s/alpha:0;
    for(k=0;k<3;++k)out[k]=color(a[k]*(1-ratio)+b[k]*ratio);
    if(mode==GIMP_PAINTER_FLOAT_MULTIPLY && a[3]>0)alpha=a[3];
  } else {
    if(mode==GIMP_PAINTER_FLOAT_SRC_IN || mode==GIMP_PAINTER_FLOAT_DST_IN)
      b[3]*=a[3];
    else if(mode==GIMP_PAINTER_FLOAT_SRC_OUT || mode==GIMP_PAINTER_FLOAT_DST_OUT)
      b[3]*=1-a[3];
    alpha=(1-weight)*a[3]+weight*b[3];
    ratio=alpha>0?weight*b[3]/alpha:0;
    for(k=0;k<3;++k)out[k]=color(a[k]*(1-ratio)+b[k]*ratio);
  }
  out[3]=(float)fmin(1,fmax(0,alpha));
  return 1;
}
