/* Standalone backend probe; does not load or modify the GIMP3 port. */
#include <gegl.h>
#include <stdio.h>

int probe_init (void)
{
  gegl_init (NULL, NULL);
  g_object_set (gegl_config (), "use-opencl", FALSE, "threads", 1,
                "tile-cache-size", (guint64) 33554432, NULL);
  return gegl_has_operation ("gegl:convolution-matrix");
}

void probe_version (int *major, int *minor, int *micro)
{ gegl_get_version (major, minor, micro); }

/* mode 0: original encoded RGBA8 carrier; 1: linear RGBA binary64;
 * mode 2: nonlinear RGBA binary64. All carriers have straight alpha. */
int probe_convolve (int width, int height, int mode, int border,
                    const double *matrix, double divisor, double offset,
                    const int *channels, const void *input, void *output)
{
  const char *names[] = {"R'G'B'A u8", "RGBA double", "R'G'B'A double"};
  if (mode < 0 || mode > 2 || border < 0 || border > 2) return 0;
  const Babl *format = babl_format (names[mode]);
  const GeglRectangle extent = {0, 0, width, height};
  GeglBuffer *buffer = gegl_buffer_new (&extent, format);
  gegl_buffer_set (buffer, &extent, 0, format, input, GEGL_AUTO_ROWSTRIDE);
  GeglNode *graph = gegl_node_new ();
  GeglNode *source = gegl_node_new_child (graph, "operation", "gegl:buffer-source",
                                        "buffer", buffer, NULL);
  GeglAbyssPolicy policies[] = {GEGL_ABYSS_CLAMP, GEGL_ABYSS_LOOP, GEGL_ABYSS_NONE};
  GeglNode *operation = gegl_node_new_child (graph,
    "operation", "gegl:convolution-matrix", "normalize", FALSE,
    "alpha-weight", FALSE, "divisor", divisor, "offset", offset,
    "red", channels[0], "green", channels[1], "blue", channels[2],
    "alpha", channels[3], "border", policies[border], NULL);
  for (unsigned x = 0; x < 5; ++x)
    for (unsigned y = 0; y < 5; ++y)
      {
        char property[] = {(char) ('a' + x), (char) ('1' + y), 0};
        /* Old PDB matrix[x*5+y] -> GEGL matrix[x][y]. */
        gegl_node_set (operation, property, matrix[x * 5 + y], NULL);
      }
  gegl_node_link (source, operation);
  gegl_node_blit (operation, 1.0, &extent, format, output,
                  GEGL_AUTO_ROWSTRIDE, GEGL_BLIT_DEFAULT);
  g_object_unref (graph);
  g_object_unref (buffer);
  return 1;
}

void probe_exit (void) { gegl_exit (); }
