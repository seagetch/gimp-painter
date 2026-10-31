#define _GNU_SOURCE
#include <babl/babl.h>
#include <link.h>
#include <stdio.h>
/* Enumerate while extensions are still resident. Do not retain their addresses. */
static int inventory(struct dl_phdr_info *info, size_t size, void *data)
{
  (void)size; (void)data;
  if (info->dlpi_name && info->dlpi_name[0])
    printf("LOADED\t%s\n", info->dlpi_name);
  return 0;
}
int main(void)
{
  babl_init();
  puts("babl_init completed");
  dl_iterate_phdr(inventory, NULL);
  babl_exit();
  puts("babl_exit completed");
  return fflush(stdout) == 0 ? 0 : 74;
}
