/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <stdio.h>
#include "core/core-types.h"
#include "base/base-types.h"
#include "base/tile-manager.h"
#include "base/pixel-region.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpdrawable.h"
#include "core/gimpchannel.h"
#include "core/gimpimage-contiguous-region.h"
#include "tests.h"
#include "stimuli.h"
static void dump(int id,const char*phase,GimpChannel*channel)
{
  guchar result[FILL_WIDTH*FILL_HEIGHT];
  tile_manager_read_pixel_data(gimp_drawable_get_tiles(GIMP_DRAWABLE(channel)),0,0,FILL_WIDTH-1,FILL_HEIGHT-1,result,FILL_WIDTH);
  printf("MASK %d %s ",id,phase);for(unsigned i=0;i<sizeof result;++i)printf("%02x",result[i]);puts("");
}
int main(void)
{
  Gimp*gimp=gimp_init_for_testing();
  for(int id=0;id<FILL_CASES;++id){
    guchar pixels[FILL_WIDTH*FILL_HEIGHT*4],mask_pixels[FILL_WIDTH*FILL_HEIGHT];
    struct FillStimulus s=fill_stimulus(id,pixels,mask_pixels);
    GimpImage*image=gimp_image_new(gimp,FILL_WIDTH,FILL_HEIGHT,s.bytes<=2?GIMP_GRAY:GIMP_RGB);
    GimpImageType type=s.bytes==1?GIMP_GRAY_IMAGE:s.bytes==2?GIMP_GRAYA_IMAGE:s.bytes==3?GIMP_RGB_IMAGE:GIMP_RGBA_IMAGE;
    TileManager*src=tile_manager_new(FILL_WIDTH,FILL_HEIGHT,s.bytes),*mask=tile_manager_new(FILL_WIDTH,FILL_HEIGHT,1);
    tile_manager_write_pixel_data(src,0,0,FILL_WIDTH-1,FILL_HEIGHT-1,pixels,FILL_WIDTH*s.bytes);
    tile_manager_write_pixel_data(mask,0,0,FILL_WIDTH-1,FILL_HEIGHT-1,mask_pixels,FILL_WIDTH);
    PixelRegion srcPR,maskPR;pixel_region_init(&srcPR,src,s.x1,s.y1,s.x2-s.x1,s.y2-s.y1,FALSE);pixel_region_init(&maskPR,mask,s.x1,s.y1,s.x2-s.x1,s.y2-s.y1,FALSE);
    GimpChannel*result=gimp_image_contiguous_region_by_seed_full(image,&srcPR,0,0,s.use_mask?&maskPR:NULL,0,0,s.x1,s.y1,s.x2,s.y2,type,s.alpha,s.bytes,s.antialias,s.threshold,s.transparent,GIMP_SELECT_CRITERION_COMPOSITE,s.seed_x,s.seed_y,s.color);
    dump(id,"search",result);gimp_channel_grow(result,1,1,FALSE);dump(id,"grow",result);
    g_object_ref_sink(result);g_object_unref(result);tile_manager_unref(src);tile_manager_unref(mask);g_object_unref(image);
  }
  puts("FILL_CAPTURE_COMPLETE");return 0;
}
