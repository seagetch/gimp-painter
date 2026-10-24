/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Gaussian-only current ancestry/subscription contract. No filter_new() or
 * historical Edge execution, and no viewable/renderer provider modifications. */
static void
filter_gaussian_ancestry_contract (void)
{
  GimpImage *image=image_new (16,16);
  GimpLayer *groups[3]={NULL,gimp_group_layer_new(image),gimp_group_layer_new(image)};
  GimpLayer *sources[3];
  GimpContainer *stacks[3];
  guint baseline[3];
  const guint sequence[]={0,1,2,0,2,1,0};
  const guchar colors[3][4]={{255,0,0,255},{0,255,0,255},{0,0,255,255}};
  GimpFilterLayer *filter;
  sources[0]=source_new(image,NULL,16,16);
  for(guint i=1;i<3;i++)
    {
      g_assert_true(gimp_image_add_layer(image,groups[i],NULL,0,FALSE));
      sources[i]=source_new(image,groups[i],16,16);
    }
  for(guint i=0;i<3;i++)
    {
      fill(sources[i],colors[i][0],colors[i][1],colors[i][2],255);
      stacks[i]=i?gimp_viewable_get_children(GIMP_VIEWABLE(groups[i])):gimp_image_get_layers(image);
      baseline[i]=signal_handler_count(stacks[i],"reorder");
    }
  filter=contract_gaussian_filter(image);
  /* Keep both groups ABOVE a root Filter, so only the ordinary red source is
   * lower input; their descendants must not be subscribed as lower topology. */
  for(guint stage=0;stage<G_N_ELEMENTS(sequence);stage++)
    {
      guint current=sequence[stage];
      guint64 generation=gimp_filter_layer_get_generation(filter),runs;
      g_assert_true(gimp_image_reorder_item(image,GIMP_ITEM(filter),
                    current?GIMP_ITEM(groups[current]):NULL,current?0:2,FALSE,NULL));
      g_assert_true(gimp_item_get_parent(GIMP_ITEM(filter))==
                    (current?GIMP_ITEM(groups[current]):NULL));
      if(stage)g_assert_cmpuint(gimp_filter_layer_get_generation(filter),>,generation);
      settle(filter);
      pixel(GIMP_LAYER(filter),colors[current][0],colors[current][1],colors[current][2],255);
      for(guint i=0;i<3;i++)
        g_assert_cmpuint(signal_handler_count(stacks[i],"reorder"),==,baseline[i]+(i==current));
      runs=gimp_filter_layer_get_run_count(filter);
      generation=gimp_filter_layer_get_generation(filter);
      for(guint i=0;i<3;i++)if(i!=current)
        gimp_drawable_update(GIMP_DRAWABLE(sources[i]),0,0,16,16);
      spin_ms(40);
      g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,runs);
      g_assert_cmpuint(gimp_filter_layer_get_generation(filter),==,generation);
      gimp_drawable_update(GIMP_DRAWABLE(sources[current]),0,0,16,16);
      settle(filter);
      g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,runs+1);
      pixel(GIMP_LAYER(filter),colors[current][0],colors[current][1],colors[current][2],255);
    }
  g_assert_true(gimp_painter_binding_close(G_OBJECT(filter),NULL));
  for(guint i=0;i<3;i++)
    g_assert_cmpuint(signal_handler_count(stacks[i],"reorder"),==,baseline[i]);
  g_object_unref(image);
  g_test_message("seven root/A/B stages;24 exact stack reorder handler counts;14 derived pixels;14 retired/upper source updates ignored;7 active updates rerun once;Gaussian only");
}

static void
filter_gaussian_reentrant_ancestry_contract (void)
{
  GimpImage *image=image_new(16,16);
  GimpLayer *group=gimp_group_layer_new(image),*root_source,*group_source;
  GimpContainer *root=gimp_image_get_layers(image),*children;
  GimpFilterLayer *filter;
  guint root_baseline,child_baseline;
  guint64 runs;
  MoveFilterStack state={image,group,FALSE};
  root_source=source_new(image,NULL,16,16);fill(root_source,255,0,0,255);
  g_assert_true(gimp_image_add_layer(image,group,NULL,0,FALSE));
  group_source=source_new(image,group,16,16);fill(group_source,0,255,0,255);
  children=gimp_viewable_get_children(GIMP_VIEWABLE(group));
  root_baseline=signal_handler_count(root,"reorder");
  child_baseline=signal_handler_count(children,"reorder");
  filter=contract_gaussian_filter(image);settle(filter);
  g_signal_connect(filter,"filter-state-changed",G_CALLBACK(move_filter_during_topology),&state);
  /* Old refresh is synchronously interrupted by reparenting inside its own
   * invalidation callback. Only the NEW stack may retain registrations. */
  gimp_container_reorder(root,GIMP_OBJECT(filter),1);
  g_assert_true(state.called);
  g_assert_true(gimp_item_get_parent(GIMP_ITEM(filter))==GIMP_ITEM(group));
  settle(filter);pixel(GIMP_LAYER(filter),0,255,0,255);
  g_assert_cmpuint(signal_handler_count(root,"reorder"),==,root_baseline);
  g_assert_cmpuint(signal_handler_count(children,"reorder"),==,child_baseline+1);
  runs=gimp_filter_layer_get_run_count(filter);
  fill(root_source,0,0,255,255);spin_ms(40);
  g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,runs);
  fill(group_source,255,255,0,255);settle(filter);
  g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,runs+1);
  pixel(GIMP_LAYER(filter),255,255,0,255);
  g_assert_true(gimp_painter_binding_close(G_OBJECT(filter),NULL));
  g_assert_cmpuint(signal_handler_count(root,"reorder"),==,root_baseline);
  g_assert_cmpuint(signal_handler_count(children,"reorder"),==,child_baseline);
  g_object_unref(image);
  g_test_message("reentrant old-to-new stack refresh;4 exact handler counts;2 independent Gaussian pixels;retired source ignored/current source reruns once");
}
