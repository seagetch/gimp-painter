#include "gimp-features.h"
/* The native replacement operation has no entry before this task. The ensuing
 * implementation also requires behavioral replacement/freeze tests; this is
 * solely a public-entry compile red, not a runtime replacement oracle. */
int main (void) { return gimp_preset_catalog_register == NULL; }
