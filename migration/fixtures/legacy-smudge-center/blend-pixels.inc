inline void
blend_pixels (const guchar *src1,
              const guchar *src2,
              guchar       *dest,
              guchar        blend,
              guint         w,
              guint         bytes)
{
  if (HAS_ALPHA (bytes))
    {
      const guint blend1 = 255 - blend;
      const guint blend2 = blend + 1;
      const guint c      = bytes - 1;

      while (w--)
        {
          const gint  a1 = blend1 * src1[c];
          const gint  a2 = blend2 * src2[c];
          const gint  a  = a1 + a2;
          guint       b;

          if (!a)
            {
              for (b = 0; b < bytes; b++)
                dest[b] = 0;
            }
          else
            {
              for (b = 0; b < c; b++)
                dest[b] =
                  src1[b] + (src1[b] * a1 + src2[b] * a2 - a * src1[b]) / a;

              dest[c] = a >> 8;
            }

          src1 += bytes;
          src2 += bytes;
          dest += bytes;
        }
    }
  else
    {
      const guchar blend1 = 255 - blend;

      while (w--)
        {
          guint b;

          for (b = 0; b < bytes; b++)
            dest[b] =
              src1[b] + (src1[b] * blend1 + src2[b] * blend - src1[b] * 255) / 255;

          src1 += bytes;
          src2 += bytes;
          dest += bytes;
        }
    }
}
