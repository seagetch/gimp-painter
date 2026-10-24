/* SPDX-License-Identifier: GPL-3.0-or-later
 * Literal old exchange and GimpRGB helpers, bounded mock row transport.
 * Independent oracle uses integer channels, not GEGL or copied floating math.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
typedef unsigned char guchar;typedef int gint;typedef unsigned guint;typedef int gboolean;typedef double gdouble;
#define TRUE 1
#define FALSE 0
#define CLAMP(x,l,h) ((x)>(h)?(h):((x)<(l)?(l):(x)))
#define ROUND(x) ((int)floor((x)+0.5))
#define g_return_if_fail(x) do{assert(x);}while(0)
#define g_new(t,n) ((t*)malloc(sizeof(t)*(n)))
#define g_free free
typedef struct{double r,g,b,a;}GimpRGB;
typedef struct{GimpRGB from,to,threshold;}myParams;static myParams xargs;
#include "old-rgb.inc"
typedef struct{int drawable_id,width,bpp;}GimpDrawable;
typedef struct{int x,y,w,h;}GimpPreview;
typedef struct{int shadow;}GimpPixelRgn;
#define GIMP_DRAWABLE_PREVIEW(x) (x)
static unsigned char source_pixels[9*7*4],destination[9*7*4];static int roi_x,roi_y,roi_w,roi_h,stride,alpha,reads,writes,merges,updates,flushes,previews;
static void gimp_preview_get_position(GimpPreview*p,int*x,int*y){*x=p->x;*y=p->y;}
static void gimp_preview_get_size(GimpPreview*p,int*w,int*h){*w=p->w;*h=p->h;}
static int gimp_drawable_mask_intersect(int id,int*x,int*y,int*w,int*h){assert(id==1);*x=roi_x;*y=roi_y;*w=roi_w;*h=roi_h;return *w>0&&*h>0;}
static int gimp_drawable_has_alpha(int id){assert(id==1);return alpha;}
static void gimp_pixel_rgn_init(GimpPixelRgn*r,GimpDrawable*d,int x,int y,int w,int h,int dirty,int shadow){(void)d;(void)x;(void)y;(void)w;(void)h;(void)dirty;r->shadow=shadow;}
static void gimp_pixel_rgn_get_row(GimpPixelRgn*r,unsigned char*p,int x,int y,int w){assert(!r->shadow);memcpy(p,source_pixels+y*stride+x*(alpha?4:3),w*(alpha?4:3));reads++;}
static void gimp_pixel_rgn_set_row(GimpPixelRgn*r,unsigned char*p,int x,int y,int w){assert(r->shadow);memcpy(destination+y*stride+x*(alpha?4:3),p,w*(alpha?4:3));writes++;}
static void gimp_progress_update(double v){(void)v;}
static void gimp_drawable_flush(GimpDrawable*d){(void)d;flushes++;}
static void gimp_drawable_merge_shadow(int id,int undo){assert(id==1&&undo);merges++;}
static void gimp_drawable_update(int id,int x,int y,int w,int h){assert(id==1&&x==roi_x&&y==roi_y&&w==roi_w&&h==roi_h);updates++;}
static void gimp_drawable_preview_draw_region(GimpPreview*p,GimpPixelRgn*r){(void)p;assert(r->shadow);previews++;}
#include "old-exchange.inc"
static uint64_t cases;static uint32_t seed=0x28461;
static unsigned random_byte(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed&255;}
static void oracle(const unsigned char*p,unsigned char*out,const unsigned char args[9],int bpp){int hit=1;for(int c=0;c<3;c++){int distance=(int)p[c]-args[c];if(distance<0)distance=-distance;if(distance>args[6+c])hit=0;}for(int c=0;c<3;c++){int d=abs((int)p[c]-args[c]);unsigned value=args[3+c]+d;out[c]=hit?(value>255?255:value):p[c];}if(bpp==4)out[3]=p[3];}
static void configure(const unsigned char a[9]){gimp_rgb_set_uchar(&xargs.from,a[0],a[1],a[2]);gimp_rgb_set_uchar(&xargs.to,a[3],a[4],a[5]);gimp_rgb_set_uchar(&xargs.threshold,a[6],a[7],a[8]);}
static void one(const unsigned char args[9],int bpp,int empty,int preview){GimpDrawable d={1,9,bpp};unsigned char expected[sizeof destination],saved[sizeof source_pixels];alpha=bpp==4;stride=9*bpp;roi_x=2;roi_y=1;roi_w=empty?0:5;roi_h=empty?0:4;reads=writes=merges=updates=flushes=previews=0;for(unsigned i=0;i<sizeof source_pixels;i++)source_pixels[i]=random_byte();memcpy(saved,source_pixels,sizeof saved);memset(destination,0xa5,sizeof destination);memcpy(expected,destination,sizeof expected);configure(args);for(int y=roi_y;y<roi_y+roi_h;y++)for(int x=roi_x;x<roi_x+roi_w;x++)oracle(source_pixels+y*stride+x*bpp,expected+y*stride+x*bpp,args,bpp);GimpPreview p={roi_x,roi_y,roi_w,roi_h};exchange(&d,preview?&p:NULL);assert(!memcmp(source_pixels,saved,sizeof saved));assert(!memcmp(destination,expected,sizeof expected));assert(reads==roi_h&&writes==roi_h);assert(merges==(!empty&&!preview)&&updates==(!empty&&!preview)&&flushes==(!empty&&!preview)&&previews==preview);cases++;}
static void critical(unsigned from,unsigned threshold,int channel,int pixel,unsigned to,int bpp){
 if(pixel<0||pixel>255)return;
 unsigned char args[9]={127,127,127,17,17,17,255,255,255};args[channel]=from;args[3+channel]=to;args[6+channel]=threshold;unsigned char input[4]={127,127,127,0},expected[4]={0};input[channel]=pixel;input[3]=(from+threshold)%256;oracle(input,expected,args,bpp);GimpDrawable d={1,9,bpp};alpha=bpp==4;stride=9*bpp;roi_x=2;roi_y=1;roi_w=roi_h=1;reads=writes=merges=updates=flushes=previews=0;memset(source_pixels,0x5a,sizeof source_pixels);memset(destination,0xa5,sizeof destination);memcpy(source_pixels+stride+2*bpp,input,bpp);configure(args);exchange(&d,NULL);assert(!memcmp(destination+stride+2*bpp,expected,bpp));assert(!memcmp(source_pixels+stride+2*bpp,input,bpp));assert(reads==1&&writes==1&&merges==1&&updates==1);cases++;
}
int main(void){
 /* Exhaust every byte from/threshold pair through literal old floating helpers. */
 for(unsigned from=0;from<256;from++)for(unsigned t=0;t<256;t++){GimpRGB f={0},v={0},lo,hi;guchar l,h,roundtrip;gimp_rgb_set_uchar(&f,from,from,from);gimp_rgb_set_uchar(&v,t,t,t);lo=hi=f;gimp_rgb_subtract(&lo,&v);gimp_rgb_add(&hi,&v);gimp_rgb_clamp(&lo);gimp_rgb_clamp(&hi);gimp_rgb_get_uchar(&lo,&l,NULL,NULL);gimp_rgb_get_uchar(&hi,&h,NULL,NULL);gimp_rgb_get_uchar(&f,&roundtrip,NULL,NULL);assert(roundtrip==from&&l==(from>t?from-t:0)&&h==(from+t>255?255:from+t));for(unsigned pixel=0;pixel<256;pixel++){assert((pixel>=l&&pixel<=h)==(abs((int)pixel-(int)from)<=(int)t));cases++;}}
 for(unsigned f=0;f<256;f++)for(unsigned t=0;t<256;t++)for(int c=0;c<3;c++){int pts[]={ (int)f-(int)t-1,(int)f-(int)t,(int)f,(int)f+(int)t,(int)f+(int)t+1};for(int k=0;k<5;k++)critical(f,t,c,pts[k],k%2?255:0,k%2?3:4);}
 for(unsigned i=0;i<20000;i++){unsigned char a[9];for(int c=0;c<9;c++)a[c]=random_byte();one(a,i%2?3:4,0,i%3==0);}
 unsigned char zero[9]={0},all[9]={255,255,255,255,255,255,255,255,255};one(zero,3,1,0);one(all,4,1,0);one(zero,3,0,0);one(all,4,0,0);
 printf("PASS %llu assertions/scenes: exhaustive byte bounds, randomized RGB/RGBA ROI, alpha/source/outside preservation, empty ROI and preview/shadow dispatch\n",(unsigned long long)cases);return 0;}
