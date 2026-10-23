#ifndef __GIMPMYPAINTCORE_DRAWABLEFEATURE_HPP__
#define __GIMPMYPAINTCORE_DRAWABLEFEATURE_HPP__
////////////////////////////////////////////////////////////////////////////////
class GeneralDrawableFeature {
protected:
  PixelRegion*
  get_tiles_region(TileManager* tiles,
                   gint x, gint y, gint w, gint h, bool writable) {
    PixelRegion* result = NULL; 
    if (tiles) {
      result = g_new(PixelRegion, 1);
      pixel_region_init (result, tiles, x, y, w, h, writable? TRUE: FALSE);
    }
    return result;
  }
  PixelRegion*
  get_temp_buf_region(TempBuf* temp_buf, gint x, gint y, gint w, gint h) {
    PixelRegion* result = NULL; 
    if (temp_buf) {
      result = g_new(PixelRegion, 1);
      pixel_region_init_temp_buf(result, temp_buf, x, y, w, h); 
    }
    return result;
  }
public:  
  void refresh() {};
  void get_drawable_offset(gint& offset_x, gint& offset_y) {
    offset_x = offset_y = 0;
  };
  void update_drawable(gint x, gint y, gint w, gint h) { };
  gint get_drawable_width() { return 0; }
  gint get_drawable_height() { return 0; }
  PixelRegion* 
  get_drawable_region(gint x, gint y, gint w, gint h, bool writable) {
    return NULL;
  };

  bool has_mask_item() { return false; }
  gint get_mask_width() { return 0; }
  gint get_mask_height() { return 0; }  
  PixelRegion* 
  get_mask_region(gint x, gint y, gint w, gint h, bool writable) {
    return NULL;
  };

  void start_undo_group() {};
  void stop_undo_group() {};
  void validate_undo_tiles(gint x, gint y, gint w, gint h) {};
  gint get_undo_width() { return 0; }
  gint get_undo_height() { return 0; }  
  PixelRegion* 
  get_undo_region(gint x, gint y, gint w, gint h, bool writable) {
    return NULL;
  };
  
  void start_floating_stroke() {};
  void stop_floating_stroke() {};
  gint get_floating_stroke_width() { return 0; }
  gint get_floating_stroke_height() { return 0; }  
  void validate_floating_stroke_tiles(gint x, gint y, gint w, gint h) {};
  PixelRegion* 
  get_floating_stroke_region(gint x, gint y, gint w, gint h, bool writable) {
    return NULL;
  };
};

////////////////////////////////////////////////////////////////////////////////
class GimpImageFeature : public GeneralDrawableFeature {
  GimpDrawable* drawable;
  TileManager*  undo_tiles;
  TileManager*  floating_stroke_tiles;
  gint          x1, y1;           /*  undo extents in image coords        */
  gint          x2, y2;           /*  undo extents in image coords        */  
  GimpItem    *drawable_item;
  GimpImage   *image;
  GimpChannel *mask;
  GimpItem    *mask_item;

  GimpUndo *
  push_undo (GimpImage* image, const gchar* undo_desc)
  {
    gimp_image_undo_group_start (image, GIMP_UNDO_GROUP_MYPAINT,
                                 undo_desc);

    gimp_image_undo_push (image, GIMP_TYPE_MYPAINT_CORE_UNDO,
                                 GIMP_UNDO_PAINT, NULL,
                                 GimpDirtyMask(0),
                                 NULL);
    if (undo_tiles) {
      gimp_image_undo_push_drawable (image, "Mypaint Brush",
                                     drawable, undo_tiles,
                                     TRUE, x1, y1, x2 - x1, y2 - y1);
    }
    gimp_image_undo_group_end (image);

    tile_manager_unref (undo_tiles);
    undo_tiles = NULL;

    return NULL;
  }
  
public:
  typedef GimpDrawable* Drawable;
  
  GimpImageFeature(GimpDrawable* d) : undo_tiles(0), floating_stroke_tiles(0),
    drawable(d), image(0), mask(0), mask_item(0), GeneralDrawableFeature() 
  {
    g_object_add_weak_pointer(G_OBJECT(d), (gpointer*)&drawable);
  };

  ~GimpImageFeature() {
    if (drawable)
      g_object_remove_weak_pointer(G_OBJECT(drawable), (gpointer*)&drawable);
    if (undo_tiles) {
      tile_manager_unref (undo_tiles);
      undo_tiles = NULL;
    }
    if (floating_stroke_tiles) {
      tile_manager_unref (floating_stroke_tiles);
      floating_stroke_tiles = NULL;
    }
  };
  
  void refresh() {
    drawable_item = GIMP_ITEM (drawable);
    image         = gimp_item_get_image (drawable_item);
    mask          = gimp_image_get_mask (image);
    mask_item     = (mask && !gimp_channel_is_empty(GIMP_CHANNEL(mask)))?
                      GIMP_ITEM (mask): NULL;
  };

  bool is_surface_for (Drawable drawable) { return drawable == this->drawable; }

  void get_drawable_offset(gint& offset_x, gint& offset_y) {
    gimp_item_get_offset (drawable_item, &offset_x, &offset_y);
  };

  void update_drawable(gint x, gint y, gint w, gint h) { 
    /*  Update the drawable  */
    gimp_drawable_update (drawable, x, y, w, h);
    if (x     < this->x1) this->x1 = x;
    if (y     < this->y1) this->y1 = y;
    if (x + w > this->x2) this->x2 = x + w;
    if (y + h > this->y2) this->y2 = y + h;
  };
  
  gint get_drawable_width() {
    return gimp_item_get_width(drawable_item);
  }

  gint get_drawable_height() {
    return gimp_item_get_height(drawable_item);
  }
  
  PixelRegion* 
  get_drawable_region(gint x, gint y, gint w, gint h, bool writable) {
    TileManager* tiles = gimp_drawable_get_tiles(drawable);
    return get_tiles_region(tiles, x, y, w, h, writable);
  };

  bool has_mask_item() {
    return mask_item;
  }
  
  gint get_mask_width() {
    return (mask_item)? gimp_item_get_width(mask_item) : 0;
  }

  gint get_mask_height() {
    return (mask_item)? gimp_item_get_height(mask_item) : 0;
  }
  
  PixelRegion* 
  get_mask_region(gint x, gint y, gint w, gint h, bool writable) {
    TileManager *tiles = NULL;
    if (mask_item)
      tiles = gimp_drawable_get_tiles(GIMP_DRAWABLE(mask));

    return get_tiles_region(tiles, x, y, w, h, writable);
  };
  
  void start_undo_group() {
    g_return_if_fail (gimp_item_is_attached (GIMP_ITEM (drawable)));
    g_print("Stroke::begin_session:: do begining of session...\n");
  //  g_return_if_fail (coords != NULL);
  //  g_return_if_fail (error == NULL || *error == NULL);

    /*  Allocate the undo structure  */
    if (undo_tiles)
      tile_manager_unref (undo_tiles);

    undo_tiles = tile_manager_new (get_drawable_width(),
                                   get_drawable_height(),
                                   gimp_drawable_bytes (drawable));

    /*  Get the initial undo extents  */
    x1 = get_drawable_width() + 1;
    y1 = get_drawable_height() + 1;
    x2 = -1;
    y2 = -1;

    /*  Freeze the drawable preview so that it isn't constantly updated.  */
    gimp_viewable_preview_freeze (GIMP_VIEWABLE (drawable));

    return;
  };
  
  void stop_undo_group() {
    GimpImage *image;

    g_return_if_fail (GIMP_IS_DRAWABLE (drawable));
    g_return_if_fail (gimp_item_is_attached (GIMP_ITEM (drawable)));

    image = gimp_item_get_image (GIMP_ITEM (drawable));

    /*  Determine if any part of the image has been altered--
     *  if nothing has, then just return...
     */
    if (x2 < x1 || y2 < y1) {
      gimp_viewable_preview_thaw (GIMP_VIEWABLE (drawable));
      return;
    }

    g_print("Stroke::end_session::push_undo(%d,%d)-(%d,%d)\n",x1,y1,x2,y2);
    push_undo (image, "Mypaint Brush");

    gimp_viewable_preview_thaw (GIMP_VIEWABLE (drawable));

    return;
  };
  
  void validate_undo_tiles(gint x, gint y, gint w, gint h) {
    gint i, j;
    GimpItem* item = GIMP_ITEM (drawable);

    g_return_if_fail (GIMP_IS_DRAWABLE (drawable));
    g_return_if_fail (undo_tiles != NULL);
    
    if (x + w > gimp_item_get_width(item))
      w = MAX(0, gimp_item_get_width(item) - x);
    if (y + h > gimp_item_get_height(item))
      h = MAX(0, gimp_item_get_height(item) - y);
    
    for (i = y; i < (y + h); i += (TILE_HEIGHT - (i % TILE_HEIGHT))) {
      for (j = x; j < (x + w); j += (TILE_WIDTH - (j % TILE_WIDTH))) {
        Tile *dest_tile = tile_manager_get_tile (undo_tiles, j, i, FALSE, FALSE);
        assert(j< gimp_item_get_width(item));
        assert(i< gimp_item_get_height(item));

        if (! tile_is_valid (dest_tile)) {
          Tile *src_tile =
            tile_manager_get_tile (gimp_drawable_get_tiles (drawable),
                                   j, i, TRUE, FALSE);
          tile_manager_map_tile (undo_tiles, j, i, src_tile);
          tile_release (src_tile, FALSE);

        }
      }
    }
  };

  gint get_undo_width() {
    return (undo_tiles)? tile_manager_width(undo_tiles) : 0;
  }

  gint get_undo_height() {
    return (undo_tiles)? tile_manager_height(undo_tiles) : 0;
  }
  
  PixelRegion* 
  get_undo_region(gint x, gint y, gint w, gint h, bool writable) {
    return get_tiles_region(undo_tiles, x, y, w, h, writable);
  };

  
  void start_floating_stroke() {
    g_return_if_fail(drawable);

    if (floating_stroke_tiles) {
      tile_manager_unref(floating_stroke_tiles);
      floating_stroke_tiles = NULL;
    }

    gint bytes = gimp_drawable_bytes_with_alpha (drawable);
    floating_stroke_tiles = tile_manager_new(get_drawable_width(),
                                             get_drawable_height(),
                                             bytes);
  };
  
  void stop_floating_stroke() {
    if (floating_stroke_tiles) {
      tile_manager_unref(floating_stroke_tiles);
      floating_stroke_tiles = NULL;
    }
  };
  
  void validate_floating_stroke_tiles(gint x, gint y, gint w, gint h) {
    gint i, j;

    g_return_if_fail (floating_stroke_tiles != NULL);

    for (i = y; i < (y + h); i += (TILE_HEIGHT - (i % TILE_HEIGHT))) {
      for (j = x; j < (x + w); j += (TILE_WIDTH - (j % TILE_WIDTH))) {
        Tile *tile = tile_manager_get_tile (floating_stroke_tiles, j, i,
                                            FALSE, FALSE);

        if (! tile_is_valid (tile)) {
          tile = tile_manager_get_tile (floating_stroke_tiles, j, i,
                                        TRUE, TRUE);
          memset (tile_data_pointer (tile, 0, 0), 0, tile_size (tile));
          tile_release (tile, TRUE);
        }
        
      }
    }
  };

  gint get_floating_stroke_width() {
    return (floating_stroke_tiles)? 
      tile_manager_width(floating_stroke_tiles) : 0;
  }

  gint get_floating_stroke_height() {
    return (floating_stroke_tiles)? 
      tile_manager_height(floating_stroke_tiles) : 0;
  }
  
  PixelRegion* 
  get_floating_stroke_region(gint x, gint y, gint w, gint h, bool writable) {
    return get_tiles_region(floating_stroke_tiles, x, y, w, h, writable);
  };
};

////////////////////////////////////////////////////////////////////////////////
class GimpTempBufFeature : public GeneralDrawableFeature {
  TempBuf *drawable, *undo, *floating;

public:
  typedef TempBuf* Drawable;
  
  GimpTempBufFeature(TempBuf* buf) : undo(0), floating(0),
    drawable(buf), GeneralDrawableFeature() 
  {
  };

  ~GimpTempBufFeature() {
    if (undo) {
      temp_buf_free(undo);
      undo = NULL;
    }
    if (floating) {
      temp_buf_free(floating);
      floating = NULL;
    }
  };
  
  bool is_surface_for (GimpDrawable* drawable) { return false; }

  gint get_drawable_width() {
    return (drawable)? drawable->width: 0;
  }

  gint get_drawable_height() {
    return (drawable)? drawable->height: 0;
  }
  
  PixelRegion* 
  get_drawable_region(gint x, gint y, gint w, gint h, bool writable) {
    return get_temp_buf_region(drawable, x, y, w, h);
  };
  
  void start_undo_group() {
    if (!drawable)
      return;
    guchar color[] = {0, 0, 0, 0};
    undo = temp_buf_new(drawable->width, drawable->height, drawable->bytes, 0, 0, color);
    temp_buf_copy(drawable, undo);
  };
  
  void stop_undo_group() {
    temp_buf_free(undo);
    undo = NULL;
  };
  
  void validate_undo_tiles(gint x, gint y, gint w, gint h) { };

  gint get_undo_width() {
    return (undo)? undo->width : 0;
  }

  gint get_undo_height() {
    return (undo)? undo->height : 0;
  }
  
  PixelRegion* 
  get_undo_region(gint x, gint y, gint w, gint h, bool writable) {
    return get_temp_buf_region(undo, x, y, w, h);
  };

  
  void start_floating_stroke() {
    if (!drawable)
      return;
    guchar color[] = {0, 0, 0, 0};
    floating = temp_buf_new(drawable->width, drawable->height, drawable->bytes, 0, 0, color);
  };
  
  void stop_floating_stroke() {
    temp_buf_free(floating);
    floating = NULL;
  };
  
  void validate_floating_stroke_tiles(gint x, gint y, gint w, gint h) { };

  gint get_floating_stroke_width() {
    return (floating)? floating->width : 0;
  }

  gint get_floating_stroke_height() {
    return (floating)? floating->height : 0;
  }
  
  PixelRegion* 
  get_floating_stroke_region(gint x, gint y, gint w, gint h, bool writable) {
    return get_temp_buf_region(floating, x, y, w, h);
  };
};

#endif