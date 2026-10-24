/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
typedef unsigned char guchar;typedef int gint;typedef int32_t gint32;
static unsigned char palette[256*3],saved[256*3];static int colors,gets,sets;static unsigned long scenes;
static guchar*gimp_image_get_colormap(gint32 id,gint*n){assert(id==23);gets++;*n=colors;return palette;}
static void gimp_image_set_colormap(gint32 id,guchar*p,gint n){assert(id==23&&p==palette&&n==colors);sets++;}
static void g_printerr(const char*fmt,...){(void)fmt;assert(!"unexpected empty host palette");}
#include "old-indexed.inc"
static void check(void){int lo=255,hi=0;assert(colors>0&&colors<=256);memcpy(saved,palette,sizeof palette);for(int i=0;i<colors*3;i++){if(saved[i]<lo)lo=saved[i];if(saved[i]>hi)hi=saved[i];}gets=sets=0;indexed_normalize(23);assert(gets==1&&sets==1);for(int i=0;i<colors*3;i++){int want=hi==lo?saved[i]:255*(saved[i]-lo)/(hi-lo);assert(palette[i]==want);}for(int i=colors*3;i<256*3;i++)assert(palette[i]==saved[i]);scenes++;}
static uint32_t rng=0x8ad711ef;static unsigned rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(void){for(int lo=0;lo<256;lo++)for(int hi=lo;hi<256;hi++){colors=3;memset(palette,0xa5,sizeof palette);for(int i=0;i<colors*3;i++)palette[i]=lo+(i*17)%(hi-lo+1);palette[0]=lo;palette[8]=hi;check();}for(int n=0;n<10000;n++){colors=1+rnd()%256;memset(palette,0xa5,sizeof palette);for(int i=0;i<colors*3;i++)palette[i]=rnd()%256;check();}printf("PASS %lu nonempty palette scenes\n",scenes);return 0;}
