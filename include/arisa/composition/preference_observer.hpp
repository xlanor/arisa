#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/preference_key.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::composition {

class PreferenceObserver {
 public:
  template <class T>
  class Typed;

  virtual ~PreferenceObserver() = default;

  virtual auto key() const noexcept -> Identity = 0;

  virtual auto resolve(const Node& node) -> bool = 0;

  virtual void deliver() = 0;

  virtual void adopt(const PreferenceObserver& previous) = 0;
};

template <class T>
class PreferenceObserver::Typed final : public PreferenceObserver {
 public:
  using Action = std::move_only_function<void(const T&)>;

  Typed(const PreferenceKey<T>& key, Action action)
      : key_(key), action_(std::move(action)), last_(key.default_value()) {}

  auto key() const noexcept -> Identity override {
    return key_.get().identity();
  }

  auto resolve(const Node& node) -> bool override {
    T value = node.preference(key_.get()).value_or(key_.get().default_value());
    if (last_ == value) {
      return false;
    }
    last_ = std::move(value);
    return true;
  }

  void deliver() override {
    if (action_) {
      action_(last_);
    }
  }

  void adopt(const PreferenceObserver& previous) override {
    last_ = static_cast<const Typed&>(previous).last_;
  }

 private:
  std::reference_wrapper<const PreferenceKey<T>> key_;
  Action action_;
  T last_;
};

template <class T, class Action>
  requires std::equality_comparable<T> && std::invocable<Action&, const T&>
auto Element::on_preference_change(const PreferenceKey<T>& key,
                                   Action action) && -> Element {
  if (key.demand() != nullptr) {
    *this = std::move(*this).environment(*key.demand(), true);
  }
  environment_.edit().preference_observers.push_back(
      std::make_shared<PreferenceObserver::Typed<T>>(
          key,
          typename PreferenceObserver::Typed<T>::Action{std::move(action)}));
  return std::move(*this);
}

}
