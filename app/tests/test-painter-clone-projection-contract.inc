/* SPDX-License-Identifier: GPL-3.0-or-later */
static void clone_contract_projection_pixel(GimpImage *image,gint x,gint y,const guchar expected[4])
{
 guchar actual[4];gimp_pickable_flush(GIMP_PICKABLE(image));
 gegl_buffer_get(gimp_pickable_get_buffer(GIMP_PICKABLE(image)),GEGL_RECTANGLE(x,y,1,1),1,babl_format("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
 g_assert_cmpmem(actual,4,expected,4);
}
static void clone_projection_roi_offset_detach(void)
{
 GimpImage *image=new_image();GimpLayer *source=new_layer(image,"source",NULL,0);GimpCloneLayer *clone;
 const guchar blue[]={0,0,255,255},red[]={255,0,0,255},clear[]={0,0,0,0};
 fill(source,0,0,255,255);gimp_item_set_offset(GIMP_ITEM(source),4,6);
 clone=new_clone(image,source);gimp_item_set_offset(GIMP_ITEM(clone),20,10);
 gimp_item_set_visible(GIMP_ITEM(source),FALSE,FALSE);
 clone_contract_projection_pixel(image,21,11,blue);
 gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),GEGL_RECTANGLE(1,1,1,1),0,babl_format("R'G'B'A u8"),red,GEGL_AUTO_ROWSTRIDE);
 gimp_drawable_update(GIMP_DRAWABLE(source),1,1,1,1);
 clone_contract_projection_pixel(image,21,11,red);clone_contract_projection_pixel(image,20,10,blue);
 gimp_item_translate(GIMP_ITEM(clone),3,2,FALSE);
 clone_contract_projection_pixel(image,24,13,red);clone_contract_projection_pixel(image,20,10,clear);
 gimp_clone_layer_set_source(clone,NULL);fill(source,0,255,0,255);
 g_assert_null(gimp_clone_layer_get_source(clone));
 /* Pinned legacy detach retains the last cache, not transparent replacement. */
 clone_contract_projection_pixel(image,24,13,red);clone_contract_projection_pixel(image,23,12,blue);
 g_object_unref(image);
}
