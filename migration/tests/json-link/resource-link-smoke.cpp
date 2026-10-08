/* SPDX-License-Identifier: GPL-3.0-or-later
 * Link smoke only: use real Resource reader/writer and real GLib streams.
 * Legacy fixture parity and native layer-preset behavior have separate tests.
 */
#include "resource.hpp"

using GimpPainter::MyPaint::Resource;

int main ()
{
  auto original = Resource::decode (
    "{\"version\":3,\"vendor\":{\"retained\":[true,null,7]},"
    "\"settings\":{\"hardness\":{\"base_value\":0.25}}}");
  auto edited = original;
  edited.set_base_value (BRUSH_HARDNESS, 0.75);
  g_assert_cmpfloat (original.base_value (BRUSH_HARDNESS), ==, 0.25);

  GError *error = nullptr;
  auto *output = g_memory_output_stream_new_resizable ();
  g_assert_true (edited.save (output, nullptr, &error));
  g_assert_no_error (error);
  auto *input = g_memory_input_stream_new_from_data (
    g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output)),
    g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (output)), nullptr);
  auto loaded = Resource::load (input);
  g_assert_cmpfloat (loaded.base_value (BRUSH_HARDNESS), ==, 0.75);
  g_assert_true (loaded.has_member ("vendor", "retained"));
  g_object_unref (input);
  g_object_unref (output);

  auto legacy = Resource::decode ("version 2\nhardness 0.375\n");
  g_assert_cmpint (legacy.source_version (), ==, 2);
  auto migrated = Resource::decode (legacy.encode ());
  g_assert_cmpfloat (migrated.base_value (BRUSH_HARDNESS), ==, 0.375);
  g_assert_true (migrated.has_member ("painter_legacy", "source"));
  g_print ("PASS: production Resource JSON reader/writer and legacy-v2 link smoke\n");
}
