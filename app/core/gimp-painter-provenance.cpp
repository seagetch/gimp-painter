/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gio/gio.h>
extern "C" {
#include "gimp-painter-provenance.h"
#include "gimp-painter-provenance-private.h"
}
#include "painter/binding-store.hpp"
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
struct BytesFree { void operator() (GBytes *p) const noexcept { if (p) g_bytes_unref (p); } };
struct VariantFree { void operator() (GVariant *p) const noexcept { if (p) g_variant_unref (p); } };
struct RecordsFree { void operator() (GPtrArray *p) const noexcept { if (p) g_ptr_array_unref (p); } };
using Bytes = std::unique_ptr<GBytes, BytesFree>;
using Variant = std::unique_ptr<GVariant, VariantFree>;
using Records = std::unique_ptr<GPtrArray, RecordsFree>;
using Object = ObjectRef<GObject>;
Records snapshot_records (GPtrArray *source)
{
  if (!source) return {};
  Records copy (g_ptr_array_new_with_free_func (reinterpret_cast<GDestroyNotify> (g_bytes_unref)));
  for (guint i = 0; i < source->len; ++i)
    {
      auto *bytes = static_cast<GBytes *> (g_ptr_array_index (source, i));
      if (!bytes) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Null provenance record");
      g_ptr_array_add (copy.get (), g_bytes_ref (bytes));
    }
  return copy;
}
struct Provenance {
  std::array<Bytes, GIMP_PAINTER_PROVENANCE_N_BYTES> bytes;
  std::array<String, GIMP_PAINTER_PROVENANCE_N_TEXT> text;
  Records records;
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
  return change (owner, [=] (Provenance& s) { Bytes next (value ? g_bytes_ref (value) : nullptr); s.bytes[field].swap (next); });
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
        if (s.bytes[i]) result->bytes[i].reset (g_bytes_ref (s.bytes[i].get ()));
      for (int i = 0; i < GIMP_PAINTER_PROVENANCE_N_TEXT; ++i)
        result->text[i].reset (g_strdup (s.text[i].get ()));
      result->records = snapshot_records (s.records.get ());
      return result;
    });
    change (target, [&] (Provenance& s) {
      for (int i = 0; i < GIMP_PAINTER_PROVENANCE_ORIGINAL; ++i)
        if (copy->bytes[i]) s.bytes[i].swap (copy->bytes[i]);
      for (int i = 0; i < GIMP_PAINTER_PROVENANCE_N_TEXT; ++i)
        if (copy->text[i]) s.text[i].swap (copy->text[i]);
      if (copy->records) s.records.swap (copy->records);
    });
  });
}
