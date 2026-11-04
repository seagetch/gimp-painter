static int nested;
static void reenter(GObject *o,void *unused){(void)unused;if(!nested++){g_signal_emit_by_name(o,"check-resize");}nested--;}
int main(int argc, char **argv){
 /* Match the real test entrypoint: g_test_init installs a TAP print handler. */
 g_test_init(&argc, &argv, NULL);
 for(int i=0;i<20;i++)objects[i]=g_object_new(G_TYPE_OBJECT,NULL);
 for(int i=0;i<2;i++)clocks[i]=g_object_new(G_TYPE_OBJECT,NULL);
 assert(g_signal_new("check-resize",G_TYPE_OBJECT,2,0,NULL,NULL,NULL,G_TYPE_NONE,0));
 assert(g_signal_new("layout",G_TYPE_OBJECT,2,0,NULL,NULL,NULL,G_TYPE_NONE,0));
 owner_observe_begin(objects[0],objects[1]);
 assert(owner_observation.owners==16 && owner_observation.clipped && owner_observation.probes==18);
 assert(owner_observation.windows[0].role==1 && owner_observation.windows[1].role==2);
 assert(owner_observation.windows[1].clock_slot==owner_observation.windows[2].clock_slot);
 g_signal_connect_data(objects[0],"check-resize",G_CALLBACK(reenter),NULL,NULL,0);
 owner_observe_dispatch(TRUE);
 for(int i=0;i<30;i++)g_signal_emit_by_name(objects[0],"check-resize");
 g_signal_emit_by_name(clocks[0],"layout");g_signal_emit_by_name(clocks[1],"layout");
 owner_observe_dispatch(FALSE);
 assert(owner_observation.slots[0].entries==60 && owner_observation.slots[0].exits==60 && !owner_observation.slots[0].depth);
 /* Destroy observed owner and shared clock before finish: weak cleanup must not
  * dereference either. Their global hooks are still removed at finish. */
 g_object_unref(objects[3]);objects[3]=NULL;g_object_unref(clocks[1]);clocks[1]=NULL;
 assert(!owner_observation.slots[5].object);assert(!owner_observation.slots[3].object);
 owner_observe_finish();assert(owner_done==1);guint previous=owner_observation.events;
 g_signal_emit_by_name(objects[0],"check-resize");g_signal_emit_by_name(clocks[0],"layout");
 assert(owner_observation.events==previous);owner_observe_finish();
 owner_observe_begin(objects[0],objects[1]);assert(!owner_observation.active);
 for(int i=0;i<20;i++)if(objects[i])g_object_unref(objects[i]);g_object_unref(clocks[0]);
 fprintf(stderr,"PASS actual GObject signal hooks: nested enter/exit, shared clocks, bounded ring/windows, destroyed owner+clock, disconnect, one-shot\n");return 0;
}
