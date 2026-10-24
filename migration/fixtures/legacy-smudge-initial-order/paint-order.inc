static void
gimp_smudge_paint (GimpPaintCore    *paint_core,
                   GimpDrawable     *drawable,
                   GimpPaintOptions *paint_options,
                   const GimpCoords *coords,
                   GimpPaintState    paint_state,
                   guint32           time)
{
  GimpSmudge *smudge = GIMP_SMUDGE (paint_core);

  switch (paint_state)
    {
    case GIMP_PAINT_STATE_MOTION:
      /* initialization fails if the user starts outside the drawable */
      if (! smudge->initialized)
        smudge->initialized = gimp_smudge_start (paint_core, drawable,
                                                 paint_options, coords);

      if (smudge->initialized)
        gimp_smudge_motion (paint_core, drawable, paint_options, coords);
      break;

    case GIMP_PAINT_STATE_FINISH:
      if (smudge->accum_data)
        {
          g_free (smudge->accum_data);
          smudge->accum_data = NULL;
        }
      if (smudge->blending_data)
        {
          g_free (smudge->blending_data);
          smudge->blending_data = NULL;
        }
      smudge->initialized = FALSE;
      break;

    default:
      break;
    }
}
