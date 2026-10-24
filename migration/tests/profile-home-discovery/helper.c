
/* Test-only introspection, appended privately; never a product API. */
void
painter_test_install_source (GimpUserInstall *install,
                             const gchar    **path,
                             gint            *major,
                             gint            *minor,
                             gboolean        *migrate)
{
  *path = install->old_dir;
  *major = install->old_major;
  *minor = install->old_minor;
  *migrate = install->migrate;
}
gboolean
painter_test_install_origin (GimpUserInstall *install)
{
  return install->painter_origin;
}
