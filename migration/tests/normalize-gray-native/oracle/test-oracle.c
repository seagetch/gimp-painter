/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char guchar;
typedef int gint;
typedef int gboolean;
typedef double gdouble;
typedef void *gpointer;
typedef struct {int drawable_id,bpp;} GimpDrawable;
static unsigned char input[64*4],output[64*4];
static int count,alpha,first,limit,reads,writes;
static unsigned long scenes;
static int gimp_drawable_has_alpha(int id){assert(id==7);return alpha;}
static void gimp_rgn_iterate1(GimpDrawable*d,int unused,void(*fn)(const guchar*,gint,gpointer),gpointer p){assert(!unused);reads++;for(int i=first;i<limit;i++)fn(input+i*d->bpp,d->bpp,p);}
static void gimp_rgn_iterate2(GimpDrawable*d,int unused,void(*fn)(const guchar*,guchar*,gint,gpointer),gpointer p){assert(!unused);writes++;for(int i=first;i<limit;i++)fn(input+i*d->bpp,output+i*d->bpp,d->bpp,p);}
#include "old-normalize.inc"
static void check(int bpp,int has_alpha){
 int lo=255,hi=0,visible=0,channels=bpp-has_alpha;unsigned char saved[sizeof input];
 assert(first>=0&&limit>first&&limit<=count);alpha=has_alpha;
 for(int i=first;i<limit;i++)if(!alpha||input[i*bpp+channels]){visible++;for(int c=0;c<channels;c++){int v=input[i*bpp+c];if(v<lo)lo=v;if(v>hi)hi=v;}}
 assert(visible);
 /* The old LUT is initialized only on this interval: never run undefined domains. */
 for(int i=first;i<limit;i++)for(int c=0;c<channels;c++)assert(input[i*bpp+c]>=lo&&input[i*bpp+c]<=hi);
 NormalizeParam_t probe;probe.min=255;probe.max=0;probe.alpha=channels;probe.has_alpha=has_alpha;
 for(int i=first;i<limit;i++)find_min_max(input+i*bpp,bpp,&probe);
 assert(probe.min==lo&&probe.max==hi); /* Refuse wrong extrema before any LUT access. */
 memcpy(saved,input,sizeof input);memset(output,0xa5,sizeof output);reads=writes=0;GimpDrawable d={7,bpp};normalize(&d);assert(reads==1&&writes==1);assert(!memcmp(saved,input,sizeof input));
 for(int i=0;i<count;i++)for(int c=0;c<bpp;c++){int want=0xa5;if(i>=first&&i<limit){int v=input[i*bpp+c];want=(has_alpha&&c==channels)?v:hi==lo?v:(255*(v-lo))/(hi-lo);}assert(output[i*bpp+c]==want);}
 scenes++;
}
static uint32_t rng=0x321acafe;
static unsigned rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(void){
 for(int bpp=1;bpp<=4;bpp++){int has_alpha=bpp==2||bpp==4,channels=bpp-has_alpha;count=7;first=1;limit=6;
  for(int lo=0;lo<256;lo++)for(int hi=lo;hi<256;hi++){
   memset(input,0xdd,sizeof input);
   for(int i=first;i<limit;i++){for(int c=0;c<channels;c++)input[i*bpp+c]=(unsigned char)(lo+(i+c)%(hi-lo+1));if(has_alpha)input[i*bpp+channels]=(i==3)?0:(i==4)?1:255;}
   input[first*bpp]=lo;input[(limit-1)*bpp+channels-1]=hi;check(bpp,has_alpha);
  }
 }
 for(int n=0;n<10000;n++){int bpp=1+rnd()%4,has_alpha=bpp==2||bpp==4,channels=bpp-has_alpha;count=64;first=1+rnd()%10;limit=first+2+rnd()%(63-first);int lo=rnd()%256,hi=lo+rnd()%(256-lo);
  for(int i=0;i<count;i++){for(int c=0;c<channels;c++)input[i*bpp+c]=lo+rnd()%(hi-lo+1);if(has_alpha)input[i*bpp+channels]=rnd()%256;}
  input[first*bpp]=lo;input[(limit-1)*bpp+channels-1]=hi;if(has_alpha){input[first*bpp+channels]=1;input[(limit-1)*bpp+channels]=255;}check(bpp,has_alpha);
 }
 printf("PASS %lu defined-domain scenes\n",scenes);return 0;
}
