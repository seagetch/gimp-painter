/* Real GType checks, typed slot, virtual close and public C bridge without RTTI. */
#include "binding-store.hpp"
#include "gimp-painter-binding.h"
using namespace GimpPainter;
struct InitiallyUnownedOwner;
namespace GimpPainter {
template<> struct TypeTraits<InitiallyUnownedOwner>
{ static GType type () noexcept { return G_TYPE_INITIALLY_UNOWNED; } };
}
namespace {
struct Implementation {
  Implementation (int& count) : closed (count) {}
  void close () noexcept { ++closed; }
  int& closed;
};
struct Slot : SlotSpec<GObject, Implementation> {};
}
int main ()
{
  int closed = 0;
  auto owner = ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  auto copy = ObjectRef<GObject>::retain (owner.get ());
  bool wrong_type_rejected = false;
  try {
    auto wrong = ObjectRef<InitiallyUnownedOwner>::retain (
      reinterpret_cast<InitiallyUnownedOwner *> (owner.get ()));
    (void) wrong;
  } catch (const Error& error) {
    wrong_type_rejected = error.code () == GIMP_PAINTER_ERROR_WRONG_TYPE;
  }
  if (!wrong_type_rejected || copy.get () != owner.get ()) return 1;
  auto& store = BindingStore::ensure (owner.get ());
  store.emplace<Slot> (closed);
  store.activate ();
  if (store.with<Slot> ([] (Implementation& impl) { return impl.closed; }) != 0) return 2;
  GError *error = nullptr;
  if (!gimp_painter_binding_close (owner.get (), &error) || error || closed != 1) return 3;
  if (!gimp_painter_binding_close (owner.get (), &error) || error || closed != 1) return 4;
  copy.reset ();
  owner.reset ();
  return closed == 1 ? 0 : 5;
}
