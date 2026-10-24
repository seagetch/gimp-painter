typedef struct
{
  guchar  lut[256];
  gdouble min;
  gdouble max;
  gint alpha;
  gboolean has_alpha;
} NormalizeParam_t;

static void
find_min_max (const guchar *src,
              gint          bpp,
              gpointer      data)
{
  NormalizeParam_t *param = (NormalizeParam_t*) data;
  gint              b;

  for (b = 0; b < param->alpha; b++)
    {
      if (!param->has_alpha || src[param->alpha])
        {
          if (src[b] < param->min)
            param->min = src[b];
          if (src[b] > param->max)
            param->max = src[b];
        }
    }
}

static void
normalize_func (const guchar *src,
                guchar       *dest,
                gint          bpp,
                gpointer      data)
{
  NormalizeParam_t *param = (NormalizeParam_t*) data;
  gint              b;

  for (b = 0; b < param->alpha; b++)
    dest[b] = param->lut[src[b]];

  if (param->has_alpha)
    dest[param->alpha] = src[param->alpha];
}

static void
normalize (GimpDrawable *drawable)
{
  NormalizeParam_t param;
  gint x;
  guchar  range;

  param.min = 255;
  param.max = 0;
  param.has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);
  param.alpha = (param.has_alpha) ? drawable->bpp - 1 : drawable->bpp;

  gimp_rgn_iterate1 (drawable, 0 /* unused */, find_min_max, &param);

  /* Calculate LUT */

  range = param.max - param.min;

  if (range != 0)
    for (x = param.min; x <= param.max; x++)
      param.lut[x] = 255 * (x - param.min) / range;
  else
    param.lut[(gint)param.min] = param.min;

  gimp_rgn_iterate2 (drawable, 0 /* unused */, normalize_func, &param);
}
