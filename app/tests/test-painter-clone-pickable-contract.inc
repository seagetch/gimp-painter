/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/gimpimage-pick-item.h"
static void clone_pickable_projection_contract(void)
{
 for(guint grouped=0;grouped<2;++grouped){
  GimpImage *image=new_image();GimpLayer *base=new_layer(image,"pick base",NULL,0),*source,*paint;GimpCloneLayer *clone;
  const guchar red[]={255,0,0,255},green[]={0,255,0,255},blue[]={0,0,255,255};
  if(grouped){source=gimp_group_layer_new(image);g_assert_true(gimp_image_add_layer(image,source,NULL,0,FALSE));paint=new_layer(image,"referenced child",source,0);}
  else source=paint=new_layer(image,"hidden source",NULL,0);
  fill(base,0,255,0,255);fill(paint,255,0,0,255);clone=new_clone(image,source);
  gimp_item_set_visible(GIMP_ITEM(source),FALSE,FALSE);
  for(gint y=-1;y<=16;++y)for(gint x=-1;x<=16;++x)
   g_assert_cmpfloat(gimp_pickable_get_opacity_at(GIMP_PICKABLE(clone),x,y),==,0.0);
  clone_contract_projection_pixel(image,4,4,red);
  g_assert_true(gimp_image_pick_layer(image,4,4,NULL)==base);
  g_assert_true(gimp_image_pick_layer(image,4,4,GIMP_LAYER(clone))==base);
  g_assert_true(gimp_image_pick_layer_by_bounds(image,4,4)==GIMP_LAYER(clone));
  fill(paint,0,0,255,255);clone_contract_projection_pixel(image,4,4,blue);
  gimp_item_set_visible(GIMP_ITEM(clone),FALSE,FALSE);clone_contract_projection_pixel(image,4,4,green);
  g_object_unref(image);
 }
 g_test_message("ordinary/group source:648 zero-opacity queries; referenced child remains rendered/updated; pixel-pick excludes clone, bounds pick includes it");
}
