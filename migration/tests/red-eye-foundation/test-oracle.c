/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
typedef unsigned char guchar;
typedef int gint;
typedef int gboolean;
typedef double gdouble;
#define RED_FACTOR 0.5133333
#define GREEN_FACTOR 1
#define BLUE_FACTOR 0.1933333
#define CLAMP(x,l,h) (((x)>(h))?(h):(((x)<(l))?(l):(x)))
static gint threshold;
#include "old-inner-loop.inc"
/* Independent rational equations: decimal constants represented as integer
 * ratios, not the old floating expression or a modern GEGL operation. */
static unsigned char rational_red(unsigned r,unsigned g,unsigned b,int t)
{
  int ar=(int)(r*UINT64_C(5133333)/10000000);
  int ab=(int)(b*UINT64_C(1933333)/10000000);
  int64_t at=((int64_t)t-50)*2;
  if(ar<(int64_t)g-at || ar<(int64_t)ab-at)return (unsigned char)r;
  uint64_t out=((uint64_t)g+ab)*10000000/10266666;
  return out>255?255:(unsigned char)out;
}
static int admitted(int64_t t)
{
  /* Both old arithmetic stages and worst-case G - adjusted_threshold must
   * fit signed32; rejects undefined old domains before invoking old C. */
  int64_t shifted=t-50,adjusted=shifted*2;
  return shifted>=INT32_MIN&&shifted<=INT32_MAX&&adjusted>=INT32_MIN&&adjusted<=INT32_MAX&&
         255-adjusted<=INT32_MAX&&-adjusted>=INT32_MIN;
}
static uint64_t cases;
static void one(unsigned r,unsigned g,unsigned b,int t,unsigned alpha,int bpp)
{
  unsigned char src[4]={r,g,b,alpha},dst[4]={0xa5,0xa5,0xa5,0xa5};
  if(!admitted(t))abort();
  threshold=t;
  redeye_inner_loop(src,dst,1,1,bpp,bpp==4,bpp);
  unsigned char expected=rational_red(r,g,b,t);
  if(dst[0]!=expected||dst[1]!=g||dst[2]!=b||(bpp==4&&dst[3]!=alpha)||(bpp==3&&dst[3]!=0xa5))
    {fprintf(stderr,"mismatch %u/%u/%u t%d a%u bpp%d actual%u expected%u\n",r,g,b,t,alpha,bpp,dst[0],expected);abort();}
  cases++;
}
int main(void)
{
  if(sizeof(int)!=4)abort();
  const int low=-1073741646,high=1073741873;
  if(!admitted(low)||!admitted(high)||admitted((int64_t)low-1)||admitted((int64_t)high+1)||admitted(INT32_MIN)||admitted(INT32_MAX))abort();
  for(unsigned r=0;r<256;r++)for(unsigned g=0;g<256;g++)for(unsigned b=0;b<256;b++)
    {
      int ar=(int)(r*UINT64_C(5133333)/10000000),ab=(int)(b*UINT64_C(1933333)/10000000);
      int d=(g>(unsigned)ab?(int)g:ab)-ar;
      /* Mathematical ceil(d/2), including negative odd d. */
      int critical=50+(d>=0?(d+1)/2:d/2);
      int thresholds[]={low,high,critical-1,critical,0,50,100};
      for(unsigned i=0;i<sizeof(thresholds)/sizeof(thresholds[0]);i++)
        {one(r,g,b,thresholds[i],(r+g+b+i)&255,4);one(r,g,b,thresholds[i],0,3);}
    }
  /* Padded multirow traversal and alpha preservation at zero/partial/full. */
  unsigned char src[3*37],dst[3*37];
  for(unsigned i=0;i<sizeof(src);i++)src[i]=(i*73+19)&255;
  memset(dst,0xa5,sizeof(dst));threshold=73;redeye_inner_loop(src,dst,8,3,4,1,37);
  for(unsigned y=0;y<3;y++)for(unsigned x=0;x<37;x++)
    {unsigned i=y*37+x;if(x>=32){if(dst[i]!=0xa5)abort();}else{unsigned pixel=y*37+(x/4)*4;unsigned char v=x%4?src[i]:rational_red(src[pixel],src[pixel+1],src[pixel+2],73);if(dst[i]!=v)abort();}}
  /* Zero width/height never touches output. */
  memset(dst,0xa5,sizeof(dst));redeye_inner_loop(src,dst,0,3,4,1,37);redeye_inner_loop(src,dst,8,0,4,1,37);
  for(unsigned i=0;i<sizeof(dst);i++)if(dst[i]!=0xa5)abort();
  printf("PASS %llu scalar RGB/RGBA comparisons;all16777216 RGB triples;7 threshold classes each;critical transitions;safe signed32 endpoints;unsafe-domain rejection;row padding;zero extents\n",(unsigned long long)cases);
  return 0;
}
