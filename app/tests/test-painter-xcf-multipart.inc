/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native multipart storage and ordinary public Save/Open acceptance. */
static GBytes *multipart_opaque_record (gsize payload_size)
{
  GError *error = NULL;
  GFileIOStream *io;
  GFile *file = g_file_new_tmp ("painter-large-record-XXXXXX", &io, &error);
  GOutputStream *output;
  guint32 header[] = {GUINT32_TO_BE (0x7ffffff0u), GUINT32_TO_BE (payload_size)};
  guint8 block[65536];
  gchar *name;
  GInputStream *input;
  GBytes *bytes;
  g_assert_no_error (error);
  output = g_io_stream_get_output_stream (G_IO_STREAM (io));
  for (gsize i = 0; i < sizeof block; ++i) block[i] = (i * 17 + i / 257) & 255;
  g_assert_true (g_output_stream_write_all (output, header, sizeof header, NULL, NULL, &error));
  for (gsize at = 0; at < payload_size; at += sizeof block)
    g_assert_true (g_output_stream_write_all (output, block, MIN (sizeof block, payload_size - at), NULL, NULL, &error));
  g_assert_true (g_io_stream_close (G_IO_STREAM (io), NULL, &error));
  g_assert_no_error (error); g_object_unref (io);
  name = g_file_get_path (file); g_free (name);
  input = G_INPUT_STREAM (g_file_read (file, NULL, &error)); g_assert_no_error (error);
  bytes = xcf_painter_snapshot_bytes (input, NULL, &error); g_assert_no_error (error); g_assert_nonnull (bytes);
  g_input_stream_close (input, NULL, NULL); g_object_unref (input);
  g_assert_true (g_file_delete (file, NULL, &error)); g_assert_no_error (error); g_object_unref (file);
  return bytes;
}
static gchar *multipart_digest (GBytes *bytes)
{
  gsize size;
  const guint8 *data = g_bytes_get_data (bytes, &size);
  GChecksum *sum = g_checksum_new (G_CHECKSUM_SHA256);
  gchar *result;
  for (gsize offset = 0; offset < size; offset += 65536)
    {
      gsize part = MIN ((gsize)65536, size - offset);
      g_checksum_update (sum, data + offset, part);
      xcf_painter_discard_snapshot_pages (data + offset, part);
    }
  result = g_strdup (g_checksum_get_string (sum)); g_checksum_free (sum); return result;
}
static void multipart_check_archive (GObject *owner, guint space, const gchar *expected, gsize expected_size)
{
  GimpPainterProvenanceTransport *transport = gimp_painter_provenance_ref_transport (owner);
  GBytes *body, *actual;
  GVariant *dict, *sequences, *record;
  gchar *digest;
  gsize size;
  g_assert_nonnull (transport); g_assert_true (transport->present[space]); g_assert_false (transport->invalid[space]);
  g_assert_nonnull (transport->capsules[space]); size = g_bytes_get_size (transport->capsules[space]);
  body = g_bytes_new_from_bytes (transport->capsules[space], 12, size - 12);
  dict = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE_VARDICT, body, FALSE));
  g_bytes_unref (body); g_assert_true (g_variant_is_normal_form (dict));
  sequences = g_variant_lookup_value (dict, "opaque-record-sequences", G_VARIANT_TYPE ("aay"));
  g_assert_nonnull (sequences); g_assert_cmpuint (g_variant_n_children (sequences), ==, 1);
  record = g_variant_get_child_value (sequences, 0); actual = g_variant_get_data_as_bytes (record);
  g_assert_cmpuint (g_bytes_get_size (actual), ==, expected_size); digest = multipart_digest (actual);
  g_assert_cmpstr (digest, ==, expected); g_free (digest); g_bytes_unref (actual);
  g_variant_unref (record); g_variant_unref (sequences); g_variant_unref (dict);
  gimp_painter_provenance_free_transport (transport);
}
static goffset multipart_file_size (GFile *file)
{
  GFileInfo *info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
  goffset size;
  g_assert_nonnull (info); size = g_file_info_get_size (info); g_object_unref (info); return size;
}
static void multipart_roundtrip_size (gsize size, gboolean item_too)
{
  guint live_before = xcf_painter_storage_live_files ();
  GimpImage *image = fixture ("legacy-runtime/clone-normal-in-group.xcf");
  GFile *file = temporary_file ();
  GBytes *record = multipart_opaque_record (size);
  GPtrArray *records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  gchar *digest = multipart_digest (record);
  GError *error = NULL;
  goffset first_size = 0;
  gint64 started = g_get_monotonic_time ();
  if (size > 256u * 1024u * 1024u)
    {
      GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image, 2, 2, "large metadata filter", 1, GIMP_LAYER_MODE_NORMAL));
      GBytes *raw = g_bytes_new_static ("filter raw provenance", 21);
      GimpValueArray *args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 17, G_TYPE_DOUBLE, 3.25, G_TYPE_NONE);
      gimp_image_add_layer (image, GIMP_LAYER (filter), NULL, 0, FALSE);
      g_assert_true (gimp_filter_layer_set_definition (filter, "unavailable-multipart-filter", raw, args, NULL));
      gimp_filter_layer_mark_as_loaded (filter);
      g_bytes_unref (raw); gimp_value_array_unref (args);
      /* Establish this newly created Filter's first import provenance before
       * testing repeated large saves. Its first Open supplies original native
       * properties that cannot exist in a never-saved native object. */
      save (image, file);
      {
        GimpImage *copy = load (file); g_object_unref (image); image = copy;
      }
    }
  g_ptr_array_add (records, record);
  g_assert_true (gimp_painter_provenance_set_records (G_OBJECT (image), records));
  if (item_too) g_assert_true (gimp_painter_provenance_set_records (G_OBJECT (layer (image, "clone")), records));
  g_ptr_array_unref (records);
  for (guint pass = 0; pass < 3; ++pass)
    {
      GimpImage *copy;
      save (image, file);
      if (!pass) first_size = multipart_file_size (file);
      else g_assert_cmpint (multipart_file_size (file), ==, first_size);
      copy = load (file); g_object_unref (image); image = copy;
      multipart_check_archive (G_OBJECT (image), GIMP_PAINTER_PROVENANCE_NAMESPACE_IMAGE, digest, size + 8);
      if (item_too) multipart_check_archive (G_OBJECT (layer (image, "clone")), GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM, digest, size + 8);
      g_assert_true (GIMP_IS_CLONE_LAYER (layer (image, "clone")));
      g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (layer (image, "clone"))) == layer (image, "source child"));
      if (size > 256u * 1024u * 1024u)
        {
          GimpFilterLayer *filter = GIMP_FILTER_LAYER (layer (image, "large metadata filter"));
          GBytes *raw; GimpValueArray *args;
          g_assert_true (GIMP_IS_FILTER_LAYER (filter));
          raw = gimp_filter_layer_ref_definition (filter);
          g_assert_cmpmem (g_bytes_get_data (raw, NULL), g_bytes_get_size (raw), "filter raw provenance", 21);
          args = gimp_filter_layer_dup_args (filter);
          g_assert_cmpuint (gimp_value_array_length (args), ==, 2);
          g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 0)), ==, 17);
          g_assert_cmpfloat (g_value_get_double (gimp_value_array_index (args, 1)), ==, 3.25);
          g_bytes_unref (raw); gimp_value_array_unref (args);
        }
    }
  g_test_message ("multipart public-pdb payload=%" G_GSIZE_FORMAT " file=%" G_GOFFSET_FORMAT " three-save-open-us=%" G_GINT64_FORMAT,
                  size, first_size, g_get_monotonic_time () - started);
  g_free (digest); g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
  g_assert_cmpuint (xcf_painter_storage_live_files (), ==, live_before);
}
static void multipart_native_small (void) { multipart_roundtrip_size (2 * 1024 * 1024 + 1, TRUE); }
static void multipart_native_large (void)
{
  if (!g_test_slow ()) { g_test_skip ("Run explicitly with -m slow for >256 MiB Save/Open resource evidence"); return; }
  const gchar *requested = g_getenv ("GIMP_TEST_XCF_MULTIPART_MIB");
  guint64 mib = requested ? g_ascii_strtoull (requested, NULL, 10) : 256;
  g_assert_cmpuint (mib, >=, 256); g_assert_cmpuint (mib, <=, 2048);
  multipart_roundtrip_size ((gsize) mib * 1024 * 1024 + 1, FALSE);
}
static void multipart_native_cancellation (void)
{
  guint live_before = xcf_painter_storage_live_files ();
  GimpImage *image = fixture ("legacy-runtime/clone-normal-in-group.xcf");
  GFile *file = temporary_file ();
  GBytes *record = multipart_opaque_record (2 * 1024 * 1024 + 1);
  GPtrArray *records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  guint32 id = gimp_painter_provenance_get_save_id (G_OBJECT (layer (image, "clone")));
  g_ptr_array_add (records, record); g_assert_true (gimp_painter_provenance_set_records (G_OBJECT (image), records)); g_ptr_array_unref (records);
  for (guint point = 0; point < 3; ++point)
    {
      SaveProgress *progress = g_object_new (save_progress_get_type (), NULL);
      GError *error = NULL;
      if (point == 0) progress->cancel_at = 0;
      if (point == 1) progress->cancel_at = 1;
      if (point == 2) progress->close_cancel = TRUE;
      sentinel_write (file);
      g_assert_false (xcf_save_file_with_painter_transport (gimp, image, file, GIMP_PROGRESS (progress), TRUE, NULL, &error));
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED); g_clear_error (&error);
      sentinel_check (file); g_assert_true (progress->ended);
      g_assert_cmpuint (gimp_painter_provenance_get_save_id (G_OBJECT (layer (image, "clone"))), ==, id);
      g_object_unref (progress);
    }
  g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
  g_assert_cmpuint (xcf_painter_storage_live_files (), ==, live_before);
}

/* Read-only scalar borrows stay alive through current-definition replacement. */
static void multipart_argument_snapshot_borrow (void)
{
  GimpImage *image = gimp_image_new (gimp, 2, 2, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image, 2, 2, "snapshot borrow", 1, GIMP_LAYER_MODE_NORMAL));
  GimpValueArray *args = gimp_value_array_new_from_types (NULL, G_TYPE_STRING, "immutable original", G_TYPE_INT, 17, G_TYPE_NONE);
  GimpFilterArgumentsSnapshot *snapshot;
  const GValue *value;
  gimp_image_add_layer (image, GIMP_LAYER (filter), NULL, 0, FALSE);
  g_assert_true (gimp_filter_layer_set_definition (filter, "unavailable", NULL, args, NULL));
  gimp_value_array_unref (args);
  snapshot = gimp_filter_layer_snapshot_arguments (filter);
  g_assert_nonnull (snapshot);
  value = gimp_filter_arguments_snapshot_peek_value (snapshot, 0);
  g_assert_nonnull (value);
  g_assert_true (value == gimp_filter_arguments_snapshot_peek_value (snapshot, 0));
  g_assert_null (gimp_filter_arguments_snapshot_peek_value (NULL, 0));
  g_assert_null (gimp_filter_arguments_snapshot_peek_value (snapshot, 2));
  g_assert_true (gimp_filter_layer_set_definition (filter, "replacement", NULL, NULL, NULL));
  g_object_unref (image);
  g_assert_cmpstr (g_value_get_string (value), ==, "immutable original");
  g_assert_cmpint (g_value_get_int (gimp_filter_arguments_snapshot_peek_value (snapshot, 1)), ==, 17);
  gimp_filter_arguments_snapshot_free (snapshot);
}

typedef struct { GCancellable *cancel; guint before; gboolean observed; } MultipartCancel;
static gpointer multipart_cancel_preparation (gpointer opaque)
{
  MultipartCancel *state = opaque;
  const gint64 deadline = g_get_monotonic_time () + 5000000;
  while (xcf_painter_storage_live_files () <= state->before &&
         g_get_monotonic_time () < deadline) g_thread_yield ();
  state->observed = xcf_painter_storage_live_files () > state->before;
  g_cancellable_cancel (state->cancel);
  return NULL;
}
static void multipart_native_midprepare_cancel (void)
{
  GimpImage *image = fixture ("legacy-runtime/clone-normal-in-group.xcf");
  GFile *file = temporary_file ();
  GBytes *record = multipart_opaque_record (8 * 1024 * 1024 + 1);
  GPtrArray *records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  GCancellable *cancel = g_cancellable_new ();
  MultipartCancel state = {cancel, xcf_painter_storage_live_files (), FALSE};
  GError *error = NULL;
  GThread *thread;
  guint32 saved_id = gimp_painter_provenance_get_save_id (G_OBJECT (layer (image, "clone")));
  gint dirty = gimp_image_is_dirty (image);
  g_ptr_array_add (records, record);
  g_assert_true (gimp_painter_provenance_set_records (G_OBJECT (image), records));
  g_ptr_array_unref (records); sentinel_write (file);
  thread = g_thread_new ("cancel-xcf-prepare", multipart_cancel_preparation, &state);
  g_assert_false (xcf_save_file_with_painter_transport (gimp, image, file, NULL, TRUE, cancel, &error));
  g_thread_join (thread);
  g_assert_true (state.observed);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED); g_clear_error (&error);
  sentinel_check (file);
  g_assert_cmpuint (xcf_painter_storage_live_files (), ==, state.before);
  g_assert_cmpuint (gimp_painter_provenance_get_save_id (G_OBJECT (layer (image, "clone"))), ==, saved_id);
  g_assert_cmpint (gimp_image_is_dirty (image), ==, dirty);
  g_object_unref (cancel); g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
}

#ifdef G_OS_UNIX
#include <sys/resource.h>
#include <signal.h>
static void multipart_native_disk_failure (void)
{
  GimpImage *image = fixture ("legacy-runtime/clone-normal-in-group.xcf");
  GFile *file = temporary_file ();
  GBytes *record = multipart_opaque_record (2 * 1024 * 1024 + 1);
  GPtrArray *records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  struct rlimit previous, small;
  struct sigaction previous_signal, ignore_signal = {0};
  GError *error = NULL;
  gboolean success;
  guint before = xcf_painter_storage_live_files ();
  guint32 saved_id = gimp_painter_provenance_get_save_id (G_OBJECT (layer (image, "clone")));
  gint dirty = gimp_image_is_dirty (image);
  g_ptr_array_add (records, record);
  g_assert_true (gimp_painter_provenance_set_records (G_OBJECT (image), records));
  g_ptr_array_unref (records); sentinel_write (file);
  g_assert_cmpint (getrlimit (RLIMIT_FSIZE, &previous), ==, 0); small = previous; small.rlim_cur = 65536;
  ignore_signal.sa_handler = SIG_IGN; sigemptyset (&ignore_signal.sa_mask);
  g_assert_cmpint (sigaction (SIGXFSZ, &ignore_signal, &previous_signal), ==, 0);
  g_assert_cmpint (setrlimit (RLIMIT_FSIZE, &small), ==, 0);
  success = xcf_save_file_with_painter_transport (gimp, image, file, NULL, TRUE, NULL, &error);
  g_assert_cmpint (setrlimit (RLIMIT_FSIZE, &previous), ==, 0);
  g_assert_cmpint (sigaction (SIGXFSZ, &previous_signal, NULL), ==, 0);
  g_assert_false (success); g_assert_nonnull (error);
  g_test_message ("Actual temporary-file write refusal: %s", error->message); g_clear_error (&error);
  sentinel_check (file);
  g_assert_cmpuint (xcf_painter_storage_live_files (), ==, before);
  g_assert_cmpuint (gimp_painter_provenance_get_save_id (G_OBJECT (layer (image, "clone"))), ==, saved_id);
  g_assert_cmpint (gimp_image_is_dirty (image), ==, dirty);
  g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
}
#else
static void multipart_native_disk_failure (void)
{ g_test_skip ("RLIMIT_FSIZE fault injection requires Unix"); }
#endif

static void multipart_native_owner_duplicate_edit (void)
{
  const guint live_before = xcf_painter_storage_live_files ();
  GimpImage *image = gimp_image_new (gimp, 4, 4, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *ordinary = gimp_layer_new (image, 4, 4, babl_format ("R'G'B'A u8"), "multipart ordinary", 1, GIMP_LAYER_MODE_NORMAL);
  GimpLayer *clone = gimp_clone_layer_new (image, ordinary, 4, 4, "multipart clone", 1, GIMP_LAYER_MODE_NORMAL);
  GimpLayer *filter = gimp_filter_layer_new (image, 4, 4, "multipart filter", 1, GIMP_LAYER_MODE_NORMAL);
  GimpLayer *group = gimp_group_layer_new (image);
  GeglColor *color = gegl_color_new ("red");
  GimpChannel *channel = gimp_channel_new (image, 4, 4, "multipart channel", color);
  GimpPath *path_item = gimp_path_new (image, "multipart path");
  GimpLayerMask *mask;
  GBytes *record = multipart_opaque_record (1024 * 1024 + 1);
  gchar *digest = multipart_digest (record);
  GPtrArray *records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  GimpValueArray *args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 17, G_TYPE_NONE);
  GFile *file = temporary_file ();
  GError *error = NULL;
  g_object_unref (color);
  gimp_image_add_layer (image, ordinary, NULL, 0, FALSE);
  gimp_image_add_layer (image, clone, NULL, 0, FALSE);
  gimp_image_add_layer (image, filter, NULL, 0, FALSE);
  gimp_image_add_layer (image, group, NULL, 0, FALSE);
  gimp_object_set_name (GIMP_OBJECT (group), "multipart group");
  gimp_image_add_channel (image, channel, NULL, 0, FALSE);
  gimp_image_add_path (image, path_item, NULL, 0, FALSE);
  mask = gimp_layer_create_mask (ordinary, GIMP_ADD_MASK_WHITE, NULL);
  gimp_layer_add_mask (ordinary, mask, FALSE, NULL);
  g_assert_true (gimp_filter_layer_set_definition (GIMP_FILTER_LAYER (filter), "unavailable", record, args, NULL));
  gimp_filter_layer_mark_as_loaded (GIMP_FILTER_LAYER (filter));
  gimp_value_array_unref (args);
  g_ptr_array_add (records, record);
  {
    GObject *owners[] = {G_OBJECT (image), G_OBJECT (ordinary), G_OBJECT (clone), G_OBJECT (filter),
      G_OBJECT (group), G_OBJECT (channel), G_OBJECT (mask), G_OBJECT (path_item)};
    for (guint i = 0; i < G_N_ELEMENTS (owners); ++i)
      g_assert_true (gimp_painter_provenance_set_records (owners[i], records));
  }
  g_ptr_array_unref (records);
  for (guint pass = 0; pass < 3; ++pass)
    {
      GimpImage *copy;
      GList *channels, *paths;
      save (image, file);
      copy = load (file); g_object_unref (image); image = copy;
      ordinary = layer (image, "multipart ordinary"); clone = layer (image, "multipart clone");
      filter = layer (image, "multipart filter"); group = layer (image, "multipart group");
      g_assert_true (GIMP_IS_CLONE_LAYER (clone)); g_assert_true (GIMP_IS_FILTER_LAYER (filter));
      g_assert_true (GIMP_IS_GROUP_LAYER (group));
      {
        GVariant *state = gimp_painter_provenance_ref_definition (G_OBJECT (clone));
        guint32 source_id = 0;
        if (state) g_variant_lookup (state, "source-id", "u", &source_id);
        g_test_message ("owner pass=%u saved-source=%u ordinary-tattoo=%u source=%s", pass, source_id,
          gimp_item_get_tattoo (GIMP_ITEM (ordinary)),
          gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)) ? gimp_object_get_name (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone))) : "NULL");
        if (state) g_variant_unref (state);
      }
      g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)) == ordinary);
      channels = gimp_image_get_channel_list (image); paths = gimp_image_get_path_list (image);
      g_assert_cmpuint (g_list_length (channels), ==, 1); g_assert_cmpuint (g_list_length (paths), ==, 1);
      mask = gimp_layer_get_mask (ordinary); g_assert_nonnull (mask);
      {
        GObject *owners[] = {G_OBJECT (image), G_OBJECT (ordinary), G_OBJECT (clone), G_OBJECT (filter),
          G_OBJECT (group), G_OBJECT (channels->data), G_OBJECT (mask), G_OBJECT (paths->data)};
        for (guint i = 0; i < G_N_ELEMENTS (owners); ++i)
          multipart_check_archive (owners[i], i ? GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM : GIMP_PAINTER_PROVENANCE_NAMESPACE_IMAGE,
                                   digest, 1024 * 1024 + 9);
      }
      args = gimp_filter_layer_dup_args (GIMP_FILTER_LAYER (filter));
      g_assert_nonnull (args); g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 0)), ==, pass ? 29 : 17);
      if (!pass)
        {
          GimpImage *duplicate = gimp_image_duplicate (image);
          GimpCloneLayer *duplicate_clone = GIMP_CLONE_LAYER (layer (duplicate, "multipart clone"));
          /* Whole-image duplication historically keeps top-level Clone source
           * identity. Group duplication has its own hierarchy-local remap. */
          g_assert_true (gimp_clone_layer_get_source (duplicate_clone) == ordinary);
          g_object_unref (image); image = duplicate;
          g_assert_null (gimp_clone_layer_get_source (duplicate_clone));
          /* Explicit user re-edit establishes the new image-local identity. */
          g_assert_true (gimp_clone_layer_set_source_full (duplicate_clone, layer (image, "multipart ordinary"), NULL));
          filter = layer (image, "multipart filter");
          g_value_set_int (gimp_value_array_index (args, 0), 29);
          record = gimp_filter_layer_ref_definition (GIMP_FILTER_LAYER (filter));
          g_assert_true (gimp_filter_layer_set_definition (GIMP_FILTER_LAYER (filter), "unavailable", record, args, NULL));
          gimp_filter_layer_mark_as_loaded (GIMP_FILTER_LAYER (filter)); g_bytes_unref (record);
        }
      gimp_value_array_unref (args); g_list_free (channels); g_list_free (paths);
    }
  g_free (digest); g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
  g_assert_cmpuint (xcf_painter_storage_live_files (), ==, live_before);
}

static void multipart_native_callback_argument_leases (void)
{
  GimpImage *image = fixture ("legacy-runtime/clone-normal-in-group.xcf");
  GFile *file = temporary_file ();
  SaveProgress *progress = g_object_new (save_progress_get_type (), NULL);
  GObject *weak_image = G_OBJECT (image), *weak_file = G_OBJECT (file), *weak_progress = G_OBJECT (progress);
  gchar *filename = g_file_get_path (file);
  GError *error = NULL;
  sentinel_write (file);
  progress->cancel_at = 0;
  progress->release_on_start[0] = G_OBJECT (image);
  progress->release_on_start[1] = G_OBJECT (file);
  progress->release_on_start[2] = G_OBJECT (progress);
  g_object_add_weak_pointer (weak_image, (gpointer *) &weak_image);
  g_object_add_weak_pointer (weak_file, (gpointer *) &weak_file);
  g_object_add_weak_pointer (weak_progress, (gpointer *) &weak_progress);
  g_assert_false (xcf_save_file_with_painter_transport (gimp, image, file, GIMP_PROGRESS (progress), TRUE, NULL, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED); g_clear_error (&error);
  g_assert_null (weak_image); g_assert_null (weak_file); g_assert_null (weak_progress);
  file = g_file_new_for_path (filename); sentinel_check (file);
  g_file_delete (file, NULL, NULL); g_object_unref (file); g_free (filename);
}

static void multipart_native_stream_failures (void)
{
  const guint live_before = xcf_painter_storage_live_files ();
  GimpImage *image = fixture ("legacy-runtime/clone-normal-in-group.xcf");
  GFile *file = temporary_file ();
  GBytes *record = multipart_opaque_record (2 * 1024 * 1024 + 1);
  GPtrArray *records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  guint32 saved_id = gimp_painter_provenance_get_save_id (G_OBJECT (layer (image, "clone")));
  gint dirty = gimp_image_is_dirty (image);
  const gsize limits[] = {0, 40, 512, 65536, 131072, 1048576, G_MAXSIZE};
  g_ptr_array_add (records, record);
  g_assert_true (gimp_painter_provenance_set_records (G_OBJECT (image), records));
  g_ptr_array_unref (records);
  for (guint i = 0; i < G_N_ELEMENTS (limits); ++i)
    {
      const guint before = xcf_painter_storage_live_files ();
      GError *error = NULL;
      GOutputStream *base;
      FailOutput *output;
      sentinel_write (file);
      base = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));
      g_assert_no_error (error);
      output = g_object_new (fail_output_get_type (), "base-stream", base, "close-base-stream", TRUE, NULL);
      g_object_unref (base); output->limit = limits[i]; output->fail_final_close = limits[i] == G_MAXSIZE;
      g_assert_false (xcf_save_stream_with_painter_transport (gimp, image, G_OUTPUT_STREAM (output), file, NULL, TRUE, NULL, &error));
      g_assert_error (error, G_IO_ERROR, (output->fail_final_close ? G_IO_ERROR_FAILED : G_IO_ERROR_NO_SPACE));
      g_assert_nonnull (strstr (error->message, output->fail_final_close ? "Injected final close" : "Injected write"));
      g_clear_error (&error);
      g_assert_true (g_output_stream_is_closed (G_OUTPUT_STREAM (output)));
      if (!output->fail_final_close) g_assert_true (output->cancelled_close);
      g_object_unref (output); sentinel_check (file);
      g_assert_cmpuint (gimp_painter_provenance_get_save_id (G_OBJECT (layer (image, "clone"))), ==, saved_id);
      g_assert_cmpint (gimp_image_is_dirty (image), ==, dirty);
      g_assert_cmpuint (xcf_painter_storage_live_files (), ==, before);
    }
  g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
  g_assert_cmpuint (xcf_painter_storage_live_files (), ==, live_before);
}

void painter_xcf_test_argument_resource_file (GimpImage *, GVariant *, GBytes *, GFile *);
static void multipart_native_argument_budget (void)
{
  const guint live_before = xcf_painter_storage_live_files ();
  GimpImage *image = fixture ("legacy-runtime/filter-edge.xcf");
  GimpFilterLayer *filter = NULL;
  GList *layers;
  GBytes *large = multipart_opaque_record (256u * 1024u * 1024u + 1);
  GBytes *raw, *opaque;
  gchar *expected = NULL;
  GVariant *base;
  GFile *file = temporary_file ();
  save (image, file);
  { GimpImage *copy = load (file); g_object_unref (image); image = copy; }
  layers = gimp_image_get_layer_list (image);
  for (GList *p = layers; p; p = p->next) if (GIMP_IS_FILTER_LAYER (p->data)) filter = p->data;
  g_list_free (layers); g_assert_nonnull (filter);
  base = gimp_painter_provenance_ref_definition (G_OBJECT (filter)); g_assert_nonnull (base);
  raw = gimp_filter_layer_ref_definition (filter);
  painter_xcf_test_argument_resource_file (image, base, large, file);
  g_variant_unref (base); g_bytes_unref (large); g_object_unref (image);
  image = load (file);
  for (guint pass = 0; pass < 2; ++pass)
    {
      GBytes *retained; gchar *digest;
      filter = GIMP_FILTER_LAYER (layer (image, "L")); g_assert_true (GIMP_IS_FILTER_LAYER (filter));
      g_assert_null (gimp_filter_layer_snapshot_arguments (filter));
      opaque = gimp_filter_layer_ref_opaque_arguments (filter); g_assert_nonnull (opaque);
      g_assert_cmpuint (g_bytes_get_size (opaque), >, 256u * 1024u * 1024u);
      digest = multipart_digest (opaque);
      if (!pass) expected = digest; else { g_assert_cmpstr (digest, ==, expected); g_free (digest); }
      g_bytes_unref (opaque);
      retained = gimp_filter_layer_ref_definition (filter);
      g_assert_true (g_bytes_equal (retained, raw)); g_bytes_unref (retained);
      pixel (GIMP_LAYER (filter), 9, 8, 7, 255);
      while (g_main_context_iteration (NULL, FALSE));
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
      if (!pass) { GimpImage *copy; save (image, file); copy = load (file); g_object_unref (image); image = copy; }
    }
  g_free (expected); g_bytes_unref (raw); g_object_unref (image);
  g_file_delete (file, NULL, NULL); g_object_unref (file);
  g_assert_cmpuint (xcf_painter_storage_live_files (), ==, live_before);
}
