#pragma once

#include <arisa/composition/identity.hpp>
#include <arisa/composition/node_anchor.hpp>
#include <arisa/composition/state_slot.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <utility>

namespace arisa::composition {

template <class T>
class StateHandle {
 public:
  auto get() const noexcept -> const T& { return slot_->value(); }

  auto identity() const noexcept -> Identity { return Identity{slot_.get()}; }

  void set(T value) const {
    if constexpr (std::equality_comparable<T>) {
      if (slot_->value() == value) {
        return;
      }
    }
    slot_->value() = std::move(value);
    changed();
  }

  template <std::invocable<T&> Mutation>
  void update(Mutation&& mutation) const {
    std::invoke(std::forward<Mutation>(mutation), slot_->value());
    changed();
  }

 private:
  friend class Context;

  StateHandle(std::shared_ptr<StateSlot::Typed<T>> slot,
              std::weak_ptr<const NodeAnchor> anchor) noexcept
      : slot_(std::move(slot)), anchor_(std::move(anchor)) {}

  void changed() const {
    if (const auto anchor = anchor_.lock()) {
      anchor->mark_dirty();
    }
  }

  std::shared_ptr<StateSlot::Typed<T>> slot_;
  std::weak_ptr<const NodeAnchor> anchor_;
};

}
