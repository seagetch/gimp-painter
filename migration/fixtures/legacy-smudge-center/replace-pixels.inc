inline void
replace_pixels (const guchar   *src1,
                const guchar   *src2,
                guchar         *dest,
                const guchar   *mask,
                gint            length,
                gint            opacity,
                const gboolean *affect,
                gint            bytes1,
                gint            bytes2)
{
  const gint    alpha        = bytes1 - 1;
  const gdouble norm_opacity = opacity * (1.0 / 65536.0);

  if (bytes1 != bytes2)
    {
      g_warning ("replace_pixels only works on commensurate pixel regions");
      return;
    }

  while (length --)
    {
      guint    b;
      gdouble  mask_val = mask[0] * norm_opacity;

      /* calculate new alpha first. */
      gint     s1_a  = src1[alpha];
      gint     s2_a  = src2[alpha];
      gdouble  a_val = s1_a + mask_val * (s2_a - s1_a);

      if (a_val == 0)
        /* In any case, write out versions of the blending function */
        /* that result when combinations of s1_a, s2_a, and         */
        /* mask_val --> 0 (or mask_val -->1)                        */
        {
          /* Case 1: s1_a, s2_a, AND mask_val all approach 0+:               */
          /* Case 2: s1_a AND s2_a both approach 0+, regardless of mask_val: */

          if (s1_a + s2_a == 0.0)
            {
              for (b = 0; b < alpha; b++)
                {
                  gint new_val = 0.5 + (gdouble) src1[b] +
                    mask_val * ((gdouble) src2[b] - (gdouble) src1[b]);

                  dest[b] = affect[b] ? MIN (new_val, 255) : src1[b];
                }
            }

          /* Case 3: mask_val AND s1_a both approach 0+, regardless of s2_a  */
          else if (s1_a + mask_val == 0.0)
            {
              for (b = 0; b < alpha; b++)
                dest[b] = src1[b];
            }

          /* Case 4: mask_val -->1 AND s2_a -->0, regardless of s1_a         */
          else if (1.0 - mask_val + s2_a == 0.0)
            {
              for (b = 0; b < alpha; b++)
                dest[b] = affect[b] ? src2[b] : src1[b];
            }
        }
      else
        {
          gdouble a_recip = 1.0 / a_val;
          /* possible optimization: fold a_recip into s1_a and s2_a           */
          for (b = 0; b < alpha; b++)
            {
              gint new_val = 0.5 + a_recip * (src1[b] * s1_a + mask_val *
                                              (src2[b] * s2_a - src1[b] * s1_a));
              dest[b] = affect[b] ? MIN (new_val, 255) : src1[b];
            }
        }

      dest[alpha] = affect[alpha] ? a_val + 0.5: s1_a;
      src1 += bytes1;
      src2 += bytes2;
      dest += bytes2;
      mask++;
    }
}
