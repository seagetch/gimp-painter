extern int registration_count;
static void __attribute__((constructor)) register_fixture (void)
{
  ++registration_count;
}
void fixture_registration_anchor (void) {}
