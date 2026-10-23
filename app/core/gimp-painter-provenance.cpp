/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gio/gio.h>
extern "C" {
#include "gimp-painter-provenance.h"
#include "gimp-painter-provenance-private.h"
}
#include "painter/binding-store.hpp"
#include "painter/bytes.hpp"
#include "painter/resources.hpp"
#include <array>
#include <memory>
#include <utility>

/* This owner has no embedded C++ implementation or parallel qdata. The native
 * GimpObject owns one typed child so its already-active derived stores are
 * never reopened for late registration during XCF loading. */
struct GimpPainterProvenance { GObject parent; };
struct GimpPainterProvenanceClass { GObjectClass parent; };
G_DEFINE_TYPE (GimpPainterProvenance, gimp_painter_provenance, G_TYPE_OBJECT)
namespace GimpPainter {
template<> struct TypeTraits<GimpPainterProvenance> {
  static GType type () { return gimp_painter_provenance_get_type (); }
};
}
namespace {
using namespace GimpPainter;
struct VariantFree { void operator() (GVariant *p) const noexcept { if (p) g_variant_unref (p); } };
struct RecordsFree { void operator() (GPtrArray *p) const noexcept { if (p) g_ptr_array_unref (p); } };
using Variant = std::unique_ptr<GVariant, VariantFree>;
using Records = std::unique_ptr<GPtrArray, RecordsFree>;
using Object = ObjectRef<GObject>;
Records snapshot_records (GPtrArray *source)
{
  if (!source) return {};
  Records copy (g_ptr_array_new_with_free_func ([] (gpointer data) {
    g_bytes_unref (static_cast<GBytes *> (data));
  }));
  for (guint i = 0; i < source->len; ++i)
    {
      auto *bytes = static_cast<GBytes *> (g_ptr_array_index (source, i));
      if (!bytes) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Null provenance record");
      g_ptr_array_add (copy.get (), g_bytes_ref (bytes));
    }
  return copy;
}
struct Transport {
  Records records;
  Bytes dispositions;
  std::array<Bytes, GIMP_PAINTER_PROVENANCE_N_NAMESPACES> capsules;
  std::array<bool, GIMP_PAINTER_PROVENANCE_N_NAMESPACES> present {};
  std::array<bool, GIMP_PAINTER_PROVENANCE_N_NAMESPACES> invalid {};
};
using TransportValue = std::unique_ptr<Transport>;
TransportValue snapshot_transport (const GimpPainterProvenanceTransport *source)
{
  if (!source) return {};
  const gsize count = source->records ? source->records->len : 0;
  const gsize size = source->dispositions ? g_bytes_get_size (source->dispositions) : 0;
  if (count != size)
    throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Transport record dispositions have the wrong length");
  for (guint n = 0; n < GIMP_PAINTER_PROVENANCE_N_NAMESPACES; ++n)
    if (source->capsules[n] && (!source->present[n] || source->invalid[n]))
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Transport capsule has no valid namespace");
  const auto *dispositions = source->dispositions
    ? static_cast<const guint8 *> (g_bytes_get_data (source->dispositions, nullptr)) : nullptr;
  for (gsize i = 0; i < size; ++i)
    if (dispositions[i] > GIMP_PAINTER_PROVENANCE_TRANSPORT_ORIGIN ||
        (dispositions[i] && !source->capsules[dispositions[i] - 1]))
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Invalid transport record disposition");
  TransportValue copy (new Transport);
  copy->records = snapshot_records (source->records);
  copy->dispositions = Bytes::retain (source->dispositions);
  for (guint n = 0; n < GIMP_PAINTER_PROVENANCE_N_NAMESPACES; ++n)
    {
      copy->capsules[n] = Bytes::retain (source->capsules[n]);
      copy->present[n] = source->present[n];
      copy->invalid[n] = source->invalid[n];
    }
  return copy;
}
TransportValue snapshot_transport (const Transport *source)
{
  if (!source) return {};
  TransportValue copy (new Transport);
  copy->records = snapshot_records (source->records.get ());
  copy->dispositions = source->dispositions;
  copy->capsules = source->capsules;
  copy->present = source->present;
  copy->invalid = source->invalid;
  return copy;
}
struct PublicTransportFree {
  void operator() (GimpPainterProvenanceTransport *p) const noexcept
  { gimp_painter_provenance_free_transport (p); }
};
GimpPainterProvenanceTransport *export_transport (const Transport *source)
{
  if (!source) return nullptr;
  std::unique_ptr<GimpPainterProvenanceTransport, PublicTransportFree> result (
    g_new0 (GimpPainterProvenanceTransport, 1));
  result->records = snapshot_records (source->records.get ()).release ();
  result->dispositions = source->dispositions ? g_bytes_ref (source->dispositions.get ()) : nullptr;
  for (guint n = 0; n < GIMP_PAINTER_PROVENANCE_N_NAMESPACES; ++n)
    {
      result->capsules[n] = source->capsules[n] ? g_bytes_ref (source->capsules[n].get ()) : nullptr;
      result->present[n] = source->present[n];
      result->invalid[n] = source->invalid[n];
    }
  return result.release ();
}
struct Provenance {
  std::array<Bytes, GIMP_PAINTER_PROVENANCE_N_BYTES> bytes;
  std::array<String, GIMP_PAINTER_PROVENANCE_N_TEXT> text;
  Records records;
  TransportValue transport;
  Variant definition;
  guint64 offset = 0;
  guint32 save_id = 0;
  gint dialect = 0;
  bool has_offset = false;
  void close () noexcept {} // immutable values stay readable until finalization
};
struct ProvenanceSlot : SlotSpec<GimpPainterProvenance, Provenance> {};
Object lease_owner (GObject *owner, bool writing)
{
  auto lease = Object::adopt (_gimp_object_lease_painter_provenance_owner (owner, writing));
  if (!lease) throw Error (GIMP_PAINTER_ERROR_CLOSED, "Provenance owner is unavailable");
  return lease;
}
Object component (GObject *owner, bool create)
{
  auto lease = lease_owner (owner, create);
  auto child = Object::adopt (_gimp_object_ref_painter_provenance (owner));
  if (!child && create)
    {
      child = Object::adopt (G_OBJECT (g_object_new (gimp_painter_provenance_get_type (), nullptr)));
      if (!_gimp_object_attach_painter_provenance (owner, child.get ()))
        throw Error (GIMP_PAINTER_ERROR_CLOSED, "Provenance owner is closed");
    }
  return child;
}
template<class R, class F> R read (GObject *owner, R fallback, F fn)
{
  return boundary<R> (nullptr, fallback, [&] {
    auto child = component (owner, false);
    return child ? BindingStore::require (child.get ()).read<ProvenanceSlot> (fn) : fallback;
  });
}
template<class F> gboolean change (GObject *owner, F fn)
{
  return boundary<gboolean> (nullptr, FALSE, [&] {
    auto lease = lease_owner (owner, true);
    auto child = component (owner, true);
    BindingStore::require (child.get ()).with<ProvenanceSlot> (fn);
    return TRUE;
  });
}
void dispose (GObject *owner)
{
  boundary_void (nullptr, [&] { if (auto *store = BindingStore::find (owner)) store->close (); });
  G_OBJECT_CLASS (gimp_painter_provenance_parent_class)->dispose (owner);
}
}
static void gimp_painter_provenance_class_init (GimpPainterProvenanceClass *klass)
{ G_OBJECT_CLASS (klass)->dispose = dispose; }
static void gimp_painter_provenance_init (GimpPainterProvenance *self)
{
  GimpPainter::boundary_void (nullptr, [&] {
    auto& store = GimpPainter::BindingStore::ensure (G_OBJECT (self));
    store.emplace<ProvenanceSlot> ();
    store.activate ();
  });
}
GBytes *gimp_painter_provenance_ref_bytes (GObject *owner, GimpPainterProvenanceBytes field)
{
  if (field < 0 || field >= GIMP_PAINTER_PROVENANCE_N_BYTES) return nullptr;
  return read<GBytes *> (owner, nullptr, [=] (const Provenance& s) { return s.bytes[field] ? g_bytes_ref (s.bytes[field].get ()) : nullptr; });
}
gboolean gimp_painter_provenance_set_bytes (GObject *owner, GimpPainterProvenanceBytes field, GBytes *value)
{
  if (field < 0 || field >= GIMP_PAINTER_PROVENANCE_N_BYTES) return FALSE;
  return change (owner, [=] (Provenance& s) { auto next = Bytes::retain (value); s.bytes[field].swap (next); });
}
gchar *gimp_painter_provenance_dup_text (GObject *owner, GimpPainterProvenanceText field)
{
  if (field < 0 || field >= GIMP_PAINTER_PROVENANCE_N_TEXT) return nullptr;
  return read<gchar *> (owner, nullptr, [=] (const Provenance& s) { return g_strdup (s.text[field].get ()); });
}
gboolean gimp_painter_provenance_set_text (GObject *owner, GimpPainterProvenanceText field, const gchar *value)
{
  if (field < 0 || field >= GIMP_PAINTER_PROVENANCE_N_TEXT) return FALSE;
  return change (owner, [=] (Provenance& s) { String next (g_strdup (value)); s.text[field].swap (next); });
}
GPtrArray *gimp_painter_provenance_ref_records (GObject *owner)
{ return read<GPtrArray *> (owner, nullptr, [] (const Provenance& s) { return snapshot_records (s.records.get ()).release (); }); }
gboolean gimp_painter_provenance_set_records (GObject *owner, GPtrArray *value)
{ return change (owner, [=] (Provenance& s) { auto next = snapshot_records (value); s.records.swap (next); }); }
GimpPainterProvenanceTransport *gimp_painter_provenance_ref_transport (GObject *owner)
{
  return read<GimpPainterProvenanceTransport *> (owner, nullptr, [] (const Provenance& s) {
    return export_transport (s.transport.get ());
  });
}
void gimp_painter_provenance_free_transport (GimpPainterProvenanceTransport *value)
{
  if (!value) return;
  /* Remove every field before callbacks from any final resource release. */
  const auto old = *value;
  *value = {};
  if (old.records) g_ptr_array_unref (old.records);
  if (old.dispositions) g_bytes_unref (old.dispositions);
  for (auto *capsule : old.capsules) if (capsule) g_bytes_unref (capsule);
  g_free (value);
}
gboolean gimp_painter_provenance_set_transport (GObject *owner, const GimpPainterProvenanceTransport *value)
{
  return boundary<gboolean> (nullptr, FALSE, [&] {
    auto next = snapshot_transport (value);
    return change (owner, [&] (Provenance& s) {
      s.transport.swap (next);
      /* Keep owner/child leased while user GBytes finalizers can reenter. */
      next.reset ();
    });
  });
}
GVariant *gimp_painter_provenance_ref_definition (GObject *owner)
{ return read<GVariant *> (owner, nullptr, [] (const Provenance& s) { return s.definition ? g_variant_ref (s.definition.get ()) : nullptr; }); }
gboolean gimp_painter_provenance_has_definition (GObject *owner)
{ return read<gboolean> (owner, FALSE, [] (const Provenance& s) { return s.definition != nullptr; }); }
gboolean gimp_painter_provenance_set_definition (GObject *owner, GVariant *value)
{ return change (owner, [=] (Provenance& s) { Variant next (value ? g_variant_ref (value) : nullptr); s.definition.swap (next); }); }
gboolean gimp_painter_provenance_get_offset (GObject *owner, guint64 *offset)
{ return read<gboolean> (owner, FALSE, [=] (const Provenance& s) { if (s.has_offset && offset) *offset = s.offset; return s.has_offset; }); }
gboolean gimp_painter_provenance_set_offset (GObject *owner, guint64 value)
{ return change (owner, [=] (Provenance& s) { s.offset = value; s.has_offset = true; }); }
gint gimp_painter_provenance_get_dialect (GObject *owner)
{ return read<gint> (owner, 0, [] (const Provenance& s) { return s.dialect; }); }
gboolean gimp_painter_provenance_set_dialect (GObject *owner, gint value)
{ return change (owner, [=] (Provenance& s) { s.dialect = value; }); }
guint32 gimp_painter_provenance_get_save_id (GObject *owner)
{ return read<guint32> (owner, 0, [] (const Provenance& s) { return s.save_id; }); }
gboolean gimp_painter_provenance_set_save_id (GObject *owner, guint32 value)
{ return change (owner, [=] (Provenance& s) { s.save_id = value; }); }
void gimp_painter_copy_provenance (GObject *source, GObject *target)
{
  boundary_void (nullptr, [&] {
    auto source_lease = lease_owner (source, false), target_lease = lease_owner (target, true);
    auto child = component (source, false);
    if (!child || source == target) return;
    /* Snapshot everything before any previous target resource is released.
     * A GBytes finalizer can reenter, close or unref either native owner. */
    auto copy = BindingStore::require (child.get ()).read<ProvenanceSlot> ([] (const Provenance& s) {
      std::unique_ptr<Provenance> result (new Provenance);
      for (int i = 0; i < GIMP_PAINTER_PROVENANCE_ORIGINAL; ++i)
        if (s.bytes[i]) result->bytes[i] = Bytes::retain (s.bytes[i].get ());
      for (int i = 0; i < GIMP_PAINTER_PROVENANCE_N_TEXT; ++i)
        result->text[i].reset (g_strdup (s.text[i].get ()));
      result->records = snapshot_records (s.records.get ());
      result->transport = snapshot_transport (s.transport.get ());
      return result;
    });
    change (target, [&] (Provenance& s) {
      for (int i = 0; i < GIMP_PAINTER_PROVENANCE_ORIGINAL; ++i)
        if (copy->bytes[i]) s.bytes[i].swap (copy->bytes[i]);
      for (int i = 0; i < GIMP_PAINTER_PROVENANCE_N_TEXT; ++i)
        if (copy->text[i]) s.text[i].swap (copy->text[i]);
      if (copy->records) s.records.swap (copy->records);
      if (copy->transport) s.transport.swap (copy->transport);
    });
  });
}
