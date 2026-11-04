static void one(GtkWidget *w) { IconCall c = icon_observe_enter(w); ++originals; icon_observe_exit(c); }
int main(int argc, char **argv)
{
  const char *mode = argc > 1 ? argv[1] : "normal";
  setup("normal"); image_observe_begin();
  if (!strcmp(mode,"setup-disabled")) image_observation.enabled=FALSE;
  if (!strcmp(mode,"setup-property")) G_OBJECT_CLASS(g_type_class_peek(image_type))->get_property=NULL;
  icon_observe_begin(); icon_observe_begin();
  if (!strncmp(mode,"setup-",6)) { assert(!icon_observation.enabled); one(first_image); icon_observe_finish(); image_observe_finish(); return 0; }
  assert(icon_observation.enabled);
  if (!strcmp(mode,"normal")) {
    one(first_image); one(internal_image);
    internal_image->name="OTHER_SECRET"; one(internal_image);
    internal_image->name="SECRET_TEST_ICON"; internal_image->pixel=99; one(internal_image);
    internal_image->fallback=1; one(internal_image);
    internal_image->direction=2; one(internal_image);
    GtkWidget *unknown=new_widget(image_type); one(unknown); g_object_unref(unknown);
    assert(icon_observation.names==2 && icon_observation.groups==6);
  } else if (!strcmp(mode,"strings")) {
    char name[129]; memset(name,'x',sizeof name); name[127]=0;
    first_image->name=name; one(first_image); name[127]='x'; name[128]=0; one(first_image);
    first_image->name=NULL; one(first_image); first_image->storage=GTK_IMAGE_PIXBUF; one(first_image);
    first_image->storage=GTK_IMAGE_ICON_NAME; first_image->name=""; one(first_image);
    assert(icon_observation.names==2 && icon_observation.groups==5);
  } else if (!strcmp(mode,"dictionary")) {
    char name[32]; for(guint i=0;i<258;++i) { snprintf(name,sizeof name,"SECRET_%u",i); first_image->name=name; one(first_image); }
    first_image->name="SECRET_0"; one(first_image);
    assert(icon_observation.names==256 && icon_observation.groups==128 && icon_observation.overflow.entries==130);
  } else if (!strcmp(mode,"groups")) {
    for(guint i=0;i<130;++i) { first_image->pixel=i; one(first_image); }
    first_image->pixel=0; one(first_image);
    assert(icon_observation.groups==128 && icon_observation.overflow.entries==2);
  } else if (!strcmp(mode,"nested")) {
    IconCall a=icon_observe_enter(first_image), b=icon_observe_enter(first_image);
    icon_observe_exit(b); icon_observe_exit(a);
    assert(icon_observation.group[0].metric.nested==1);
    assert(icon_observation.group[0].metric.total>icon_observation.group[0].metric.longest);
  } else if (!strcmp(mode,"destroy")) {
    IconCall a=icon_observe_enter(first_image);
    g_signal_emit_by_name(first_image,"destroy"); g_object_unref(first_image); forwarding_destroyed=TRUE;
    icon_observe_exit(a); forwarding_destroyed=FALSE;
    guint cleared=0; for(guint i=0;i<image_observation.images;++i) cleared += !image_observation.identities[i].object; assert(cleared==1);
  } else if (!strcmp(mode,"clock")) {
    IconCall a=icon_observe_enter(first_image); fake_time=-10; icon_observe_exit(a);
  } else if (!strcmp(mode,"duration")) {
    IconCall a=icon_observe_enter(first_image); icon_observation.group[0].metric.total=INT64_MAX; icon_observe_exit(a);
  } else if (!strcmp(mode,"counter")) {
    icon_observation.entries=IMAGE_COUNT_CAP; one(first_image); assert(icon_observation.dropped==1);
  } else if (!strcmp(mode,"unmatched")) {
    IconCall a=icon_observe_enter(first_image); icon_observe_exit(a); icon_observe_exit(a);
  } else if (!strcmp(mode,"inflight")) {
    (void)icon_observe_enter(first_image);
  } else if (!strcmp(mode,"invalid")) {
    first_image->scale=0; one(first_image);
  } else if (!strcmp(mode,"subtype")) {
    GType subtype=new_type("SyntheticSubclass", image_type); GtkWidget *w=new_widget(subtype); one(w); g_object_unref(w);
  } else return 2;
  icon_observe_finish(); icon_observe_finish();
  for(guint i=0;i<ICON_NAME_CAP;++i) for(guint j=0;j<ICON_BYTE_CAP;++j) assert(!icon_observation.dictionary[i][j]);
  /* exercise unused source interfaces while keeping their output separate */
  image_observation.active=FALSE; ImageCall unused=image_observe_enter(NULL); image_observe_exit(unused);
  image_observe_finish();
  return 0;
}
