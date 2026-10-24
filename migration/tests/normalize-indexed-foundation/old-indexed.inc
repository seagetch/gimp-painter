static void
indexed_normalize (gint32 image_ID)  /* a.d.m. */
{
  guchar *cmap;
  gint ncols,i;
  gint hi=0,lo=255;

  cmap = gimp_image_get_colormap (image_ID, &ncols);

  if (cmap==NULL)
    {
      g_printerr ("normalize: cmap was NULL!  Quitting...\n");
      return;
    }

  for (i=0;i<ncols;i++)
    {
      if (cmap[i*3 +0] > hi) hi=cmap[i*3 +0];
      if (cmap[i*3 +1] > hi) hi=cmap[i*3 +1];
      if (cmap[i*3 +2] > hi) hi=cmap[i*3 +2];
      if (cmap[i*3 +0] < lo) lo=cmap[i*3 +0];
      if (cmap[i*3 +1] < lo) lo=cmap[i*3 +1];
      if (cmap[i*3 +2] < lo) lo=cmap[i*3 +2];
    }

  if (hi!=lo)
    for (i=0;i<ncols;i++)
      {
        cmap[i*3 +0] = (255 * (cmap[i*3 +0] - lo)) / (hi-lo);
        cmap[i*3 +1] = (255 * (cmap[i*3 +1] - lo)) / (hi-lo);
        cmap[i*3 +2] = (255 * (cmap[i*3 +2] - lo)) / (hi-lo);
      }

  gimp_image_set_colormap (image_ID, cmap, ncols);
}
