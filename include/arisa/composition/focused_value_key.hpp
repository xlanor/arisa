#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/environment_override.hpp>
#include <arisa/composition/identity.hpp>

#include <concepts>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace arisa::composition {

template <class T>
class FocusedValueKey {
  template <class V>
  struct Slot {
    using type = V;

    static auto wrap(V value) -> type { return value; }

    static auto unwrap(const type& held) -> V { return held; }
  };

  template <class U>
  struct Slot<Binding<U>> {
    struct type {
      Binding<U> binding;

      friend auto operator==(const type& left, const type& right) -> bool {
        return left.binding.identity() == right.binding.identity();
      }
    };

    static auto wrap(Binding<U> value) -> type {
      return type{.binding = std::move(value)};
    }

    static auto unwrap(const type& held) -> Binding<U> { return held.binding; }
  };

  using Stored = typename Slot<T>::type;

 public:
  constexpr explicit FocusedValueKey(std::string_view name)
    requires std::equality_comparable<Stored>
      : key_(name, std::nullopt) {}

  FocusedValueKey(const FocusedValueKey&) = delete;
  auto operator=(const FocusedValueKey&) -> FocusedValueKey& = delete;
  FocusedValueKey(FocusedValueKey&&) = delete;
  auto operator=(FocusedValueKey&&) -> FocusedValueKey& = delete;
  ~FocusedValueKey() = default;

  auto identity() const noexcept -> Identity { return key_.identity(); }

  constexpr auto name() const noexcept -> std::string_view {
    return key_.name();
  }

  auto declare(T value) const -> std::shared_ptr<const EnvironmentOverride> {
    return std::make_shared<const EnvironmentOverride>(EnvironmentOverride::of(
        key_, std::optional<Stored>{Slot<T>::wrap(std::move(value))}));
  }

  static auto read(const EnvironmentOverride::Value& value)
      -> std::optional<T> {
    const std::optional<Stored>& held =
        static_cast<const EnvironmentOverride::Typed<std::optional<Stored>>&>(
            value)
            .get();
    if (!held.has_value()) {
      return std::nullopt;
    }
    return Slot<T>::unwrap(*held);
  }

 private:
  EnvironmentKey<std::optional<Stored>> key_;
};

}
