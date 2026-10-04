#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include "../../../plug-ins/common/convolution-matrix-kernel.h"

static unsigned random_state = 0x415c9327u;
static unsigned next_value (void)
{
  random_state ^= random_state << 13;
  random_state ^= random_state >> 17;
  random_state ^= random_state << 5;
  return random_state;
}

static guchar old_pixel (const PainterConvolution *k, const guchar *rows[5],
                         gsize offset, int bpp)
{
  gfloat sum = 0;
  for (int y=0; y<5; y++)
    for (int x=0; x<5; x++)
      {
        gfloat temp = k->matrix_u8[x*5+y];
        temp *= rows[y][offset+x*bpp];
        sum += temp;
      }
  sum /= k->divisor_u8;
  sum += k->offset_u8;
  int rounded = (int) (sum + 0.5);
  return CLAMP (rounded, 0, 255);
}

int main (void)
{
  PainterConvolution k = {0};
  guchar data[5][36];
  const guchar *rows[5] = {data[0],data[1],data[2],data[3],data[4]};
  guchar out;
  size_t comparisons = 0;
  for (int trial=0; trial<10000; trial++)
    {
      for (int i=0;i<25;i++) k.matrix[i] = ((int)(next_value()%43)-21)/7.0;
      k.divisor = ((int)(next_value()%401)+1)/11.0;
      if (trial%2) k.divisor = -k.divisor;
      k.offset = ((int)(next_value()%2001)-1000)/13.0;
      assert (painter_convolution_validate (&k, TRUE));
      for (int i=0;i<5;i++)
        for (int j=0;j<36;j++) data[i][j] = next_value()%256;
      for (int bpp=1;bpp<=4;bpp++)
        for (int off=0;off<4*bpp;off++)
          {
            assert (painter_convolution_pixel_u8 (&k, rows, off, bpp, &out));
            assert (out == old_pixel (&k, rows, off, bpp));
            comparisons++;
          }
    }
  memset (&k,0,sizeof(k)); k.divisor=1; k.matrix[1*5+3]=1;
  assert (painter_convolution_validate (&k, TRUE));
  assert (painter_convolution_pixel_u8 (&k, rows, 2, 4, &out));
  assert (out==data[3][6]);
  k.divisor=0; assert (!painter_convolution_validate(&k,TRUE));
  k.divisor=DBL_MIN; assert (!painter_convolution_validate(&k,TRUE));
  k.divisor=1; k.matrix[0]=NAN; assert (!painter_convolution_validate(&k,TRUE));
  k.matrix[0]=DBL_MAX; assert (!painter_convolution_validate(&k,TRUE));
  memset (&k,0,sizeof(k)); k.divisor=1; k.offset=2147483500.0;
  assert (painter_convolution_validate(&k,TRUE));
  assert (painter_convolution_pixel_u8(&k,rows,0,1,&out) && out==255);
  k.offset=2147483648.0; assert (painter_convolution_validate(&k,TRUE));
  assert (!painter_convolution_pixel_u8(&k,rows,0,1,&out));
  memset (&k,0,sizeof(k)); k.divisor=FLT_MAX; k.matrix[0]=FLT_MAX;
  assert (painter_convolution_validate(&k,TRUE));
  data[0][0]=1; assert (painter_convolution_pixel_u8(&k,rows,0,1,&out) && out==1);
  data[0][0]=0; assert (painter_convolution_pixel_u8(&k,rows,0,1,&out) && out==0);
  data[0][0]=255; assert (!painter_convolution_pixel_u8(&k,rows,0,1,&out));

  gdouble native[5][36] = {{0}};
  const gdouble *native_rows[5] = {native[0],native[1],native[2],native[3],native[4]};
  gdouble result;
  memset (&k,0,sizeof(k)); k.divisor=1; k.matrix[12]=1;
  assert (painter_convolution_validate(&k,FALSE));
  const double values[] = {-3.25, 3.5, 1.0 + 0x1p-40, 0x1p-1022, -0x1p-1022};
  for (size_t i=0;i<G_N_ELEMENTS(values);i++)
    {
      native[2][8]=values[i];
      assert (painter_convolution_pixel_double(&k,native_rows,0,4,&result));
      assert (result==values[i]);
    }
  native[2][8]=2.5; k.offset=127.5;
  assert (painter_convolution_pixel_double(&k,native_rows,0,4,&result));
  assert (result==3.0);
  k.offset=0; k.matrix[12]=DBL_MAX;
  assert (painter_convolution_validate(&k,FALSE));
  assert (!painter_convolution_pixel_double(&k,native_rows,0,4,&result));
  k.matrix[12]=1; native[0][0]=NAN;
  assert (!painter_convolution_pixel_double(&k,native_rows,0,4,&result));
  printf("PASS %zu U8 old-arithmetic comparisons, x-major orientation, native precision/HDR, normalized offset, and invalid arithmetic\n",comparisons);
  return 0;
}
