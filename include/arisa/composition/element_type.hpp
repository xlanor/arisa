#pragma once

#include <arisa/composition/identity.hpp>

#include <compare>
#include <cstddef>
#include <source_location>
#include <string_view>

namespace arisa::composition {

class ElementType {
 public:
  template <class T>
  static constexpr auto of() noexcept -> ElementType {
    return known<T>;
  }

  constexpr auto name() const noexcept -> std::string_view { return name_; }

  friend constexpr auto operator==(ElementType left, ElementType right) noexcept
      -> bool {
    return left.identity_ == right.identity_;
  }

  friend constexpr auto operator<=>(ElementType left,
                                    ElementType right) noexcept
      -> std::strong_ordering {
    return left.identity_ <=> right.identity_;
  }

 private:
  template <class T>
  static constexpr char tag{};

  template <class T>
  static const ElementType known;

  template <class T>
  static constexpr auto spelled() noexcept -> std::string_view {
    constexpr std::string_view marker = "T = ";
    const std::string_view whole =
        std::source_location::current().function_name();
    const std::size_t begin = whole.find(marker);
    if (begin == std::string_view::npos) {
      return whole;
    }
    const std::size_t from = begin + marker.size();
    const std::size_t end = whole.find_first_of(";]", from);
    return whole.substr(from, end == std::string_view::npos
                                  ? std::string_view::npos
                                  : end - from);
  }

  constexpr ElementType(Identity identity, std::string_view name) noexcept
      : identity_(identity), name_(name) {}

  Identity identity_;
  std::string_view name_;
};

template <class T>
inline constexpr ElementType ElementType::known{Identity{&tag<T>},
                                                spelled<T>()};

}
