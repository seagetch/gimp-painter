static void oracle (const guchar *a,const guchar *b,guchar *d,guint opacity,guint mask,guint mode)
{
  const gboolean affect[4]={TRUE,TRUE,TRUE,TRUE};
  const guchar m=mask; const guchar *mp=mask==256?NULL:&m;
  guchar intermediate[4];
  if(mode==27||mode==29) {const guchar *swap=a;a=b;b=swap;}
  if(mode==23) erase_inten_pixels(a,b,d,mp,opacity,affect,1,4);
  else if(mode==25) anti_erase_inten_pixels(a,b,d,mp,opacity,affect,1,4);
  else {
    if(mode>=26) {
      GimpCompositeContext ctx={(guchar *)a,(guchar *)b,intermediate,1,0,0,0};
      if(mode>=28) gimp_composite_src_out_any_any_any_generic(&ctx);
      else gimp_composite_src_in_any_any_any_generic(&ctx);
      g_assert_cmpint(ctx.combine,==,REPLACE_INTEN); b=ctx.D;
    }
    replace_inten_pixels(a,b,d,mp,opacity,affect,1,4,4);
  }
}
static guint64 count;
static void compare (guchar *a,guchar *b,guint opacity,guint mask,guint mode,gboolean alias)
{
  guchar expected[4],actual[4],left[4],right[4];
  oracle(a,b,expected,opacity,mask,mode);
  gimp_painter_legacy_composite_u8(a,b,actual,opacity,mask,mode);
  if(memcmp(expected,actual,4))
    g_error("mode=%u opacity=%u mask=%u A=%u,%u,%u,%u B=%u,%u,%u,%u expected=%u,%u,%u,%u actual=%u,%u,%u,%u",mode,opacity,mask,a[0],a[1],a[2],a[3],b[0],b[1],b[2],b[3],expected[0],expected[1],expected[2],expected[3],actual[0],actual[1],actual[2],actual[3]);
  if(alias) {
    memcpy(left,a,4);memcpy(right,b,4);
    gimp_painter_legacy_composite_u8(left,right,left,opacity,mask,mode);
    g_assert_cmpmem(left,4,expected,4);
    memcpy(left,a,4);
    gimp_painter_legacy_composite_u8(left,right,right,opacity,mask,mode);
    g_assert_cmpmem(right,4,expected,4);
  }
  ++count;
}
int main(void)
{
  const guint opacity[]={0,1,2,63,127,128,129,254,255};
  const guint mask[]={0,1,127,128,254,255,256};
  guint32 random=0x718a50c3;
  for(guint mode=23;mode<=29;++mode) {
    guint64 start=count;
    for(guint aa=0;aa<256;++aa) for(guint ba=0;ba<256;++ba)
      for(guint o=0;o<G_N_ELEMENTS(opacity);++o) for(guint m=0;m<G_N_ELEMENTS(mask);++m) {
        guchar a[4]={0,255,127,aa}, b[4]={255,0,128,ba};
        compare(a,b,opacity[o],mask[m],mode,FALSE);
      }
    for(guint n=0;n<100000;++n) {
      guchar bytes[10];
      for(guint k=0;k<10;++k) {random=random*1664525u+1013904223u;bytes[k]=random>>24;}
      compare(bytes,bytes+4,bytes[8],n%17?bytes[9]:256,mode,TRUE);
    }
    printf("mode %u: %" G_GUINT64_FORMAT " comparisons passed (100000 include both output aliases)\n",mode,count-start);
  }
  printf("total: %" G_GUINT64_FORMAT " exact RGBA comparisons\n",count);
  return 0;
}
