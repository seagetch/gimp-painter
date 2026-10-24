/* Exact function extracted from pinned original; no historical executable is run. */
static void
redeye_inner_loop (const guchar *src,
                   guchar       *dest,
                   gint          width,
                   gint          height,
                   gint          bpp,
                   gboolean      has_alpha,
                   gint          rowstride)
{
  const gint red   = 0;
  const gint green = 1;
  const gint blue  = 2;
  const gint alpha = 3;
  gint       x, y;

  for (y = 0; y < height; y++)
    {
      const guchar *s = src;
      guchar       *d = dest;

      for (x = 0; x < width; x++)
        {
          gint adjusted_red       = s[red] * RED_FACTOR;
          gint adjusted_green     = s[green] * GREEN_FACTOR;
          gint adjusted_blue      = s[blue] * BLUE_FACTOR;
          gint adjusted_threshold = (threshold - 50) * 2;

          if (adjusted_red >= adjusted_green - adjusted_threshold &&
              adjusted_red >= adjusted_blue - adjusted_threshold)
            {
              d[red] = CLAMP (((gdouble) (adjusted_green + adjusted_blue)
                        / (2.0  * RED_FACTOR)), 0, 255);
            }
          else
            {
              d[red] = s[red];
            }

          d[green] = s[green];
          d[blue] = s[blue];

          if (has_alpha)
            d[alpha] = s[alpha];

          s += bpp;
          d += bpp;
        }

      src += rowstride;
      dest += rowstride;
    }
}
