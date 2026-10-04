/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native XCF Save/Open coverage; include after test-painter-xcf-multipart.inc.
 * Mutations keep every owner's property span the same size, so the writer's
 * native layer, hierarchy and tile offsets remain authoritative. */

typedef enum
{
  MULTIPART_WIRE_REVERSE,
  MULTIPART_WIRE_GROUP,
  MULTIPART_WIRE_BAD_DIGEST,
  MULTIPART_WIRE_MISSING,
  MULTIPART_WIRE_DUPLICATE,
  MULTIPART_WIRE_TRUNCATED,
  MULTIPART_WIRE_FOREIGN_OWNER,
  MULTIPART_WIRE_WRONG_NAMESPACE
} MultipartWireMutation;

static guint32
multipart_wire_be32 (const guint8 *data)
{
  guint32 value;
  memcpy (&value, data, sizeof value);
  return GUINT32_FROM_BE (value);
}

static guint32
multipart_wire_le32 (const guint8 *data)
{
  guint32 value;
  memcpy (&value, data, sizeof value);
  return GUINT32_FROM_LE (value);
}

static void
multipart_wire_put_le32 (guint8 *data,
                         guint32 value)
{
  value = GUINT32_TO_LE (value);
  memcpy (data, &value, sizeof value);
}

static void
multipart_wire_append (GByteArray *output,
                       GBytes     *bytes)
{
  gsize size;
  const guint8 *data = g_bytes_get_data (bytes, &size);
  g_byte_array_append (output, data, size);
}

static void
multipart_wire_property (GByteArray *output,
                         const guint8 *data,
                         gsize size)
{
  const guint32 header[] = {GUINT32_TO_BE (21), GUINT32_TO_BE (size)};
  g_assert_cmpuint (size, <=, G_MAXUINT32);
  g_byte_array_append (output, (const guint8 *) header, sizeof header);
  if (size) g_byte_array_append (output, data, size);
}

static guint
multipart_wire_version (const guint8 *data,
                        gsize size)
{
  g_assert_cmpuint (size, >=, 30);
  g_assert_cmpmem (data, 9, "gimp xcf ", 9);
  if (data[9] != 'v') return 0;
  g_assert_true (g_ascii_isdigit (data[10]));
  g_assert_true (g_ascii_isdigit (data[11]));
  g_assert_true (g_ascii_isdigit (data[12]));
  return (data[10] - '0') * 100 + (data[11] - '0') * 10 + data[12] - '0';
}

static gsize
multipart_wire_props_end (const guint8 *data,
                          gsize size,
                          gsize begin)
{
  for (gsize at = begin; ;)
    {
      guint32 type, length;
      g_assert_cmpuint (at, <=, size);
      g_assert_cmpuint (size - at, >=, 8);
      type = multipart_wire_be32 (data + at);
      length = multipart_wire_be32 (data + at + 4);
      g_assert_cmpuint (length, <=, size - at - 8);
      at += 8 + length;
      if (!type) { g_assert_cmpuint (length, ==, 0); return at; }
    }
}

static gsize
multipart_wire_layer_props (const guint8 *data,
                            gsize size,
                            const gchar *name)
{
  const guint version = multipart_wire_version (data, size);
  const guint width = version >= 11 ? 8 : 4;
  gsize at = multipart_wire_props_end (data, size, 26 + (version >= 4 ? 4 : 0));
  for (;; at += width)
    {
      guint64 offset = 0;
      guint32 name_size;
      g_assert_cmpuint (at + width, <=, size);
      for (guint i = 0; i < width; ++i) offset = (offset << 8) | data[at + i];
      g_assert_cmpuint (offset, >, 0);
      g_assert_cmpuint (offset, <=, size - 16);
      name_size = multipart_wire_be32 (data + offset + 12);
      g_assert_cmpuint (name_size, >, 0);
      g_assert_cmpuint (name_size, <=, size - offset - 16);
      g_assert_cmpuint (data[offset + 15 + name_size], ==, 0);
      if (!strcmp ((const gchar *) data + offset + 16, name))
        return offset + 16 + name_size;
    }
}

/* These helpers parse inner parasite framing independently of the production
 * multipart parser. That also gives the exact raw-record retention oracle. */
static gsize
multipart_wire_record_size (const guint8 *data,
                            gsize available)
{
  guint32 name_size, payload_size;
  g_assert_cmpuint (available, >=, 13);
  name_size = multipart_wire_be32 (data);
  g_assert_cmpuint (name_size, >, 0);
  g_assert_cmpuint (name_size, <=, available - 12);
  g_assert_cmpuint (data[3 + name_size], ==, 0);
  payload_size = multipart_wire_be32 (data + 8 + name_size);
  g_assert_cmpuint (payload_size, <=, available - 12 - name_size);
  return 12 + name_size + payload_size;
}

static gboolean
multipart_wire_reserved (GBytes *record)
{
  const guint8 *data = g_bytes_get_data (record, NULL);
  const gchar *name = (const gchar *) data + 4;
  return !strcmp (name, "gimp-painter-image") ||
         !strcmp (name, "gimp-painter-item") ||
         !strcmp (name, "gimp-painter-origin") ||
         g_str_has_prefix (name, "gimp-painter-chunk-v1/") ||
         g_str_has_prefix (name, "gimp-painter-inert-v1/");
}

static gboolean
multipart_wire_chunk (GBytes *record)
{
  const guint8 *data = g_bytes_get_data (record, NULL);
  return g_str_has_prefix ((const gchar *) data + 4, "gimp-painter-chunk-v1/");
}

static GPtrArray *
multipart_wire_records (const guint8 *data,
                        gsize size,
                        gsize begin,
                        gboolean reserved_only)
{
  GPtrArray *records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  const gsize end = multipart_wire_props_end (data, size, begin);
  for (gsize at = begin; at < end;)
    {
      const guint32 type = multipart_wire_be32 (data + at);
      const guint32 length = multipart_wire_be32 (data + at + 4);
      const gsize next = at + 8 + length;
      if (type == 21)
        for (gsize inner = at + 8; inner < next;)
          {
            const gsize count = multipart_wire_record_size (data + inner, next - inner);
            GBytes *record = g_bytes_new (data + inner, count);
            if (!reserved_only || multipart_wire_reserved (record)) g_ptr_array_add (records, record);
            else g_bytes_unref (record);
            inner += count;
          }
      at = next;
    }
  return records;
}

static void
multipart_wire_mutate_record (GPtrArray *records,
                              guint index,
                              MultipartWireMutation mutation)
{
  GBytes *old = g_ptr_array_index (records, index);
  gsize size;
  const guint8 *source = g_bytes_get_data (old, &size);
  guint8 *data = g_memdup2 (source, size);
  const guint32 name_size = multipart_wire_be32 (data);
  guint8 *payload = data + 12 + name_size;
  g_assert_cmpuint (size - 12 - name_size, >=, 84);
  switch (mutation)
    {
    case MULTIPART_WIRE_BAD_DIGEST:
      data[size - 1] ^= 0x80;
      break;
    case MULTIPART_WIRE_MISSING:
      /* Same-length ordinary parasite replaces one reserved chunk. */
      memcpy (data + 4, "test", 4);
      break;
    case MULTIPART_WIRE_TRUNCATED:
      /* Truncate the chunk payload, keeping the surrounding XCF well-formed.
       * An empty PROP_PARASITES header below replaces these eight bytes. */
      size -= 8;
      wire32 (data, 8 + name_size, size - 12 - name_size);
      break;
    case MULTIPART_WIRE_WRONG_NAMESPACE:
      multipart_wire_put_le32 (payload + 20, 2);
      break;
    default:
      g_assert_not_reached ();
    }
  g_ptr_array_index (records, index) = g_bytes_new_take (data, size);
  g_bytes_unref (old);
}

static void
multipart_wire_mutate_owner (guint8 *data,
                             gsize size,
                             gsize begin,
                             MultipartWireMutation mutation)
{
  const gsize end = multipart_wire_props_end (data, size, begin);
  GByteArray *output = g_byte_array_new ();
  GPtrArray *properties = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  GPtrArray *records = multipart_wire_records (data, size, begin, FALSE);
  guint first_chunk = G_MAXUINT, second_chunk = G_MAXUINT;
  guint parasite_properties = 0;

  for (gsize at = begin; at < end - 8;)
    {
      const guint32 type = multipart_wire_be32 (data + at);
      const guint32 length = multipart_wire_be32 (data + at + 4);
      if (type == 21)
        {
          ++parasite_properties;
          g_ptr_array_add (properties, g_bytes_new (data + at, length + 8));
        }
      else g_byte_array_append (output, data + at, length + 8);
      at += length + 8;
    }
  g_assert_cmpuint (parasite_properties, >, 32);
  for (guint i = 0; i < records->len; ++i)
    if (multipart_wire_chunk (g_ptr_array_index (records, i)))
      {
        if (first_chunk == G_MAXUINT) first_chunk = i;
        else if (second_chunk == G_MAXUINT) second_chunk = i;
      }
  g_assert_cmpuint (second_chunk, !=, G_MAXUINT);

  if (mutation == MULTIPART_WIRE_REVERSE)
    {
      for (guint i = properties->len; i > 0; --i)
        multipart_wire_append (output, g_ptr_array_index (properties, i - 1));
    }
  else
    {
      GByteArray *group = g_byte_array_new ();
      GPtrArray *ordinary = g_ptr_array_new ();
      guint ordinary_at = 0, appended = 0;
      if (mutation == MULTIPART_WIRE_DUPLICATE)
        {
          GBytes *old = g_ptr_array_index (records, second_chunk);
          GBytes *duplicate = g_ptr_array_index (records, first_chunk);
          g_assert_cmpuint (g_bytes_get_size (old), ==, g_bytes_get_size (duplicate));
          g_ptr_array_index (records, second_chunk) = g_bytes_ref (duplicate);
          g_bytes_unref (old);
        }
      else if (mutation == MULTIPART_WIRE_FOREIGN_OWNER)
        {
          /* A complete foreign set must not acquire the current native owner. */
          for (guint i = 0; i < records->len; ++i)
            {
              GBytes *old = g_ptr_array_index (records, i);
              if (multipart_wire_reserved (old))
                {
                  gsize count;
                  const guint8 *source = g_bytes_get_data (old, &count);
                  guint8 *copy = g_memdup2 (source, count);
                  guint8 *payload = copy + 12 + multipart_wire_be32 (copy);
                  g_assert_cmpuint (count - (payload - copy), >=, 76);
                  multipart_wire_put_le32 (payload + 16, multipart_wire_le32 (payload + 16) + 71);
                  g_ptr_array_index (records, i) = g_bytes_new_take (copy, count);
                  g_bytes_unref (old);
                }
            }
        }
      else if (mutation != MULTIPART_WIRE_GROUP)
        multipart_wire_mutate_record (records, first_chunk, mutation);

      for (guint i = 0; i < records->len; ++i)
        if (!multipart_wire_reserved (g_ptr_array_index (records, i)))
          g_ptr_array_add (ordinary, g_ptr_array_index (records, i));
      g_assert_cmpuint (ordinary->len, >=, 3);
      /* Reverse the chunks and put ordinary parasites between them in one
       * PROP_PARASITES. The manifest is deliberately encountered last. */
      for (guint i = records->len; i > 0; --i)
        if (multipart_wire_reserved (g_ptr_array_index (records, i - 1)))
          {
            multipart_wire_append (group, g_ptr_array_index (records, i - 1));
            if (++appended % 7 == 0 && ordinary_at < ordinary->len)
              multipart_wire_append (group, g_ptr_array_index (ordinary, ordinary_at++));
          }
      while (ordinary_at < ordinary->len)
        multipart_wire_append (group, g_ptr_array_index (ordinary, ordinary_at++));
      multipart_wire_property (output, group->data, group->len);
      for (guint i = 1; i < parasite_properties; ++i)
        multipart_wire_property (output, NULL, 0);
      if (mutation == MULTIPART_WIRE_TRUNCATED)
        multipart_wire_property (output, NULL, 0);
      g_ptr_array_unref (ordinary);
      g_byte_array_unref (group);
    }
  g_byte_array_append (output, data + end - 8, 8);
  g_assert_cmpuint (output->len, ==, end - begin);
  g_assert_cmpint (memcmp (data + begin, output->data, output->len), !=, 0);
  memcpy (data + begin, output->data, output->len);
  g_byte_array_unref (output);
  g_ptr_array_unref (properties);
  g_ptr_array_unref (records);
}

static const gchar *multipart_ordinary_names[] = {
  "painter-multipart-ordinary-a", "painter-multipart-ordinary-b", "painter-multipart-ordinary-c"
};
static const guint8 multipart_ordinary_data[] = {0, 0x31, 0xff, 'G', 'P', 'X', 'C', 'C', 0, 0xa7};

static void
multipart_ordinary_attach (GObject *owner)
{
  for (guint i = 0; i < G_N_ELEMENTS (multipart_ordinary_names); ++i)
    {
      GimpParasite *parasite = gimp_parasite_new (multipart_ordinary_names[i],
                                                GIMP_PARASITE_PERSISTENT | GIMP_PARASITE_UNDOABLE,
                                                sizeof multipart_ordinary_data, multipart_ordinary_data);
      if (GIMP_IS_IMAGE (owner)) gimp_image_parasite_attach (GIMP_IMAGE (owner), parasite, FALSE);
      else gimp_item_parasite_attach (GIMP_ITEM (owner), parasite, TRUE);
      gimp_parasite_free (parasite);
    }
}

static void
multipart_ordinary_check (GObject *owner)
{
  for (guint i = 0; i < G_N_ELEMENTS (multipart_ordinary_names); ++i)
    {
      const GimpParasite *parasite = GIMP_IS_IMAGE (owner) ?
        gimp_image_parasite_find (GIMP_IMAGE (owner), multipart_ordinary_names[i]) :
        gimp_item_parasite_find (GIMP_ITEM (owner), multipart_ordinary_names[i]);
      guint32 size;
      const void *actual;
      g_assert_nonnull (parasite);
      actual = gimp_parasite_get_data (parasite, &size);
      g_assert_cmpmem (actual, size, multipart_ordinary_data, sizeof multipart_ordinary_data);
      g_assert_cmpuint (gimp_parasite_get_flags (parasite), ==,
                        GIMP_PARASITE_PERSISTENT | GIMP_PARASITE_UNDOABLE);
    }
}

static guint
multipart_inert_check (GObject *owner,
                       guint space,
                       GPtrArray *expected)
{
  GimpPainterProvenanceTransport *transport = gimp_painter_provenance_ref_transport (owner);
  gsize count;
  const guint8 *dispositions;
  guint original_at = 0, carriers = 0;
  g_assert_nonnull (transport);
  g_assert_true (transport->present[space]);
  g_assert_true (transport->invalid[space]);
  g_assert_null (transport->capsules[space]);
  g_assert_nonnull (transport->records);
  g_assert_cmpuint (transport->records->len, >=, expected->len);
  dispositions = g_bytes_get_data (transport->dispositions, &count);
  g_assert_cmpuint (count, ==, transport->records->len);
  for (guint i = 0; i < transport->records->len; ++i)
    {
      GBytes *record = g_ptr_array_index (transport->records, i);
      const guint8 *data = g_bytes_get_data (record, NULL);
      const gchar *name = (const gchar *) data + 4;
      const gchar prefix[] = "gimp-painter-inert-v1/";
      g_assert_cmpuint (dispositions[i], ==, GIMP_PAINTER_PROVENANCE_TRANSPORT_INERT);
      if (g_str_has_prefix (name, prefix))
        {
          /* Versioned inert carriers are one-time writer overhead. Every
           * original record still has to survive byte-exact, in order. */
          g_assert_cmpuint (strlen (name), ==, sizeof prefix - 1 + 64);
          for (guint j = sizeof prefix - 1; name[j]; ++j)
            g_assert_true (g_ascii_isxdigit (name[j]));
          ++carriers;
          continue;
        }
      g_assert_cmpuint (original_at, <, expected->len);
      g_assert_true (g_bytes_equal (record, g_ptr_array_index (expected, original_at++)));
    }
  g_assert_cmpuint (original_at, ==, expected->len);
  g_assert_cmpuint (carriers, <=, expected->len);
  g_assert_false (gimp_painter_provenance_has_definition (owner));
  gimp_painter_provenance_free_transport (transport);
  return carriers;
}

static void
multipart_native_wire_case (MultipartWireMutation mutation)
{
  const guint live_before = xcf_painter_storage_live_files ();
  const gsize payload_size = 2 * 1024 * 1024 + 1;
  const gboolean valid = mutation <= MULTIPART_WIRE_GROUP;
  GimpImage *image = fixture ("legacy-runtime/clone-normal-in-group.xcf");
  GFile *file = temporary_file ();
  GBytes *record = multipart_opaque_record (payload_size);
  GPtrArray *records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  GPtrArray *expected_image, *expected_item;
  gchar *digest = multipart_digest (record);
  gchar *raw;
  gsize size, image_props, item_props;
  goffset first_size = 0;
  GError *error = NULL;

  g_ptr_array_add (records, record);
  g_assert_true (gimp_painter_provenance_set_records (G_OBJECT (image), records));
  g_assert_true (gimp_painter_provenance_set_records (G_OBJECT (layer (image, "clone")), records));
  g_ptr_array_unref (records);
  multipart_ordinary_attach (G_OBJECT (image));
  multipart_ordinary_attach (G_OBJECT (layer (image, "clone")));
  g_assert_true (xcf_save_file_with_painter_transport (gimp, image, file, NULL, TRUE, NULL, &error));
  g_assert_no_error (error);
  g_object_unref (image);
  g_assert_true (g_file_load_contents (file, NULL, &raw, &size, NULL, &error));
  g_assert_no_error (error);
  image_props = 26 + (multipart_wire_version ((const guint8 *) raw, size) >= 4 ? 4 : 0);
  item_props = multipart_wire_layer_props ((const guint8 *) raw, size, "clone");
  multipart_wire_mutate_owner ((guint8 *) raw, size, image_props, mutation);
  multipart_wire_mutate_owner ((guint8 *) raw, size, item_props, mutation);
  expected_image = multipart_wire_records ((const guint8 *) raw, size, image_props, TRUE);
  expected_item = multipart_wire_records ((const guint8 *) raw, size, item_props, TRUE);
  g_assert_true (g_file_replace_contents (file, raw, size, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpint (multipart_file_size (file), ==, size);
  g_free (raw);
  image = load (file);

  /* Check the mutated native Open itself and then three complete Save/Open
   * cycles. The first rewritten file establishes the stable framing size. */
  for (guint pass = 0; pass < 4; ++pass)
    {
      GimpLayer *clone = layer (image, "clone");
      multipart_ordinary_check (G_OBJECT (image));
      multipart_ordinary_check (G_OBJECT (clone));
      pixel (clone, 191, 32, 64, 255);
      if (valid)
        {
          multipart_check_archive (G_OBJECT (image), GIMP_PAINTER_PROVENANCE_NAMESPACE_IMAGE, digest, payload_size + 8);
          multipart_check_archive (G_OBJECT (clone), GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM, digest, payload_size + 8);
          g_assert_true (GIMP_IS_CLONE_LAYER (clone));
          g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)) == layer (image, "source child"));
        }
      else
        {
          multipart_inert_check (G_OBJECT (image), GIMP_PAINTER_PROVENANCE_NAMESPACE_IMAGE, expected_image);
          multipart_inert_check (G_OBJECT (clone), GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM, expected_item);
          g_assert_false (GIMP_IS_CLONE_LAYER (clone));
          g_assert_false (GIMP_IS_FILTER_LAYER (clone));
          g_assert_null (gimp_item_parasite_find (GIMP_ITEM (clone), "gimp-painter-item"));
          g_assert_null (gimp_image_parasite_find (image, "gimp-painter-image"));
        }
      if (pass < 3)
        {
          GimpImage *copy;
          g_assert_true (xcf_save_file_with_painter_transport (gimp, image, file, NULL, TRUE, NULL, &error));
          g_assert_no_error (error);
          if (!pass) first_size = multipart_file_size (file);
          else g_assert_cmpint (multipart_file_size (file), ==, first_size);
          copy = load (file);
          g_object_unref (image);
          image = copy;
        }
    }
  g_test_message ("native multipart wire case=%u stable-file=%" G_GOFFSET_FORMAT, mutation, first_size);
  g_ptr_array_unref (expected_image);
  g_ptr_array_unref (expected_item);
  g_free (digest);
  g_object_unref (image);
  g_file_delete (file, NULL, NULL);
  g_object_unref (file);
  g_assert_cmpuint (xcf_painter_storage_live_files (), ==, live_before);
}

static void multipart_native_reordered (void) { multipart_native_wire_case (MULTIPART_WIRE_REVERSE); }
static void multipart_native_grouped (void) { multipart_native_wire_case (MULTIPART_WIRE_GROUP); }
static void
multipart_native_adversarial (void)
{
  for (guint scene = MULTIPART_WIRE_BAD_DIGEST; scene <= MULTIPART_WIRE_WRONG_NAMESPACE; ++scene)
    multipart_native_wire_case (scene);
}

static guint32
multipart_wire_manifest_owner (GPtrArray *records)
{
  for (guint i = 0; i < records->len; ++i)
    {
      GBytes *record = g_ptr_array_index (records, i);
      gsize size;
      const guint8 *data = g_bytes_get_data (record, &size);
      const guint32 name_size = multipart_wire_be32 (data);
      if (!strcmp ((const gchar *) data + 4, "gimp-painter-item"))
        {
          g_assert_cmpuint (size - 12 - name_size, ==, 76);
          return multipart_wire_le32 (data + 12 + name_size + 16);
        }
    }
  g_assert_not_reached ();
  return 0;
}

static void
multipart_native_inert_ownership_changes (void)
{
  const guint live_before = xcf_painter_storage_live_files ();
  GimpImage *source = fixture ("legacy-runtime/clone-normal-in-group.xcf");
  GFile *input = temporary_file ();
  GFile *output = temporary_file ();
  GPtrArray *records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  GPtrArray *expected;
  GError *error = NULL;
  gchar *raw;
  gsize size, begin;
  guint32 foreign_id;

  g_ptr_array_add (records, multipart_opaque_record (2 * 1024 * 1024 + 1));
  g_assert_true (gimp_painter_provenance_set_records (G_OBJECT (layer (source, "clone")), records));
  g_ptr_array_unref (records);
  multipart_ordinary_attach (G_OBJECT (layer (source, "clone")));
  g_assert_true (xcf_save_file_with_painter_transport (gimp, source, input, NULL, TRUE, NULL, &error));
  g_assert_no_error (error);
  g_object_unref (source);
  g_assert_true (g_file_load_contents (input, NULL, &raw, &size, NULL, &error));
  g_assert_no_error (error);
  begin = multipart_wire_layer_props ((const guint8 *) raw, size, "clone");
  /* The whole set stays internally complete and consistent. Only its owner
   * ID disagrees with the native tattoo at the first Open. */
  multipart_wire_mutate_owner ((guint8 *) raw, size, begin, MULTIPART_WIRE_FOREIGN_OWNER);
  expected = multipart_wire_records ((const guint8 *) raw, size, begin, TRUE);
  foreign_id = multipart_wire_manifest_owner (expected);
  g_assert_true (g_file_replace_contents (input, raw, size, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error));
  g_assert_no_error (error);
  g_free (raw);

  for (guint scene = 0; scene < 3; ++scene)
    {
      GimpImage *image = load (input);
      GimpLayer *item = layer (image, "clone");
      const gchar *name = scene == 0 ? "clone" : scene == 1 ? "inert duplicate" : "inert conversion";
      goffset first_size = 0;
      g_assert_cmpuint (gimp_item_get_tattoo (GIMP_ITEM (item)), !=, foreign_id);
      g_assert_false (GIMP_IS_CLONE_LAYER (item));
      g_assert_cmpuint (multipart_inert_check (G_OBJECT (item), GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM, expected), ==, 0);
      if (scene == 1)
        {
          item = GIMP_LAYER (gimp_item_duplicate (GIMP_ITEM (item), GIMP_TYPE_LAYER));
          g_assert_nonnull (item);
          gimp_object_set_name (GIMP_OBJECT (item), name);
          gimp_image_add_layer (image, item, NULL, 0, FALSE);
        }
      else if (scene == 2)
        {
          GimpImage *destination = fixture ("legacy-runtime/ordinary-layers.xcf");
          item = GIMP_LAYER (gimp_item_convert (GIMP_ITEM (item), destination, GIMP_TYPE_LAYER));
          g_assert_nonnull (item);
          gimp_object_set_name (GIMP_OBJECT (item), name);
          gimp_image_add_layer (destination, item, NULL, 0, FALSE);
          g_object_unref (image);
          image = destination;
        }
      /* This makes the old set wire-valid on the next Open unless the saved
       * inert disposition is independent of changing native ownership. */
      g_assert_cmpuint (gimp_painter_provenance_get_save_id (G_OBJECT (item)), ==, 0);
      gimp_item_set_tattoo (GIMP_ITEM (item), foreign_id);
      g_assert_cmpuint (gimp_item_get_tattoo (GIMP_ITEM (item)), ==, foreign_id);
      multipart_inert_check (G_OBJECT (item), GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM, expected);
      for (guint pass = 0; pass < 3; ++pass)
        {
          GimpImage *copy;
          g_assert_true (xcf_save_file_with_painter_transport (gimp, image, output, NULL, TRUE, NULL, &error));
          g_assert_no_error (error);
          /* The first Save adds inert carriers once; later files must not grow. */
          if (!pass) first_size = multipart_file_size (output);
          else g_assert_cmpint (multipart_file_size (output), ==, first_size);
          copy = load (output);
          g_object_unref (image);
          image = copy;
          item = layer (image, name);
          g_assert_nonnull (item);
          g_assert_cmpuint (gimp_item_get_tattoo (GIMP_ITEM (item)), ==, foreign_id);
          g_assert_false (GIMP_IS_CLONE_LAYER (item));
          g_assert_false (GIMP_IS_FILTER_LAYER (item));
          g_assert_cmpuint (multipart_inert_check (G_OBJECT (item), GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM, expected), >, 0);
          multipart_ordinary_check (G_OBJECT (item));
          pixel (item, 191, 32, 64, 255);
        }
      g_test_message ("inert ownership change=%u owner=%u stable-file=%" G_GOFFSET_FORMAT,
                      scene, foreign_id, first_size);
      g_object_unref (image);
    }
  g_ptr_array_unref (expected);
  g_file_delete (input, NULL, NULL);
  g_file_delete (output, NULL, NULL);
  g_object_unref (input);
  g_object_unref (output);
  g_assert_cmpuint (xcf_painter_storage_live_files (), ==, live_before);
}

static GBytes *
multipart_unknown_capsule (gboolean unknown_kind)
{
  const gsize size = 2 * 1024 * 1024 + 13;
  const guint8 magic[] = {'G', 'P', 'X', 'C', 'F', 0, 0, 0, 1, 0, 0, 0};
  guint8 *payload = g_malloc (size);
  for (gsize i = 0; i < size; ++i) payload[i] = (i * 19 + i / 251) & 255;
  if (unknown_kind)
    {
      GVariantBuilder builder;
      GVariant *value;
      GByteArray *wire = g_byte_array_new ();
      g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
      g_variant_builder_add (&builder, "{sv}", "version", g_variant_new_uint32 (1));
      g_variant_builder_add (&builder, "{sv}", "kind", g_variant_new_string ("future-multipart-layer"));
      g_variant_builder_add (&builder, "{sv}", "id", g_variant_new_uint32 (2));
      g_variant_builder_add (&builder, "{sv}", "future-payload",
                             g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, payload, size, 1));
      value = g_variant_ref_sink (g_variant_builder_end (&builder));
      g_assert_true (g_variant_is_normal_form (value));
#if G_BYTE_ORDER == G_BIG_ENDIAN
      { GVariant *swapped = g_variant_byteswap (value); g_variant_unref (value); value = swapped; }
#endif
      g_byte_array_append (wire, magic, sizeof magic);
      g_byte_array_append (wire, g_variant_get_data (value), g_variant_get_size (value));
      g_variant_unref (value);
      g_free (payload);
      return g_byte_array_free_to_bytes (wire);
    }
  memcpy (payload, magic, sizeof magic);
  payload[8] = 3; /* Future opaque version, distinct from the v2 manifest. */
  return g_bytes_new_take (payload, size);
}

static void
multipart_native_opaque_capsules (void)
{
  const guint live_before = xcf_painter_storage_live_files ();
  const guint flags = GIMP_PARASITE_PERSISTENT | GIMP_PARASITE_UNDOABLE;
  for (guint unknown_kind = 0; unknown_kind < 2; ++unknown_kind)
    {
      GBytes *expected = multipart_unknown_capsule (unknown_kind);
      GFile *input = synthetic_capsule_file (expected);
      GFile *output = temporary_file ();
      GimpImage *image;
      gchar *raw;
      gsize size, begin, wanted_size;
      const void *wanted = g_bytes_get_data (expected, &wanted_size);
      GError *error = NULL;
      goffset first_size = 0;
      guint changed = 0;

      /* Put the nondefault flags on the native input too, before its Open. */
      g_assert_true (g_file_load_contents (input, NULL, &raw, &size, NULL, &error));
      g_assert_no_error (error);
      begin = multipart_wire_layer_props ((const guint8 *) raw, size, "L");
      for (gsize at = begin; at < multipart_wire_props_end ((const guint8 *) raw, size, begin) - 8;)
        {
          guint32 type = multipart_wire_be32 ((const guint8 *) raw + at);
          guint32 length = multipart_wire_be32 ((const guint8 *) raw + at + 4);
          if (type == 21)
            for (gsize inner = at + 8; inner < at + 8 + length;)
              {
                guint32 name_size = multipart_wire_be32 ((const guint8 *) raw + inner);
                gsize count = multipart_wire_record_size ((const guint8 *) raw + inner, at + 8 + length - inner);
                if (!strcmp (raw + inner + 4, "gimp-painter-item"))
                  { wire32 ((guint8 *) raw, inner + 4 + name_size, flags); ++changed; }
                inner += count;
              }
          at += 8 + length;
        }
      g_assert_cmpuint (changed, ==, 1);
      g_assert_true (g_file_replace_contents (input, raw, size, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error));
      g_assert_no_error (error);
      g_free (raw);
      image = load (input);
      for (guint pass = 0; pass < 4; ++pass)
        {
          GimpLayer *item = layer (image, "L");
          const GimpParasite *parasite = gimp_item_parasite_find (GIMP_ITEM (item), "gimp-painter-item");
          const void *actual;
          guint32 actual_size;
          g_assert_nonnull (parasite);
          actual = gimp_parasite_get_data (parasite, &actual_size);
          g_assert_cmpmem (actual, actual_size, wanted, wanted_size);
          g_assert_cmpuint (gimp_parasite_get_flags (parasite), ==, flags);
          g_assert_false (GIMP_IS_CLONE_LAYER (item));
          g_assert_false (GIMP_IS_FILTER_LAYER (item));
          g_assert_false (gimp_painter_provenance_has_definition (G_OBJECT (item)));
          pixel (item, 9, 8, 7, 255);
          if (pass < 3)
            {
              GimpImage *copy;
              g_assert_true (xcf_save_file_with_painter_transport (gimp, image, output, NULL, TRUE, NULL, &error));
              g_assert_no_error (error);
              if (!pass) first_size = multipart_file_size (output);
              else g_assert_cmpint (multipart_file_size (output), ==, first_size);
              copy = load (output);
              g_object_unref (image);
              image = copy;
            }
        }
      g_test_message ("opaque native capsule kind=%u bytes=%" G_GSIZE_FORMAT " flags=%u stable-file=%" G_GOFFSET_FORMAT,
                      unknown_kind, wanted_size, flags, first_size);
      g_object_unref (image);
      g_bytes_unref (expected);
      g_file_delete (input, NULL, NULL);
      g_file_delete (output, NULL, NULL);
      g_object_unref (input);
      g_object_unref (output);
    }
  g_assert_cmpuint (xcf_painter_storage_live_files (), ==, live_before);
}

static void
multipart_named_parasite_attach (GObject *owner,
                                 const gchar *name,
                                 GBytes *bytes)
{
  gsize size;
  const void *data = g_bytes_get_data (bytes, &size);
  GimpParasite *parasite = gimp_parasite_new (name,
                                            GIMP_PARASITE_PERSISTENT | GIMP_PARASITE_UNDOABLE,
                                            size, data);
  if (GIMP_IS_IMAGE (owner)) gimp_image_parasite_attach (GIMP_IMAGE (owner), parasite, FALSE);
  else gimp_item_parasite_attach (GIMP_ITEM (owner), parasite, TRUE);
  gimp_parasite_free (parasite);
}

static void
multipart_named_parasite_check (GObject *owner,
                                const gchar *name,
                                GBytes *expected)
{
  const GimpParasite *parasite = GIMP_IS_IMAGE (owner) ?
    gimp_image_parasite_find (GIMP_IMAGE (owner), name) :
    gimp_item_parasite_find (GIMP_ITEM (owner), name);
  gsize wanted_size;
  guint32 actual_size;
  const void *wanted = g_bytes_get_data (expected, &wanted_size);
  const void *actual;
  g_assert_nonnull (parasite);
  actual = gimp_parasite_get_data (parasite, &actual_size);
  g_assert_cmpmem (actual, actual_size, wanted, wanted_size);
  g_assert_cmpuint (gimp_parasite_get_flags (parasite), ==,
                    GIMP_PARASITE_PERSISTENT | GIMP_PARASITE_UNDOABLE);
}

static void
multipart_native_unclaimed_parasites (void)
{
  const guint live_before = xcf_painter_storage_live_files ();
  const guint8 future[] = {'G', 'P', 'X', 'C', 'F', 0, 0, 0, 3, 0, 0, 0, 0xab, 0, 0xef};
  const guint8 rgba[] = {13, 77, 199, 255};
  GBytes *foreign = g_bytes_new_static (future, sizeof future);
  for (guint future_origin = 0; future_origin < 2; ++future_origin)
    {
      GimpImage *image = gimp_image_new (gimp, 1, 1, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
      GimpLayer *item = gimp_layer_new (image, 1, 1, babl_format ("R'G'B'A u8"),
                                        "ordinary owner", 1, GIMP_LAYER_MODE_NORMAL);
      GFile *file = temporary_file ();
      GBytes *origin;
      GError *error = NULL;
      if (future_origin) origin = g_bytes_ref (foreign);
      else
        {
          const guint8 magic[] = {'G', 'P', 'X', 'C', 'F', 0, 0, 0, 1, 0, 0, 0};
          GVariantBuilder builder;
          GVariant *value;
          GByteArray *wire = g_byte_array_new ();
          g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
          g_variant_builder_add (&builder, "{sv}", "version", g_variant_new_uint32 (1));
          g_variant_builder_add (&builder, "{sv}", "archive-marker", g_variant_new_string ("standalone-origin"));
          value = g_variant_ref_sink (g_variant_builder_end (&builder));
#if G_BYTE_ORDER == G_BIG_ENDIAN
          { GVariant *swapped = g_variant_byteswap (value); g_variant_unref (value); value = swapped; }
#endif
          g_byte_array_append (wire, magic, sizeof magic);
          g_byte_array_append (wire, g_variant_get_data (value), g_variant_get_size (value));
          g_variant_unref (value);
          origin = g_byte_array_free_to_bytes (wire);
        }
      gimp_image_add_layer (image, item, NULL, 0, FALSE);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (item)), GEGL_RECTANGLE (0, 0, 1, 1),
                       0, babl_format ("R'G'B'A u8"), rgba, GEGL_AUTO_ROWSTRIDE);
      multipart_named_parasite_attach (G_OBJECT (image), "gimp-painter-origin", origin);
      multipart_named_parasite_attach (G_OBJECT (item), "gimp-painter-origin", origin);
      /* The namespace is reserved, but this owner cannot replace its value. */
      multipart_named_parasite_attach (G_OBJECT (image), "gimp-painter-item", foreign);
      multipart_named_parasite_attach (G_OBJECT (item), "gimp-painter-image", foreign);
      for (guint pass = 0; pass < 3; ++pass)
        {
          GimpImage *copy;
          g_assert_true (xcf_save_file_with_painter_transport (gimp, image, file, NULL, TRUE, NULL, &error));
          g_assert_no_error (error);
          copy = load (file);
          g_object_unref (image);
          image = copy;
          item = layer (image, "ordinary owner");
          multipart_named_parasite_check (G_OBJECT (image), "gimp-painter-origin", origin);
          multipart_named_parasite_check (G_OBJECT (item), "gimp-painter-origin", origin);
          multipart_named_parasite_check (G_OBJECT (image), "gimp-painter-item", foreign);
          multipart_named_parasite_check (G_OBJECT (item), "gimp-painter-image", foreign);
          g_assert_false (GIMP_IS_CLONE_LAYER (item));
          g_assert_false (GIMP_IS_FILTER_LAYER (item));
          g_assert_true (gimp_painter_provenance_has_definition (G_OBJECT (item)));
          pixel (item, 13, 77, 199, 255);
        }
      g_object_unref (image);
      g_bytes_unref (origin);
      g_file_delete (file, NULL, NULL);
      g_object_unref (file);
    }
  g_bytes_unref (foreign);
  g_assert_cmpuint (xcf_painter_storage_live_files (), ==, live_before);
}
