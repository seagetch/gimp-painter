/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Original15.012: restoring a deleted Filter reevaluates the latest lower
 * content, not the cached pixels captured before deletion. Gaussian only. */
static void
filter_gaussian_restore_latest_lower_contract (void)
{
  const guchar colors[3][4]={{0,255,0,255},{0,0,255,255},{255,255,0,255}};
  for(guint grouped=0;grouped<2;grouped++)
    {
      GimpImage *image=image_new(16,16);
      GimpLayer *parent=grouped?gimp_group_layer_new(image):NULL,*source;
      GimpContainer *stack;
      GimpFilterLayer *filter;
      guint handlers;
      guint64 definition;
      gint id;
      if(parent)g_assert_true(gimp_image_add_layer(image,parent,NULL,0,FALSE));
      source=source_new(image,parent,16,16);fill(source,255,0,0,255);
      stack=parent?gimp_viewable_get_children(GIMP_VIEWABLE(parent)):gimp_image_get_layers(image);
      handlers=signal_handler_count(stack,"reorder");
      filter=contract_gaussian_filter(image);
      if(parent)g_assert_true(gimp_image_reorder_item(image,GIMP_ITEM(filter),GIMP_ITEM(parent),0,FALSE,NULL));
      settle(filter);pixel(GIMP_LAYER(filter),255,0,0,255);
      definition=gimp_filter_layer_get_definition_revision(filter);
      id=gimp_item_get_id(GIMP_ITEM(filter));
      g_object_ref(filter);
      gimp_image_undo_free(image);
      gimp_image_remove_layer(image,GIMP_LAYER(filter),TRUE,NULL);
      for(guint cycle=0;cycle<3;cycle++)
        {
          guint64 runs=gimp_filter_layer_get_run_count(filter);
          g_assert_false(gimp_item_is_attached(GIMP_ITEM(filter)));
          /* Intentionally update the lower native buffer without adding a
           * second Undo item, leaving deletion as the next history action. */
          fill(source,colors[cycle][0],colors[cycle][1],colors[cycle][2],255);
          spin_ms(20);
          g_assert_cmpuint(gimp_filter_layer_get_cache_generation(filter),!=,
                           gimp_filter_layer_get_generation(filter));
          g_assert_true(gimp_image_undo(image));
          g_assert_true(gimp_item_is_attached(GIMP_ITEM(filter)));
          g_assert_true(gimp_item_get_parent(GIMP_ITEM(filter))==(parent?GIMP_ITEM(parent):NULL));
          g_assert_cmpint(gimp_item_get_id(GIMP_ITEM(filter)),==,id);
          g_assert_cmpuint(gimp_filter_layer_get_definition_revision(filter),==,definition);
          settle(filter);
          g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),>,runs);
          pixel(GIMP_LAYER(filter),colors[cycle][0],colors[cycle][1],colors[cycle][2],255);
          g_assert_cmpuint(signal_handler_count(stack,"reorder"),==,handlers+1);
          if(cycle<2)g_assert_true(gimp_image_redo(image));
        }
      g_object_unref(filter);g_object_unref(image);
    }
  g_test_message("root/group deletion history:6 restores after independent lower edits,4 redo deletions,8 exact pixels,identity/definition and6 stack handler counts preserved;Gaussian only");
}
