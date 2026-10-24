/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
typedef unsigned char guchar;typedef int gint;typedef void *gpointer;
#define GIMP_RGB_LUMINANCE_RED (0.2126)
#define GIMP_RGB_LUMINANCE_GREEN (0.7152)
#define GIMP_RGB_LUMINANCE_BLUE (0.0722)
static gint lum_red_lookup[256],lum_green_lookup[256],lum_blue_lookup[256];
static gint final_red_lookup[256],final_green_lookup[256],final_blue_lookup[256];
static struct {struct {double r,g,b,a;} color;} cvals;
static void
colorify_func (const guchar *src,
               guchar       *dest,
               gint          bpp,
               gpointer      data)
{
  gint lum;

  lum = lum_red_lookup[src[0]]   +
        lum_green_lookup[src[1]] +
        lum_blue_lookup[src[2]];

  dest[0] = final_red_lookup[lum];
  dest[1] = final_green_lookup[lum];
  dest[2] = final_blue_lookup[lum];

  if (bpp == 4)
    dest[3] = src[3];
}
void old_colorify_init(double r,double g,double b,double a){int i;cvals.color.r=r;cvals.color.g=g;cvals.color.b=b;cvals.color.a=a;
  for (i = 0; i < 256; i ++)
    {
      lum_red_lookup[i]     = i * GIMP_RGB_LUMINANCE_RED;
      lum_green_lookup[i]   = i * GIMP_RGB_LUMINANCE_GREEN;
      lum_blue_lookup[i]    = i * GIMP_RGB_LUMINANCE_BLUE;
      final_red_lookup[i]   = i * cvals.color.r;
      final_green_lookup[i] = i * cvals.color.g;
      final_blue_lookup[i]  = i * cvals.color.b;
    }
}
void old_colorify_pixel(const unsigned char *src,unsigned char *dst,int bpp){colorify_func(src,dst,bpp,0);}
int old_colorify_lut(int i){return final_red_lookup[i];}
