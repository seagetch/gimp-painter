/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FILTER_LAYER_HANDLE_HPP
#define GIMP_FILTER_LAYER_HANDLE_HPP
#include "../painter/gimp-painter-visibility.h"
extern "C" {
#include "gimpfilterlayer.h"
}
#include "gimp-painter-type-traits.hpp"
#include "painter/resources.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
class FilterLayerRef
{
public:
  struct DefinitionFree
  { void operator() (GBytes *value) const noexcept { g_bytes_unref (value); } };
  struct ArgumentsSnapshotFree
  { void operator() (GimpFilterArgumentsSnapshot *value) const noexcept
    { gimp_filter_arguments_snapshot_free (value); } };
  struct ArgumentsFree
  { void operator() (GimpValueArray *value) const noexcept { gimp_value_array_unref (value); } };
  using Definition = std::unique_ptr<GBytes, DefinitionFree>;
  using ArgumentsSnapshot = std::unique_ptr<GimpFilterArgumentsSnapshot, ArgumentsSnapshotFree>;
  using Arguments = std::unique_ptr<GimpValueArray, ArgumentsFree>;
  static FilterLayerRef retain (GimpFilterLayer *layer)
  { return FilterLayerRef (ObjectRef<GimpFilterLayer>::retain (layer)); }
  static FilterLayerRef adopt (GimpFilterLayer *layer)
  { return FilterLayerRef (ObjectRef<GimpFilterLayer>::adopt (layer)); }
  static FilterLayerRef sink (GimpFilterLayer *layer)
  { return FilterLayerRef (ObjectRef<GimpFilterLayer>::sink (layer)); }
  static FilterLayerRef create (GimpImage *image, int width, int height, const char *name,
                               double opacity, GimpLayerMode mode)
  {
    auto *layer = gimp_filter_layer_new (image, width, height, name, opacity, mode);
    if (!layer) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "FilterLayer construction failed");
    /* GimpItem factories return a floating reference. Own it before a later
     * image insertion can sink it on behalf of the container. */
    return sink (GIMP_FILTER_LAYER (layer));
  }
  GimpFilterLayer *get () const noexcept { return owner_.get (); }
  /* Checked Blinds/Small Tiles preparation, with the C entry's finite scalar/provider and
   * post-publication reentry contract. These are not old void-setter aliases. */
  gboolean try_set_legacy_procedure (const char *procedure, const GArray *arguments,
                                      GError **error) const noexcept
  { return gimp_filter_layer_set_legacy_procedure (get (), procedure, arguments, error); }
  void set_legacy_procedure (const char *procedure, const GArray *arguments) const
  {
    GError *error = nullptr;
    if (!gimp_filter_layer_set_legacy_procedure (get (), procedure, arguments, &error))
      {
        const auto code = error ? static_cast<GimpPainterError> (error->code) : GIMP_PAINTER_ERROR_INVALID_STATE;
        throw Error (code, take_error_message (error, "FilterLayer legacy preparation failed").c_str ());
      }
  }
  void set_definition (const char *procedure, GBytes *raw, const GimpValueArray *args) const
  {
    GError *error = nullptr;
    if (!gimp_filter_layer_set_definition (get (), procedure, raw, args, &error))
      {
        const auto code = error ? static_cast<GimpPainterError> (error->code) : GIMP_PAINTER_ERROR_INVALID_STATE;
        throw Error (code, take_error_message (error, "FilterLayer definition failed").c_str ());
      }
  }
  /* Preserve the C result/error contract. Checks and their data are borrowed
   * only for this synchronous call; snapshot identity and revision pass through. */
  gboolean try_set_definition (const char *procedure, GBytes *raw,
                               const GimpValueArray *args, GError **error) const noexcept
  { return gimp_filter_layer_set_definition (get (), procedure, raw, args, error); }
  gboolean try_edit_definition (const char *procedure, GBytes *raw,
                                const GimpValueArray *args, GError **error) const noexcept
  { return gimp_filter_layer_edit_definition (get (), procedure, raw, args, error); }
  gboolean try_set_definition_checked (const char *procedure, GBytes *raw,
                                       const GimpValueArray *args, gboolean push_undo,
                                       gboolean (*check) (gpointer), gpointer check_data,
                                       GError **error) const noexcept
  { return gimp_filter_layer_set_definition_checked (get (), procedure, raw, args,
                                                     push_undo, check, check_data, error); }
  gboolean try_edit_argument_patch (guint64 expected_revision,
                                    const GimpFilterArgumentsSnapshot *snapshot,
                                    guint n_patches, const GimpFilterArgumentPatch *patches,
                                    gboolean (*check) (gpointer), gpointer check_data,
                                    GError **error) const noexcept
  { return gimp_filter_layer_edit_argument_patch (get (), expected_revision, snapshot,
                                                  n_patches, patches, check, check_data, error); }
  /* Original-name query routes over the saved native model. Copy a borrowed
   * name immediately; adopt the independently owned GArray returned by C. */
  String get_procedure () const
  { return String (g_strdup (gimp_filter_layer_get_procedure (get ()))); }
  ArrayRef get_procedure_args () const
  { return ArrayRef::adopt (gimp_filter_layer_get_procedure_args (get ())); }
  /* Read handles own exactly the C provider's result. Prefix limits are bytes;
   * repairing a split UTF-8 sequence is a display concern, not a model edit.
   * A snapshot does not materialize arguments or retain the layer itself. */
  String procedure_prefix (gsize max_bytes, gboolean *truncated = nullptr) const
  { return String (gimp_filter_layer_dup_procedure_prefix (get (), max_bytes, truncated)); }
  Definition definition () const
  { return Definition (gimp_filter_layer_ref_definition (get ())); }
  ArgumentsSnapshot snapshot_arguments () const
  { return ArgumentsSnapshot (gimp_filter_layer_snapshot_arguments (get ())); }
  Arguments arguments () const
  { return Arguments (gimp_filter_layer_dup_args (get ())); }
  guint64 definition_revision () const noexcept
  { return gimp_filter_layer_get_definition_revision (get ()); }
  /* Native cache certification/cancellation, not the historical one-shot flag. */
  void mark_as_loaded () const noexcept { gimp_filter_layer_mark_as_loaded (get ()); }
  String procedure () const { return String (gimp_filter_layer_dup_procedure (get ())); }
  GimpFilterLayerState state () const noexcept { return gimp_filter_layer_get_state (get ()); }
  void cancel () const noexcept { gimp_filter_layer_cancel (get ()); }
private:
  explicit FilterLayerRef (ObjectRef<GimpFilterLayer> owner) : owner_ (std::move (owner))
  { if (!owner_) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected FilterLayer"); }
  ObjectRef<GimpFilterLayer> owner_;
};
} // namespace GimpPainter
#endif
