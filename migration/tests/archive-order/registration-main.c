/* Illustrative fixture: a constructor in an unreferenced archive member is
 * not discovered by the linker, even if that archive is in a rescan group. */
int registration_count;
#ifdef EXPLICIT_ROOT
void fixture_registration_anchor (void);
#endif
int main (void)
{
#ifdef EXPLICIT_ROOT
  fixture_registration_anchor ();
#endif
  return registration_count == 1 ? 0 : 23;
}
