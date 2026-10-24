void
combine_inten_and_inten_a_pixels (const guchar   *src1,
                                  const guchar   *src2,
                                  guchar         *dest,
                                  const guchar   *mask,
                                  const guint     opacity,
                                  const gboolean *affect,
                                  guint           length,
                                  const guint     bytes)
{
  const gint alpha      = bytes;
  const gint src2_bytes = bytes + 1;

  if (mask)
    {
      const guchar *m = mask;

      while (length --)
        {
          register glong  t1;
          const guchar    new_alpha = INT_MULT3 (src2[alpha], *m, opacity, t1);
          guint           b;

          for (b = 0; b < bytes; b++)
            dest[b] = (affect[b] ?
                       INT_BLEND (src2[b], src1[b], new_alpha, t1) : src1[b]);

          m++;
          src1 += bytes;
          src2 += src2_bytes;
          dest += bytes;
        }
    }
  else
    {
      if (bytes == 3 && affect[0] && affect[1] && affect[2])
        {
          while (length --)
            {
              register glong  t1;
              const guchar    new_alpha = INT_MULT (src2[alpha], opacity, t1);

              dest[0] = INT_BLEND (src2[0], src1[0], new_alpha, t1);
              dest[1] = INT_BLEND (src2[1], src1[1], new_alpha, t1);
              dest[2] = INT_BLEND (src2[2], src1[2], new_alpha, t1);

              src1 += bytes;
              src2 += src2_bytes;
              dest += bytes;
            }
        }
      else
        {
          while (length --)
            {
              register glong  t1;
              const guchar    new_alpha = INT_MULT (src2[alpha], opacity, t1);
              guint           b;

              for (b = 0; b < bytes; b++)
                dest[b] = (affect[b] ?
                           INT_BLEND (src2[b], src1[b], new_alpha, t1) :
                           src1[b]);

              src1 += bytes;
              src2 += src2_bytes;
              dest += bytes;
            }
        }
    }
}
