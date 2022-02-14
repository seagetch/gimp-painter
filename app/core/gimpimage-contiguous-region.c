/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>

#include <cairo.h>
#include <gegl.h>

#include "libgimpcolor/gimpcolor.h"

#include "core-types.h"

#include "base/pixel-processor.h"
#include "base/pixel-region.h"
#include "base/tile.h"
#include "base/tile-manager.h"

#include "gimpchannel.h"
#include "gimpimage.h"
#include "gimpimage-contiguous-region.h"
#include "gimppickable.h"


typedef struct
{
  GimpImage           *image;
  GimpImageType        type;
  gboolean             sample_merged;
  gboolean             antialias;
  gint                 threshold;
  gboolean             select_transparent;
  GimpSelectCriterion  select_criterion;
  gboolean             has_alpha;
  guchar               color[MAX_CHANNELS];
} ContinuousRegionData;


typedef struct {
  PixelRegion* pr1;
  PixelRegion* pr2;
  PixelRegion* pr3;
  gboolean    pr1_writable;
  gboolean    pr2_writable;
  gboolean    pr3_writable;
  gint origin_x;
  gint origin_y;
  gint cur_x;
  gint cur_y;
  gint cur_tile_width;
  gint min_x, min_y;
  gint max_x, max_y;
} PixelRegionIteratorX3;



/*  local function prototypes  */

static void contiguous_region_by_color    (ContinuousRegionData *cont,
                                           PixelRegion          *imagePR,
                                           PixelRegion          *maskPR);

static gint pixel_difference              (const guchar        *col1,
                                           const guchar        *col2,
                                           const guchar        *col3,
                                           gboolean             antialias,
                                           gint                 threshold,
                                           gint                 bytes,
                                           gboolean             has_alpha,
                                           gboolean             select_transparent,
                                           GimpSelectCriterion  select_criterion);
static gboolean find_contiguous_segment   (GimpImage           *image,
                                           const guchar        *col,
                                           PixelRegionIteratorX3* iter,
                                           GimpImageType        src_type,
                                           gboolean             has_alpha,
                                           gboolean             select_transparent,
                                           GimpSelectCriterion  select_criterion,
                                           gboolean             antialias,
                                           gint                 threshold,
                                           gint                 initial,
                                           gint                *start,
                                           gint                *end);
static void find_contiguous_region_helper (GimpImage           *image,
                                           PixelRegion         *mask,
                                           PixelRegion         *src,
                                           PixelRegion         *src_mask,
                                           GimpImageType        src_type,
                                           gboolean             has_alpha,
                                           gboolean             select_transparent,
                                           GimpSelectCriterion  select_criterion,
                                           gboolean             antialias,
                                           gint                 threshold,
                                           gint                 x,
                                           gint                 y,
                                           gint                 off_x,
                                           gint                 off_y,
                                           const guchar        *col);

static void
pixel_region_iterator_x_init(
  PixelRegionIteratorX3* iter, 
  PixelRegion* pr1, PixelRegion* pr2, PixelRegion* pr3, 
  gboolean pr1_writable, gboolean pr2_writable, gboolean pr3_writable,
  gint x, gint y, gint min_x, gint min_y, gint max_x, gint max_y)
{
  iter->pr1 = iter->pr2 = iter->pr3 = NULL;
  if (iter){
    if (pr1) {
      iter->pr1 = g_new0(PixelRegion, 1);
      pixel_region_duplicate(iter->pr1, pr1);
    }
    if (pr2) {
      iter->pr2 = g_new0(PixelRegion, 1);
      pixel_region_duplicate(iter->pr2, pr2);
    }
    if (pr3) {
      iter->pr3 = g_new0(PixelRegion, 1);
      pixel_region_duplicate(iter->pr3, pr3);
    }
    iter->pr1_writable = pr1_writable;
    iter->pr2_writable = pr2_writable;
    iter->pr3_writable = pr3_writable;
    iter->origin_x = iter->cur_x = x;
    iter->origin_y = iter->cur_y = y;
    iter->min_x = min_x;
    iter->max_x = max_x;
    iter->min_y = min_y;
    iter->max_y = max_y;
    iter->cur_tile_width  = 0;
  }
}

static void
pixel_region_iterator_x_cleanup(PixelRegionIteratorX3* iter)
{
  if (iter->pr1)
    g_free(iter->pr1);
  if (iter->pr2)
    g_free(iter->pr2);
  if (iter->pr3)
    g_free(iter->pr3);
}

static void
pixel_region_iterator_x_update(PixelRegionIteratorX3* iter)
{
  gint off_x = iter->cur_x - iter->min_x;
  gint off_y = iter->cur_y - iter->min_y;

  if (iter->pr1) {
    if (iter->pr1->curtile != NULL)
      tile_release (iter->pr1->curtile, iter->pr1_writable && iter->pr1->dirty);
    
    iter->pr1->offx    = (iter->pr1->x + off_x) % TILE_WIDTH;
    iter->pr1->offy    = (iter->pr1->y + off_y) % TILE_WIDTH;
    
    iter->pr1->curtile = tile_manager_get_tile (iter->pr1->tiles, iter->pr1->x + off_x, iter->pr1->y + off_y, TRUE, iter->pr1_writable);
    iter->pr1->data    = tile_data_pointer (iter->pr1->curtile, iter->pr1->offx, iter->pr1->offy);
    iter->pr1->dirty   = FALSE;
  }

  if (iter->pr2) {
    if (iter->pr2->curtile != NULL)
      tile_release (iter->pr2->curtile, iter->pr2_writable && iter->pr2->dirty);
    
    iter->pr2->offx    = (iter->pr2->x + off_x) % TILE_WIDTH;
    iter->pr2->offy    = (iter->pr2->y + off_y) % TILE_WIDTH;
    
    iter->pr2->curtile = tile_manager_get_tile (iter->pr2->tiles, iter->pr2->x + off_x, iter->pr2->y + off_y, TRUE, iter->pr2_writable);
    iter->pr2->data    = tile_data_pointer (iter->pr2->curtile, iter->pr2->offx, iter->pr2->offy);
    iter->pr2->dirty   = FALSE;
  }

  if (iter->pr3) {
    if (iter->pr3->curtile != NULL)
      tile_release (iter->pr3->curtile, iter->pr3_writable && iter->pr3->dirty);
    
    iter->pr3->offx    = (iter->pr3->x + off_x) % TILE_WIDTH;
    iter->pr3->offy    = (iter->pr3->y + off_y) % TILE_WIDTH;

    iter->pr3->curtile = tile_manager_get_tile (iter->pr3->tiles, iter->pr3->x + off_x, iter->pr3->y + off_y, TRUE, iter->pr3_writable);
    iter->pr3->data    = tile_data_pointer (iter->pr3->curtile, iter->pr3->offx, iter->pr3->offy);
    iter->pr3->dirty   = FALSE;
  }
};

static gboolean
pixel_region_iterator_x_next(PixelRegionIteratorX3* iter)
{
  gint off_x;
  gint pr1_width_fw, pr2_width_fw, pr3_width_fw;

  pr1_width_fw = pr2_width_fw = pr3_width_fw = TILE_WIDTH;

  iter->cur_x += iter->cur_tile_width;

  if (iter->cur_x >= iter->max_x)
    return FALSE;

  pixel_region_iterator_x_update(iter);
  off_x = iter->cur_x - iter->min_x;

  if (iter->pr1)
    pr1_width_fw = MIN(TILE_WIDTH - iter->pr1->offx, iter->pr1->w - off_x);
  if (iter->pr2)
    pr2_width_fw = MIN(TILE_WIDTH - iter->pr2->offx, iter->pr2->w - off_x);
  if (iter->pr3)
    pr3_width_fw = MIN(TILE_WIDTH - iter->pr3->offx, iter->pr3->w - off_x);

  iter->cur_tile_width = CLAMP(MIN(pr1_width_fw, MIN(pr2_width_fw, pr3_width_fw)), 0, TILE_WIDTH);
  return TRUE;
};

static gboolean
pixel_region_iterator_x_prev(PixelRegionIteratorX3* iter)
{
  gint off_x;
  gint pr1_width_bw, pr2_width_bw, pr3_width_bw;

  pr1_width_bw = pr2_width_bw = pr3_width_bw = TILE_WIDTH;

  off_x = iter->cur_x - iter->min_x;

  if (iter->pr1)
    pr1_width_bw = MIN(iter->pr1->offx > 0 ? iter->pr1->offx: TILE_WIDTH, off_x);
  if (iter->pr2)
    pr2_width_bw = MIN(iter->pr2->offx > 0 ? iter->pr2->offx: TILE_WIDTH, off_x);
  if (iter->pr3)
    pr3_width_bw = MIN(iter->pr3->offx > 0 ? iter->pr3->offx: TILE_WIDTH, off_x);

  iter->cur_tile_width = CLAMP(MIN(pr1_width_bw, MIN(pr2_width_bw, pr3_width_bw)), 0, TILE_WIDTH);

  iter->cur_x -= iter->cur_tile_width;

  if (iter->cur_x < iter->min_x) {
    g_print("prev: shoud not be happened.\n");
    return FALSE;
  }

  pixel_region_iterator_x_update(iter);
  return TRUE;
}


/*  public functions  */


GimpChannel *
gimp_image_contiguous_region_by_seed (GimpImage           *image,
                                      GimpDrawable        *drawable,
                                      gboolean             sample_merged,
                                      gboolean             antialias,
                                      gint                 threshold,
                                      gboolean             select_transparent,
                                      GimpSelectCriterion  select_criterion,
                                      gint                 x,
                                      gint                 y)
{
  return gimp_image_contiguous_region_by_seed_full (
    image, drawable, NULL, sample_merged, antialias, threshold,
    select_transparent, select_criterion, x, y
  );
}

// added by gimp-painter 2.8
GimpChannel *
gimp_image_contiguous_region_by_seed_full (GimpImage           *image,
                                           GimpDrawable        *drawable,
                                           GimpChannel         *source_mask,
                                           gboolean             sample_merged,
                                           gboolean             antialias,
                                           gint                 threshold,
                                           gboolean             select_transparent,
                                           GimpSelectCriterion  select_criterion,
                                           gint                 x,
                                           gint                 y)
{
  PixelRegion    srcPR, maskPR, src_mask_PR;
  GimpPickable  *pickable;
  TileManager   *tiles;
  GimpChannel   *mask;
  GimpImageType  src_type;
  gboolean       has_alpha;
  gint           bytes;
  Tile          *tile;
  gint           x1, y1, x2, y2;
  gint           off_x = 0, off_y = 0;

  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), NULL);

  if (sample_merged)
    pickable = GIMP_PICKABLE (gimp_image_get_projection (image));
  else
    pickable = GIMP_PICKABLE (drawable);

  gimp_pickable_flush (pickable);

  src_type  = gimp_pickable_get_image_type (pickable);
  has_alpha = GIMP_IMAGE_TYPE_HAS_ALPHA (src_type);
  bytes     = GIMP_IMAGE_TYPE_BYTES (src_type);

  tiles = gimp_pickable_get_tiles (pickable);

  if (source_mask) {

    gimp_channel_bounds (source_mask, &x1, &y1, &x2, &y2);
    if (GIMP_IS_DRAWABLE(pickable)) {
      gimp_item_get_offset (GIMP_ITEM(pickable), &off_x, &off_y);
    }
    if (x1 < off_x)
      x1 = off_x;
    if (y1 < off_y)
      y1 = off_y;
    if (x2 - off_x >= tile_manager_width (tiles))
      x2 = tile_manager_width (tiles);
    if (y2 - off_y >= tile_manager_height (tiles))
      y2 = tile_manager_height (tiles);
    pixel_region_init (&src_mask_PR, gimp_drawable_get_tiles (GIMP_DRAWABLE (source_mask)),
                       x1, y1, x2 - x1, y2 - y1, TRUE);

  } else {

    x1 = y1 = 0;
    x2 = tile_manager_width (tiles);
    y2 = tile_manager_height (tiles);

  }

  mask = gimp_channel_new_mask (image, tile_manager_width (tiles), tile_manager_height (tiles));
  if (x1 >= x2 || y1 >= y2) {
    return mask;
  }
  pixel_region_init (&maskPR, gimp_drawable_get_tiles (GIMP_DRAWABLE (mask)),
                     x1 - off_x, y1 - off_y, x2 - x1, y2 - y1, TRUE);

  pixel_region_init (&srcPR, tiles, x1 - off_x, y1 - off_y, x2 - x1, y2 - y1, FALSE);

  
  tile = tile_manager_get_tile (srcPR.tiles, x, y, TRUE, FALSE);
  if (tile)
    {
      const guchar *start;
      guchar        start_col[MAX_CHANNELS];

      start = tile_data_pointer (tile, x, y);

      if (has_alpha)
        {
          if (select_transparent)
            {
              /*  don't select transparent regions if the start pixel isn't
               *  fully transparent
               */
              if (start[bytes - 1] > 0)
                select_transparent = FALSE;
            }
        }
      else
        {
          select_transparent = FALSE;
        }

      if (GIMP_IMAGE_TYPE_IS_INDEXED (src_type))
        {
          gimp_image_get_color (image, src_type, start, start_col);
        }
      else
        {
          gint i;

          for (i = 0; i < bytes; i++)
            start_col[i] = start[i];
        }

      find_contiguous_region_helper (image, &maskPR, &srcPR, source_mask? &src_mask_PR: NULL,
                                     src_type, has_alpha,
                                     select_transparent, select_criterion,
                                     antialias, threshold,
                                     x, y, off_x, off_y, start_col);

      tile_release (tile, FALSE);
    }

  return mask;
}

GimpChannel *
gimp_image_contiguous_region_by_color (GimpImage            *image,
                                       GimpDrawable         *drawable,
                                       gboolean              sample_merged,
                                       gboolean              antialias,
                                       gint                  threshold,
                                       gboolean              select_transparent,
                                       GimpSelectCriterion  select_criterion,
                                       const GimpRGB        *color)
{
  /*  Scan over the image's active layer, finding pixels within the
   *  specified threshold from the given R, G, & B values.  If
   *  antialiasing is on, use the same antialiasing scheme as in
   *  fuzzy_select.  Modify the image's mask to reflect the
   *  additional selection
   */
  GimpPickable *pickable;
  TileManager  *tiles;
  GimpChannel  *mask;
  PixelRegion   imagePR, maskPR;
  gint          width, height;

  ContinuousRegionData  cont;

  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (color != NULL, NULL);

  gimp_rgba_get_uchar (color,
                       cont.color + 0,
                       cont.color + 1,
                       cont.color + 2,
                       cont.color + 3);

  if (sample_merged)
    pickable = GIMP_PICKABLE (gimp_image_get_projection (image));
  else
    pickable = GIMP_PICKABLE (drawable);

  gimp_pickable_flush (pickable);

  cont.type      = gimp_pickable_get_image_type (pickable);
  cont.has_alpha = GIMP_IMAGE_TYPE_HAS_ALPHA (cont.type);

  tiles  = gimp_pickable_get_tiles (pickable);
  width  = tile_manager_width (tiles);
  height = tile_manager_height (tiles);

  pixel_region_init (&imagePR, tiles, 0, 0, width, height, FALSE);

  if (cont.has_alpha)
    {
      if (select_transparent)
        {
          /*  don't select transparancy if "color" isn't fully transparent
           */
          if (cont.color[3] > 0)
            select_transparent = FALSE;
        }
    }
  else
    {
      select_transparent = FALSE;
    }

  cont.image              = image;
  cont.antialias          = antialias;
  cont.threshold          = threshold;
  cont.select_transparent = select_transparent;
  cont.select_criterion   = select_criterion;

  mask = gimp_channel_new_mask (image, width, height);

  pixel_region_init (&maskPR, gimp_drawable_get_tiles (GIMP_DRAWABLE (mask)),
                     0, 0, width, height,
                     TRUE);

  pixel_regions_process_parallel ((PixelProcessorFunc)
                                  contiguous_region_by_color, &cont,
                                  2, &imagePR, &maskPR);

  return mask;
}


/*  private functions  */

static void
contiguous_region_by_color (ContinuousRegionData *cont,
                            PixelRegion          *imagePR,
                            PixelRegion          *maskPR)
{
  const guchar *image = imagePR->data;
  guchar       *mask  = maskPR->data;
  gint          x, y;

  for (y = 0; y < imagePR->h; y++)
    {
      const guchar *i = image;
      guchar       *m = mask;

      for (x = 0; x < imagePR->w; x++)
        {
          guchar  rgb[MAX_CHANNELS];

          /*  Get the rgb values for the color  */
          gimp_image_get_color (cont->image, cont->type, i, rgb);

          /*  Find how closely the colors match  */
          *m++ = pixel_difference (cont->color, rgb, NULL,
                                   cont->antialias,
                                   cont->threshold,
                                   cont->has_alpha ? 4 : 3,
                                   cont->has_alpha,
                                   cont->select_transparent,
                                   cont->select_criterion);

          i += imagePR->bytes;
        }

      image += imagePR->rowstride;
      mask += maskPR->rowstride;
    }
}

// updated by gimp-painter 2.8
static gint
pixel_difference (const guchar        *col1,
                  const guchar        *col2,
                  const guchar        *src_mask, 
                  gboolean             antialias,
                  gint                 threshold,
                  gint                 bytes,
                  gboolean             has_alpha,
                  gboolean             select_transparent,
                  GimpSelectCriterion  select_criterion)
{
  gfloat max = 0;

  /*  if there is an alpha channel, never select transparent regions  */
  if (! select_transparent && has_alpha && col2[bytes - 1] == 0)
    return 0;

  if (select_transparent && has_alpha)
    {
      max = abs (col1[bytes - 1] - col2[bytes - 1]);
    }
  else
    {
      gint diff;
      gint b;
      gint av0, av1, av2;
      gint bv0, bv1, bv2;

      if (has_alpha)
        bytes--;

      switch (select_criterion)
        {
        case GIMP_SELECT_CRITERION_COMPOSITE:
          for (b = 0; b < bytes; b++)
            {
              diff = abs (col1[b] - col2[b]);
              if (diff > max)
                max = diff;
            }
          break;

        case GIMP_SELECT_CRITERION_R:
          max = abs (col1[0] - col2[0]);
          break;

        case GIMP_SELECT_CRITERION_G:
          max = abs (col1[1] - col2[1]);
          break;

        case GIMP_SELECT_CRITERION_B:
          max = abs (col1[2] - col2[2]);
          break;

        case GIMP_SELECT_CRITERION_H:
          av0 = (gint) col1[0];
          av1 = (gint) col1[1];
          av2 = (gint) col1[2];
          bv0 = (gint) col2[0];
          bv1 = (gint) col2[1];
          bv2 = (gint) col2[2];
          gimp_rgb_to_hsv_int (&av0, &av1, &av2);
          gimp_rgb_to_hsv_int (&bv0, &bv1, &bv2);
          /* wrap around candidates for the actual distance */
          {
            gint dist1 = abs (av0 - bv0);
            gint dist2 = abs (av0 - 360 - bv0);
            gint dist3 = abs (av0 - bv0 + 360);
            max = MIN (dist1, dist2);
            if (max > dist3)
              max = dist3;
          }
          break;

        case GIMP_SELECT_CRITERION_S:
          av0 = (gint) col1[0];
          av1 = (gint) col1[1];
          av2 = (gint) col1[2];
          bv0 = (gint) col2[0];
          bv1 = (gint) col2[1];
          bv2 = (gint) col2[2];
          gimp_rgb_to_hsv_int (&av0, &av1, &av2);
          gimp_rgb_to_hsv_int (&bv0, &bv1, &bv2);
          max = abs (av1 - bv1);
          break;

        case GIMP_SELECT_CRITERION_V:
          av0 = (gint) col1[0];
          av1 = (gint) col1[1];
          av2 = (gint) col1[2];
          bv0 = (gint) col2[0];
          bv1 = (gint) col2[1];
          bv2 = (gint) col2[2];
          gimp_rgb_to_hsv_int (&av0, &av1, &av2);
          gimp_rgb_to_hsv_int (&bv0, &bv1, &bv2);
          max = abs (av2 - bv2);
          break;
        }
    }

  if (src_mask) {
    max = (*src_mask * max + (255 - *src_mask) * 255) / 255;
  }

  if (antialias && threshold > 0)
    {
      gfloat aa = 1.5 - ( max / threshold);

      if (aa <= 0.0)
        return 0;
      else if (aa < 0.5)
        return (guchar) (aa * 512);
      else
        return 255;
    }
  else
    {
      if (max > threshold)
        return 0;
      else
        return 255;
    }
}

// updated by gimp-painter 2.8
static gboolean
find_contiguous_segment (GimpImage           *image,
                         const guchar        *col,
                         PixelRegionIteratorX3* iter,
                         GimpImageType        src_type,
                         gboolean             has_alpha,
                         gboolean             select_transparent,
                         GimpSelectCriterion  select_criterion,
                         gboolean             antialias,
                         gint                 threshold,
                         gint                 initial,
                         gint                *start,
                         gint                *end)
{
  guchar  s_color[MAX_CHANNELS];
  guchar  diff;
  gint    col_bytes = iter->pr1->bytes;
  gint    cur_tile_remained;

  pixel_region_iterator_x_update(iter);
  if (GIMP_IMAGE_TYPE_IS_INDEXED (src_type))
    {
      col_bytes = has_alpha ? 4 : 3;

      gimp_image_get_color (image, src_type, iter->pr1->data, s_color);

      diff = pixel_difference (col, s_color, iter->pr3? iter->pr3->data: NULL, antialias, threshold,
                               col_bytes, has_alpha, select_transparent,
                               select_criterion);
     }
  else
    {
      diff = pixel_difference (col, iter->pr1->data, iter->pr3? iter->pr3->data: NULL, antialias, threshold,
                               col_bytes, has_alpha, select_transparent,
                               select_criterion);
//      g_print("bytes=%d, mask.x=%d, mask.y=%d, mask=%d,diff=%d\n",iter->pr3? iter->pr3->bytes: 0, iter->pr3? iter->pr3->x:0, iter->pr3? iter->pr3->y:0, iter->pr3? *iter->pr3->data: 0, diff);
    }

  /* check the starting pixel */
  if (! diff)
    {
      if (iter->pr1->curtile)
        tile_release (iter->pr1->curtile, FALSE);
      if (iter->pr2->curtile)
        tile_release (iter->pr2->curtile, FALSE);
      if (iter->pr3 && iter->pr3->curtile)
        tile_release (iter->pr3->curtile, FALSE);
      return FALSE;
    }

  iter->pr2->dirty   = TRUE;
  *iter->pr2->data-- = diff;
  iter->pr1->data -= iter->pr1->bytes;
  *start = initial - 1;

  iter->cur_x = *start + 1;
  cur_tile_remained = iter->cur_tile_width = 0;

  while (*start >= iter->min_x && diff)
    {
      if (cur_tile_remained <= 0) {
        pixel_region_iterator_x_prev (iter);
        iter->pr1->data += iter->pr1->bytes * (*start - iter->cur_x);
        iter->pr2->data += iter->pr2->bytes * (*start - iter->cur_x);
        if (iter->pr3)
          iter->pr3->data += iter->pr3->bytes * (*start - iter->cur_x);
        cur_tile_remained = iter->cur_tile_width;
      }

      if (GIMP_IMAGE_TYPE_IS_INDEXED (src_type))
        {
          gimp_image_get_color (image, src_type, iter->pr1->data, s_color);

          diff = pixel_difference (col, s_color, iter->pr3? iter->pr3->data: NULL, antialias, threshold,
                                   col_bytes, has_alpha, select_transparent,
                                   select_criterion);
        }
      else
        {
          diff = pixel_difference (col, iter->pr1->data, iter->pr3? iter->pr3->data: NULL, antialias, threshold,
                                   col_bytes, has_alpha, select_transparent,
                                   select_criterion);
        }

      iter->pr2->dirty   = TRUE;
      if ((*iter->pr2->data-- = diff))
        {
          iter->pr1->data -= iter->pr1->bytes;
          iter->pr3->data -= iter->pr3->bytes;
          (*start)--;
          cur_tile_remained --;
        }
    }

  diff = 1;
  *end = initial + 1;
  cur_tile_remained = iter->cur_tile_width = 0;
  iter->cur_x = *end;

  pixel_region_iterator_x_update (iter);

  while (*end < iter->max_x && diff)
    {

      if (cur_tile_remained <= 0) {
        pixel_region_iterator_x_next(iter);
        iter->pr1->data += iter->pr1->bytes * (*end - iter->cur_x);
        iter->pr2->data += iter->pr2->bytes * (*end - iter->cur_x);
        if (iter->pr3)
          iter->pr3->data += iter->pr3->bytes * (*end - iter->cur_x);
        cur_tile_remained = iter->cur_tile_width;
      }

      if (GIMP_IMAGE_TYPE_IS_INDEXED (src_type))
        {
          gimp_image_get_color (image, src_type, iter->pr1->data, s_color);

          diff = pixel_difference (col, s_color, iter->pr3? iter->pr3->data: NULL, antialias, threshold,
                                   col_bytes, has_alpha, select_transparent,
                                   select_criterion);
        }
      else
        {
          diff = pixel_difference (col, iter->pr1->data, iter->pr3? iter->pr3->data: NULL, antialias, threshold,
                                   col_bytes, has_alpha, select_transparent,
                                   select_criterion);
        }

      iter->pr2->dirty   = TRUE;
      if ((*iter->pr2->data++ = diff))
        {
          iter->pr1->data += iter->pr1->bytes;
          iter->pr3->data += iter->pr3->bytes;
          (*end)++;
          cur_tile_remained --;
        }
    }

  if (iter->pr1->curtile)
    tile_release (iter->pr1->curtile, iter->pr1_writable);
  if (iter->pr2->curtile)
    tile_release (iter->pr2->curtile, iter->pr2_writable);
  if (iter->pr3 && iter->pr3->curtile)
    tile_release (iter->pr3->curtile, iter->pr3_writable);

  return TRUE;
}

// updated by gimp-painter 2.8
static void
find_contiguous_region_helper (GimpImage           *image,
                               PixelRegion         *mask,
                               PixelRegion         *src,
                               PixelRegion         *src_mask,
                               GimpImageType        src_type,
                               gboolean             has_alpha,
                               gboolean             select_transparent,
                               GimpSelectCriterion  select_criterion,
                               gboolean             antialias,
                               gint                 threshold,
                               gint                 x,
                               gint                 y,
                               gint                 off_x,
                               gint                 off_y,
                               const guchar        *col)
{
  gint    start, end;
  gint    new_start, new_end;
  gint    val;
  Tile   *tile;
  GQueue *coord_stack;

  coord_stack = g_queue_new ();

  /* To avoid excessive memory allocation (y, start, end) tuples are
   * stored in interleaved format:
   *
   * [y1] [start1] [end1] [y2] [start2] [end2]
   */
  g_queue_push_tail (coord_stack, GINT_TO_POINTER (y));
  g_queue_push_tail (coord_stack, GINT_TO_POINTER (x - 1));
  g_queue_push_tail (coord_stack, GINT_TO_POINTER (x + 1));

  do
    {
      y     = GPOINTER_TO_INT (g_queue_pop_head (coord_stack));
      start = GPOINTER_TO_INT (g_queue_pop_head (coord_stack));
      end   = GPOINTER_TO_INT (g_queue_pop_head (coord_stack));

      for (x = start + 1; x < end; x++)
        {
          PixelRegionIteratorX3 iter;
          tile = tile_manager_get_tile (mask->tiles, x, y, TRUE, FALSE);
          val = *(const guchar *) tile_data_pointer (tile, x, y);
          tile_release (tile, FALSE);
          if (val != 0)
            continue;

          pixel_region_iterator_x_init(&iter, src, mask, src_mask, FALSE, TRUE, FALSE, x, y, src->x, src->y, src->x + src->w, src->y + src->h);

          if (! find_contiguous_segment (image, col, &iter, src_type, has_alpha,
                                         select_transparent, select_criterion,
                                         antialias, threshold, x,
                                         &new_start, &new_end)) {
            pixel_region_iterator_x_cleanup (&iter);
            continue;
          }
          pixel_region_iterator_x_cleanup (&iter);
//          g_print("%d-%d\n",new_start, new_end);

          if (y + 1 < src->y + src->h)
            {
              g_queue_push_tail (coord_stack, GINT_TO_POINTER (y + 1));
              g_queue_push_tail (coord_stack, GINT_TO_POINTER (new_start));
              g_queue_push_tail (coord_stack, GINT_TO_POINTER (new_end));
            }

          if (y - 1 >= src->y)
            {
              g_queue_push_tail (coord_stack, GINT_TO_POINTER (y - 1));
              g_queue_push_tail (coord_stack, GINT_TO_POINTER (new_start));
              g_queue_push_tail (coord_stack, GINT_TO_POINTER (new_end));
            }
        }
    }
  while (! g_queue_is_empty (coord_stack));

  g_queue_free (coord_stack);
}
