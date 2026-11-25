#pragma once

#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/with_animation.hpp>

#include <memory>
#include <string_view>
#include <utility>

namespace arisa::composition {

template <class T>
class GestureStateHandle;

template <class T>
class GestureState {
 public:
  using Reset = void (*)(const T&, motion::Transaction&);
  using ResetTransaction = auto (*)() -> motion::Transaction;

  constexpr explicit GestureState(std::string_view name, T initial = T{})
      : key_(name, std::move(initial)) {}

  constexpr GestureState(std::string_view name, T initial, Reset on_reset)
      : key_(name, std::move(initial)), reset_(on_reset) {}

  constexpr GestureState(std::string_view name, T initial,
                         ResetTransaction transaction)
      : key_(name, std::move(initial)), transaction_(transaction) {}

  constexpr GestureState(std::string_view name, Reset on_reset)
      : GestureState(name, T{}, on_reset) {}

  constexpr GestureState(std::string_view name, ResetTransaction transaction)
      : GestureState(name, T{}, transaction) {}

  GestureState(const GestureState&) = delete;
  auto operator=(const GestureState&) -> GestureState& = delete;
  GestureState(GestureState&&) = delete;
  auto operator=(GestureState&&) -> GestureState& = delete;
  ~GestureState() = default;

  constexpr auto key() const noexcept -> const StateKey<T>& { return key_; }

  constexpr auto reset() const noexcept -> Reset { return reset_; }

  constexpr auto reset_transaction() const noexcept -> ResetTransaction {
    return transaction_;
  }

 private:
  StateKey<T> key_;
  Reset reset_{nullptr};
  ResetTransaction transaction_{nullptr};
};

template <class T>
class GestureStateHandle {
 public:
  GestureStateHandle(StateHandle<T> handle, T initial)
      : handle_(std::move(handle)),
        initial_(std::make_shared<const T>(std::move(initial))),
        reset_(std::make_shared<motion::Transaction>()) {}

  GestureStateHandle(StateHandle<T> handle, const GestureState<T>& state)
      : GestureStateHandle(std::move(handle), state.key().initial()) {
    custom_ = state.reset();
    transaction_ = state.reset_transaction();
  }

  auto get() const noexcept -> const T& { return handle_.get(); }

  template <class Body>
  void update(Body& body, const auto& value) const {
    T next = handle_.get();
    motion::Transaction transaction;
    body(value, next, transaction);
    *reset_ = transaction;
    motion::with_transaction(transaction,
                             [this, &next] { handle_.set(std::move(next)); });
  }

  void reset() const {
    motion::Transaction transaction =
        transaction_ != nullptr ? transaction_() : *reset_;
    if (custom_ != nullptr) {
      custom_(handle_.get(), transaction);
    }
    motion::with_transaction(transaction, [this] { handle_.set(*initial_); });
  }

 private:
  StateHandle<T> handle_;
  std::shared_ptr<const T> initial_;
  std::shared_ptr<motion::Transaction> reset_;
  typename GestureState<T>::Reset custom_{nullptr};
  typename GestureState<T>::ResetTransaction transaction_{nullptr};
};

}
