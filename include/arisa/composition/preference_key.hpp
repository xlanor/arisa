#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/identity.hpp>

#include <concepts>
#include <string_view>
#include <utility>

namespace arisa::composition {

template <class T>
class PreferenceKey {
 public:
  using Reduce = void (*)(T& value, T next);
  using Absorb = bool (*)(const T& previous, const T& incoming);

  PreferenceKey(std::string_view name, T default_value, Reduce reducer,
                Absorb absorber = nullptr)
      : name_(name),
        default_value_(std::move(default_value)),
        reduce_(reducer),
        absorb_(absorber) {}

  PreferenceKey(std::string_view name, T default_value, Reduce reducer,
                const EnvironmentKey<bool>& demand)
      : name_(name),
        default_value_(std::move(default_value)),
        reduce_(reducer),
        absorb_(nullptr),
        demand_(&demand) {}

  PreferenceKey(const PreferenceKey&) = delete;
  auto operator=(const PreferenceKey&) -> PreferenceKey& = delete;
  PreferenceKey(PreferenceKey&&) = delete;
  auto operator=(PreferenceKey&&) -> PreferenceKey& = delete;
  ~PreferenceKey() = default;

  auto name() const noexcept -> std::string_view { return name_; }

  auto default_value() const noexcept -> const T& { return default_value_; }

  void reduce(T& value, T next) const { reduce_(value, std::move(next)); }

  auto absorbs(const T& previous, const T& incoming) const -> bool {
    if (absorb_ != nullptr) {
      return absorb_(previous, incoming);
    }
    if constexpr (std::equality_comparable<T>) {
      return previous == incoming;
    } else {
      return false;
    }
  }

  auto identity() const noexcept -> Identity { return Identity{this}; }

  auto demand() const noexcept -> const EnvironmentKey<bool>* {
    return demand_;
  }

 private:
  std::string_view name_;
  T default_value_;
  Reduce reduce_;
  Absorb absorb_;
  const EnvironmentKey<bool>* demand_{nullptr};
};

}
