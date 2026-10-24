static void
exchange (GimpDrawable *drawable,
          GimpPreview  *preview)
{
  GimpPixelRgn  srcPR, destPR;
  guchar        min_red,  min_green,  min_blue;
  guchar        max_red,  max_green,  max_blue;
  guchar        from_red, from_green, from_blue;
  guchar        to_red,   to_green,   to_blue;
  guchar       *src_row, *dest_row;
  gint          x, y, bpp = drawable->bpp;
  gboolean      has_alpha;
  gint          x1, y1, y2;
  gint          width, height;
  GimpRGB       min;
  GimpRGB       max;

  if (preview)
    {
      gimp_preview_get_position (preview, &x1, &y1);
      gimp_preview_get_size (preview, &width, &height);
    }
  else if (! gimp_drawable_mask_intersect (drawable->drawable_id,
                                           &x1, &y1, &width, &height))
    {
      return;
    }

  y2 = y1 + height;

  has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);
  /* allocate memory */
  src_row = g_new (guchar, drawable->width * bpp);

  gimp_rgb_get_uchar (&xargs.from, &from_red, &from_green, &from_blue);
  gimp_rgb_get_uchar (&xargs.to,   &to_red,   &to_green,   &to_blue);

  /* get boundary values */
  min = xargs.from;
  gimp_rgb_subtract (&min, &xargs.threshold);
  gimp_rgb_clamp (&min);
  gimp_rgb_get_uchar (&min, &min_red, &min_green, &min_blue);

  max = xargs.from;
  gimp_rgb_add (&max, &xargs.threshold);
  gimp_rgb_clamp (&max);
  gimp_rgb_get_uchar (&max, &max_red, &max_green, &max_blue);

  dest_row = g_new (guchar, drawable->width * bpp);

  gimp_pixel_rgn_init (&srcPR, drawable,
                       x1, y1, width, height, FALSE, FALSE);
  gimp_pixel_rgn_init (&destPR, drawable,
                       x1, y1, width, height, (preview == NULL), TRUE);

  for (y = y1; y < y2; y++)
    {
      gimp_pixel_rgn_get_row (&srcPR, src_row, x1, y, width);

      for (x = 0; x < width; x++)
        {
          guchar pixel_red, pixel_green, pixel_blue;
          guchar new_red, new_green, new_blue;
          guint  idx;

          /* get current pixel-values */
          pixel_red   = src_row[x * bpp];
          pixel_green = src_row[x * bpp + 1];
          pixel_blue  = src_row[x * bpp + 2];

          idx = x * bpp;

          /* want this pixel? */
          if (pixel_red >= min_red &&
              pixel_red <= max_red &&
              pixel_green >= min_green &&
              pixel_green <= max_green &&
              pixel_blue >= min_blue &&
              pixel_blue <= max_blue)
            {
              guchar red_delta, green_delta, blue_delta;

              red_delta   = pixel_red > from_red ?
                pixel_red - from_red : from_red - pixel_red;
              green_delta = pixel_green > from_green ?
                pixel_green - from_green : from_green - pixel_green;
              blue_delta  = pixel_blue > from_blue ?
                pixel_blue - from_blue : from_blue - pixel_blue;

              new_red   = CLAMP (to_red   + red_delta,   0, 255);
              new_green = CLAMP (to_green + green_delta, 0, 255);
              new_blue  = CLAMP (to_blue  + blue_delta,  0, 255);
            }
          else
            {
              new_red   = pixel_red;
              new_green = pixel_green;
              new_blue  = pixel_blue;
            }

          /* fill buffer */
          dest_row[idx + 0] = new_red;
          dest_row[idx + 1] = new_green;
          dest_row[idx + 2] = new_blue;

          /* copy alpha-channel */
          if (has_alpha)
            dest_row[idx + 3] = src_row[x * bpp + 3];
        }
      /* store the dest */
      gimp_pixel_rgn_set_row (&destPR, dest_row, x1, y, width);

      /* and tell the user what we're doing */
      if (!preview && (y % 10) == 0)
        gimp_progress_update ((gdouble) y / (gdouble) height);
    }

  g_free (src_row);
  g_free (dest_row);

  if (preview)
    {
      gimp_drawable_preview_draw_region (GIMP_DRAWABLE_PREVIEW (preview),
                                         &destPR);
    }
  else
    {
      gimp_progress_update (1.0);
      /* update the processed region */
      gimp_drawable_flush (drawable);
      gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
      gimp_drawable_update (drawable->drawable_id, x1, y1, width, height);
    }
}
