/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <fenv.h>
#include <stdlib.h>
_Static_assert(DBL_MANT_DIG==53 && DBL_MAX_EXP==1024,"IEEE binary64 required");
void old_colorify_init(double,double,double,double);
void old_colorify_pixel(const unsigned char*,unsigned char*,int);
int old_colorify_lut(int);
/* Independent integer simulation of round-to-nearest-even binary64 i*c,
 * then truncation. No floating multiplication is used by this oracle. */
static unsigned scale(unsigned i,double c){
 uint64_t bits;memcpy(&bits,&c,8);unsigned e=(bits>>52)&2047;
 uint64_t m=bits&UINT64_C(0xfffffffffffff);int exp;
 if(!i||!bits)return 0;
 if(e){m|=UINT64_C(1)<<52;exp=(int)e-1023-52;}else exp=-1074;
 uint64_t product=m*i;if(!product)return 0;
 unsigned top=63-(unsigned)__builtin_clzll(product);
 if(top>52){unsigned shift=top-52;uint64_t q=product>>shift,rem=product&((UINT64_C(1)<<shift)-1),half=UINT64_C(1)<<(shift-1);
  if(rem>half||(rem==half&&(q&1)))q++;
  product=q;exp+=(int)shift;
 }
 if(exp>=0)return (unsigned)(product<<exp);
 return -exp>=64?0:(unsigned)(product>>-exp);
}
static void fail(unsigned r,unsigned g,unsigned b,unsigned c,unsigned got,unsigned expected){fprintf(stderr,"mismatch %u/%u/%u channel%u got%u expected%u\n",r,g,b,c,got,expected);exit(1);}
int main(void){
 if(fegetround()!=FE_TONEAREST){fputs("requires round-to-nearest-even\n",stderr);return 3;}
 const double colors[][4]={{0,0,0,0},{1,1,1,1},{1,0,0,.3},{0,1,0,.5},{0,0,1,0},{.5,.25,.75,1},{.1,.2,.3,.7},{0x1.fffffffffffffp-1,0x1p-1022,0x1p-1074,1}};
 uint64_t count=0,boundaries=0;
 for(unsigned k=0;k<sizeof colors/sizeof colors[0];k++){
  unsigned lut[3][256];for(unsigned c=0;c<3;c++)for(unsigned x=0;x<256;x++)lut[c][x]=scale(x,colors[k][c]);
  old_colorify_init(colors[k][0],colors[k][1],colors[k][2],colors[k][3]);
  for(unsigned r=0;r<256;r++)for(unsigned g=0;g<256;g++)for(unsigned b=0;b<256;b++){
   unsigned char in[4]={(unsigned char)r,(unsigned char)g,(unsigned char)b,(unsigned char)(r+g+b)},out[4];
   unsigned lum=r*2126/10000+g*7152/10000+b*722/10000;
   for(int channels=3;channels<=4;channels++){
    memset(out,0xa5,4);old_colorify_pixel(in,out,channels);
    for(unsigned c=0;c<3;c++)if(out[c]!=lut[c][lum])fail(r,g,b,c,out[c],lut[c][lum]);
    if(out[3]!=(channels==4?in[3]:0xa5))fail(r,g,b,3,out[3],channels==4?in[3]:0xa5);count++;
   }
  }
 }
 for(unsigned i=1;i<256;i++)for(unsigned numerator=0;numerator<=i;numerator++){
  double middle=(double)numerator/i,near[3]={nextafter(middle,0),middle,nextafter(middle,1)};
  for(unsigned n=0;n<3;n++){
   old_colorify_init(near[n],0,0,0);unsigned expected=scale(i,near[n]);unsigned actual=old_colorify_lut(i);
   if(actual!=expected)fail(i,numerator,n,0,actual,expected);boundaries++;
  }
 }
 old_colorify_init(1,1,1,0);unsigned char white[4]={255,255,255,0},out[4];old_colorify_pixel(white,out,4);
 if(out[0]!=254||out[1]!=254||out[2]!=254||out[3]!=0)return 2;
 printf("PASS Colorify %llu complete RGB/RGBA pixel comparisons;%llu binary64 tint-boundary comparisons;white254;alpha/hidden RGB preserved\n",(unsigned long long)count,(unsigned long long)boundaries);return 0;
}
