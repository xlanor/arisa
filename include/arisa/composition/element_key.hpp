#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace arisa::composition {

class ElementKey;

template <class T>
concept Identifiable =
    std::convertible_to<const T&, ElementKey> || requires(const T& value) {
      { std::hash<T>{}(value) } -> std::convertible_to<std::size_t>;
    };

class ElementKey {
 public:
  template <std::integral T>
    requires(!std::same_as<T, bool>)
  constexpr ElementKey(T value) noexcept
      : value_(std::in_place_index<0>, value) {}

  ElementKey(std::string text)
      : value_(std::in_place_index<1>, std::move(text)) {}

  ElementKey(std::string_view text)
      : value_(std::in_place_index<1>, std::string{text}) {}

  ElementKey(const char* text)
      : value_(std::in_place_index<1>, std::string{text}) {}

  template <Identifiable T>
  static auto identifying(const T& value) -> ElementKey {
    if constexpr (std::convertible_to<const T&, ElementKey>) {
      return ElementKey{value};
    } else {
      return ElementKey{static_cast<std::int64_t>(std::hash<T>{}(value))};
    }
  }

  auto description() const -> std::string;

  auto hash() const noexcept -> std::size_t;

  template <class T>
  auto as() const -> std::optional<T> {
    if constexpr (std::integral<T>) {
      if (const auto* number = std::get_if<0>(&value_)) {
        return static_cast<T>(*number);
      }
    } else if constexpr (std::constructible_from<T, std::string>) {
      if (const auto* text = std::get_if<1>(&value_)) {
        return T{*text};
      }
    }
    return std::nullopt;
  }

  friend auto operator==(const ElementKey&, const ElementKey&)
      -> bool = default;
  friend auto operator<=>(const ElementKey&, const ElementKey&) = default;

 private:
  std::variant<std::int64_t, std::string> value_;
};

}
