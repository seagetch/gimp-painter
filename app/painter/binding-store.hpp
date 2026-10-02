/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_BINDING_STORE_HPP
#define GIMP_PAINTER_BINDING_STORE_HPP

#include "object-ref.hpp"
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace GimpPainter {

/* Declare each slot as a distinct type deriving from SlotSpec<Owner, Impl>.
 * Its identity cannot be supplied independently of these two types. */
template<class OwnerType, class Implementation>
struct SlotSpec
{
  using Owner = OwnerType;
  using Impl = Implementation;
  static GType owner_type () { return TypeTraits<Owner>::type (); }
};

class BindingStore
{
public:
  enum class State { constructing, active, closing, closed };
  static BindingStore& ensure (GObject *owner);
  static BindingStore *find (GObject *owner) noexcept;
  static BindingStore& require (GObject *owner);

  BindingStore (const BindingStore&) = delete;
  BindingStore& operator= (const BindingStore&) = delete;
  ~BindingStore () noexcept;

  /* Only construction adapters may register; lookup never creates a slot. */
  template<class Slot, class... Args>
  void emplace (Args&&... args)
  {
    check_thread ();
    if (state_ != State::constructing)
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Registration after construction");
    if (!G_TYPE_CHECK_INSTANCE_TYPE (owner_, Slot::owner_type ()))
      throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Slot owner has the wrong GType");
    const auto *key = identity<Slot> ();
    if (entry (key) || constructing_slot_ == key)
      throw Error (GIMP_PAINTER_ERROR_DUPLICATE_SLOT, "Slot is already registered or constructing");
    if (constructing_slot_)
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Nested slot construction is not allowed");
    auto owner_lease = ObjectRef<GObject>::retain (owner_);
    struct Pending
    {
      explicit Pending (const void *&target, const void *key) : target (target) { target = key; }
      ~Pending () { target = nullptr; }
      const void *&target;
    } pending (constructing_slot_, key);
    /* A constructor can synchronously close the owner or release its caller's
     * last ref. The lease, reservation, and post-construction check keep that
     * from publishing an invalid or duplicate slot into a destroyed store. */
    std::unique_ptr<EntryBase> value (new Entry<Slot> (std::forward<Args> (args)...));
    if (state_ != State::constructing)
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Owner closed during slot construction");
    entries_.push_back (std::move (value));
  }

  /* Explicit construction-property path; never enables normal callbacks. */
  template<class Slot, class Function>
  auto initialize (Function&& function) -> decltype (function (std::declval<typename Slot::Impl&> ()))
  {
    check_thread ();
    if (state_ != State::constructing)
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Initialization after construction");
    auto owner_lease = ObjectRef<GObject>::retain (owner_);
    return function (lookup<Slot> ());
  }

  void activate ();
  void close ();
  State state () const { check_thread (); return state_; }
  std::uint64_t generation () const { check_thread (); return generation_; }
  bool accepts (std::uint64_t generation) const
  { check_thread (); return state_ == State::active && generation_ == generation; }

  /* Never return Impl*, a reference, or a closure retaining the supplied borrow.
   * The lease makes signal reentry, close and caller-owned-ref loss safe. */
  template<class Slot, class Function>
  auto with (Function&& function) -> decltype (function (std::declval<typename Slot::Impl&> ()))
  {
    check_thread ();
    if (state_ != State::active)
      throw Error (state_ == State::constructing ? GIMP_PAINTER_ERROR_INVALID_STATE
                                                : GIMP_PAINTER_ERROR_CLOSED,
                   "Binding is not active");
    auto owner_lease = ObjectRef<GObject>::retain (owner_);
    return function (lookup<Slot> ());
  }

  template<class Slot, class Function>
  auto read (Function&& function) -> decltype (function (std::declval<const typename Slot::Impl&> ()))
  {
    check_thread ();
    if (finalizing_)
      throw Error (GIMP_PAINTER_ERROR_CLOSED, "Owner is finalizing");
    auto owner_lease = ObjectRef<GObject>::retain (owner_);
    return function (static_cast<const typename Slot::Impl&> (lookup<Slot> ()));
  }

private:
  struct EntryBase
  {
    explicit EntryBase (const void *identity) : id (identity) {}
    virtual ~EntryBase () = default;
    virtual void close () noexcept = 0;
    const void *id;
  };
  template<class Slot> struct Entry : EntryBase
  {
    template<class... Args> explicit Entry (Args&&... args)
      : EntryBase (identity<Slot> ()), impl (new typename Slot::Impl (std::forward<Args> (args)...))
    {
      static_assert (noexcept (impl->close ()), "Slot close must be noexcept and nonblocking");
      static_assert (std::is_nothrow_destructible<typename Slot::Impl>::value,
                     "Slot destructor must not throw");
    }
    ~Entry () noexcept override { close (); }
    void close () noexcept override
    { if (!closed) { closed = true; impl->close (); } }
    std::unique_ptr<typename Slot::Impl> impl;
    bool closed = false;
  };

  template<class Slot> static const void *identity () noexcept
  { static const char token = 0; return &token; }
  template<class Slot> typename Slot::Impl& lookup ()
  {
    auto *found = entry (identity<Slot> ());
    if (!found)
      throw Error (GIMP_PAINTER_ERROR_MISSING_SLOT, "Required slot is not registered");
    return *static_cast<Entry<Slot> *> (found)->impl;
  }
  explicit BindingStore (GObject *owner) noexcept;
  EntryBase *entry (const void *identity) const noexcept;
  void check_thread () const;
  void close_unchecked () noexcept;

  GObject *owner_;                 // owner owns this store, never a strong cycle
  GThread *thread_;                // retained identity, even if creator exits
  const void *constructing_slot_ = nullptr;
  bool finalizing_ = false;
  State state_ = State::constructing;
  std::uint64_t generation_ = 0;
  std::vector<std::unique_ptr<EntryBase>> entries_;
};

} // namespace GimpPainter
#endif
