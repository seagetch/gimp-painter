/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include "core/core-types.h"
#include <stdio.h>
#include <string.h>
#include "base/base-types.h"
#include "base/pixel-region.h"
#include "paint-funcs/paint-funcs.h"
#include "paint-funcs/paint-funcs-types.h"
#define WIDTH 19
#define HEIGHT 3
int main(void){
 for(guint bytes=1;bytes<=4;++bytes)for(guint blend=0;blend<256;++blend){
  guchar sampled[WIDTH*HEIGHT*4],accum[WIDTH*HEIGHT*4],shaded[WIDTH*HEIGHT*4],color[4]={203,39,91,255};
  for(guint i=0;i<WIDTH*HEIGHT*bytes;++i){sampled[i]=(i*37+blend*13)%256;accum[i]=(i*59+blend*7+103)%256;}
  if(bytes%2==0){color[bytes-1]=255;for(guint i=0;i<WIDTH*HEIGHT;++i){if(i%7==0)sampled[i*bytes+bytes-1]=0;if(i%9==0)accum[i*bytes+bytes-1]=0;}}
  PixelRegion s,a,d;pixel_region_init_data(&s,sampled,bytes,WIDTH*bytes,0,0,WIDTH,HEIGHT);pixel_region_init_data(&a,accum,bytes,WIDTH*bytes,0,0,WIDTH,HEIGHT);pixel_region_init_data(&d,accum,bytes,WIDTH*bytes,0,0,WIDTH,HEIGHT);blend_region(&s,&a,&d,blend);
  pixel_region_init_data(&s,accum,bytes,WIDTH*bytes,0,0,WIDTH,HEIGHT);pixel_region_init_data(&d,shaded,bytes,WIDTH*bytes,0,0,WIDTH,HEIGHT);shade_region(&s,&d,color,blend);
  guchar composed[WIDTH*HEIGHT*4],coverage[WIDTH*HEIGHT],selection[WIDTH*HEIGHT],paint[WIDTH*HEIGHT*4];gboolean affect[4]={TRUE,TRUE,TRUE,TRUE};
  memcpy(composed,sampled,WIDTH*HEIGHT*bytes);for(guint n=0;n<WIDTH*HEIGHT;++n){coverage[n]=(n*31+blend*17)%256;selection[n]=(n*23+91)%256;}
  PixelRegion cov;pixel_region_init_data(&cov,coverage,1,WIDTH,0,0,WIDTH,HEIGHT);
  if(bytes%2==0){
   pixel_region_init_data(&s,sampled,bytes,WIDTH*bytes,0,0,WIDTH,HEIGHT);pixel_region_init_data(&a,shaded,bytes,WIDTH*bytes,0,0,WIDTH,HEIGHT);pixel_region_init_data(&d,composed,bytes,WIDTH*bytes,0,0,WIDTH,HEIGHT);combine_regions_replace(&s,&a,&d,&cov,NULL,blend,affect,COMBINE_INTEN_A_INTEN_A);
  }else{
   for(guint n=0;n<WIDTH*HEIGHT;++n){for(guint c=0;c<bytes;++c)paint[n*(bytes+1)+c]=shaded[n*bytes+c];paint[n*(bytes+1)+bytes]=255;}
   pixel_region_init_data(&s,paint,bytes+1,WIDTH*(bytes+1),0,0,WIDTH,HEIGHT);apply_mask_to_region(&s,&cov,173);
   combine_inten_and_inten_a_pixels(sampled,paint,composed,selection,blend,affect,WIDTH*HEIGHT,bytes);
  }
  printf("PIXELS %u %u ",bytes,blend);for(guint i=0;i<WIDTH*HEIGHT*bytes;++i)printf("%02x",accum[i]);putchar(' ');for(guint i=0;i<WIDTH*HEIGHT*bytes;++i)printf("%02x",shaded[i]);putchar(' ');for(guint i=0;i<WIDTH*HEIGHT*bytes;++i)printf("%02x",composed[i]);putchar('\n');
 }puts("SMUDGE_PIXELS_COMPLETE");return 0;
}
