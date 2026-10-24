#include <stdint.h>
typedef int gint;typedef unsigned char guchar;typedef void *gpointer;
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX_PATTERNS 9
#define MAX_PATTERN_SIZE 108
static const gint   pattern_width[MAX_PATTERNS] = { 2, 4, 1, 1, 2, 3, 6, 6, 5 };
static const gint   pattern_height[MAX_PATTERNS] = { 6, 12, 3, 6, 12, 3, 6,
                                                     18, 15 };
static const gint pattern[MAX_PATTERNS][MAX_PATTERN_SIZE] =
{
  {
    0, 1,
    0, 2,
    1, 2,
    1, 0,
    2, 0,
    2, 1,
  },
  {
    0, 0, 1, 1,
    0, 0, 1, 1,
    0, 0, 2, 2,
    0, 0, 2, 2,
    1, 1, 2, 2,
    1, 1, 2, 2,
    1, 1, 0, 0,
    1, 1, 0, 0,
    2, 2, 0, 0,
    2, 2, 0, 0,
    2, 2, 1, 1,
    2, 2, 1, 1,
  },
  {
    0,
    1,
    2,
  },
  {
    0,
    0,
    1,
    1,
    2,
    2,
  },
  {
    0, 1,
    0, 1,
    0, 2,
    0, 2,
    1, 2,
    1, 2,
    1, 0,
    1, 0,
    2, 0,
    2, 0,
    2, 1,
    2, 1,
  },
  {
    0, 1, 2,
    2, 0, 1,
    1, 2, 0,
  },
  {
    0, 0, 1, 1, 2, 2,
    0, 0, 1, 1, 2, 2,
    2, 2, 0, 0, 1, 1,
    2, 2, 0, 0, 1, 1,
    1, 1, 2, 2, 0, 0,
    1, 1, 2, 2, 0, 0,
  },
  {
    2, 2, 0, 0, 0, 0,
    2, 2, 2, 0, 0, 2,
    2, 2, 2, 2, 2, 2,
    2, 2, 2, 1, 1, 2,
    2, 2, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1,
    0, 0, 1, 1, 1, 1,
    0, 0, 0, 1, 1, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 2, 2, 0,
    0, 0, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2,
    1, 1, 2, 2, 2, 2,
    1, 1, 1, 2, 2, 1,
    1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 1,
    1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
  },
  {
    0, 1, 2, 0, 0,
    1, 1, 1, 2, 0,
    0, 1, 2, 2, 2,
    0, 0, 1, 2, 0,
    0, 1, 1, 1, 2,
    2, 0, 1, 2, 2,
    0, 0, 0, 1, 2,
    2, 0, 1, 1, 1,
    2, 2, 0, 1, 2,
    2, 0, 0, 0, 1,
    1, 2, 0, 1, 1,
    2, 2, 2, 0, 1,
    1, 2, 0, 0, 0,
    1, 1, 2, 0, 1,
    1, 2, 2, 2, 0,
  }
};
static struct {gint pattern_number,additive,rotated;} vvals;
static void
video_func (gint x,
            gint y,
            const guchar *src,
            guchar *dest,
            gint bpp,
            gpointer data)
{
  gint b, sel_b;

  if (vvals.rotated)
    {
      sel_b = pattern[vvals.pattern_number]
        [pattern_width[vvals.pattern_number]*
         (x % pattern_height[vvals.pattern_number]) +
         y % pattern_width[vvals.pattern_number] ];
    }
  else
    {
      sel_b = pattern[vvals.pattern_number]
        [pattern_width[vvals.pattern_number]*
         (y % pattern_height[vvals.pattern_number]) +
         x % pattern_width[vvals.pattern_number] ];
    }

  for (b = 0; b < bpp; b++)
    {
      if (b < 3 )
        {
          dest [b] = (sel_b == b) ? src[b] : 0;
          if (vvals.additive)
            {
              gint temp = (gint) dest[b] + src[b];
              dest[b] = MIN (temp, 255);
            }
        }
      else
        {
          dest[b] = src[b];
        }
    }
}

/* Harness-only defined-domain guard; this is not historical validation. */
int video_old_oracle(int x,int y,const uint8_t *src,uint8_t *dest,int bpp,int p,int additive,int rotated){
 if(x<0||y<0||p<0||p>=9||(bpp!=3&&bpp!=4)||!src||!dest)return 0;
 vvals.pattern_number=p;vvals.additive=additive;vvals.rotated=rotated;
 video_func(x,y,src,dest,bpp,0);return 1;
}
