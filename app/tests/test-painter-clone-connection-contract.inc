/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Original14.013: observe real registrations, not just final cached pixels. */
static guint clone_connection_count(GObject *object,const gchar *name)
{
 guint id=g_signal_lookup(name,G_OBJECT_TYPE(object));
 guint count=g_signal_handlers_block_matched(object,G_SIGNAL_MATCH_ID,id,0,NULL,NULL,NULL);
 g_assert_cmpuint(g_signal_handlers_unblock_matched(object,G_SIGNAL_MATCH_ID,id,0,NULL,NULL,NULL),==,count);
 return count;
}
static void clone_connection_counts(GObject *object,const guint baseline[4],gboolean attached)
{
 const gchar *names[]={"update","name-changed","disconnect","notify"};
 const guint delta[]={1,1,1,2};
 for(guint i=0;i<4;++i)
  g_assert_cmpuint(clone_connection_count(object,names[i]),==,baseline[i]+(attached?delta[i]:0));
}
static void clone_source_connections_exact_once(void)
{
 const gchar *names[]={"update","name-changed","disconnect","notify"};
 GimpImage *image=new_image();GimpLayer *a=new_layer(image,"source A",NULL,0),*b=new_layer(image,"source B",NULL,0);
 guint baseline_a[4],baseline_b[4];gint updates=0;
 for(guint i=0;i<4;++i){baseline_a[i]=clone_connection_count(G_OBJECT(a),names[i]);baseline_b[i]=clone_connection_count(G_OBJECT(b),names[i]);}
 GimpCloneLayer *clone=new_clone(image,a);
 gulong handler=g_signal_connect(clone,"update",G_CALLBACK(count_source_update),&updates);
 for(guint repeat=0;repeat<3;++repeat){
  clone_connection_counts(G_OBJECT(a),baseline_a,TRUE);clone_connection_counts(G_OBJECT(b),baseline_b,FALSE);
  gimp_clone_layer_set_source(clone,b);
  clone_connection_counts(G_OBJECT(a),baseline_a,FALSE);clone_connection_counts(G_OBJECT(b),baseline_b,TRUE);
  gimp_clone_layer_set_source(clone,b); /* no duplicate subscriptions */
  clone_connection_counts(G_OBJECT(a),baseline_a,FALSE);clone_connection_counts(G_OBJECT(b),baseline_b,TRUE);
  updates=0;gimp_drawable_update(GIMP_DRAWABLE(a),0,0,1,1);g_assert_cmpint(updates,==,0);
  gimp_drawable_update(GIMP_DRAWABLE(b),0,0,1,1);g_assert_cmpint(updates,==,1);
  gimp_clone_layer_set_source(clone,a);
  clone_connection_counts(G_OBJECT(a),baseline_a,TRUE);clone_connection_counts(G_OBJECT(b),baseline_b,FALSE);
 }
 gimp_clone_layer_set_source(clone,NULL);
 clone_connection_counts(G_OBJECT(a),baseline_a,FALSE);clone_connection_counts(G_OBJECT(b),baseline_b,FALSE);
 updates=0;gimp_drawable_update(GIMP_DRAWABLE(a),0,0,1,1);gimp_drawable_update(GIMP_DRAWABLE(b),0,0,1,1);g_assert_cmpint(updates,==,0);
 g_signal_handler_disconnect(clone,handler);g_object_unref(image);
 g_test_message("3 A-to-B/same-B/to-A cycles and detach:104 handler cardinality checks;3 single live updates and retired-source silence");
}
