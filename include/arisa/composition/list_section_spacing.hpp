#pragma once

#include <optional>

namespace arisa::composition {

class ListSectionSpacing {
 public:
  static constexpr float compact_length = 20.0F;

  static const ListSectionSpacing default_;
  static const ListSectionSpacing compact;

  static constexpr auto custom(float spacing) noexcept -> ListSectionSpacing {
    return ListSectionSpacing{spacing};
  }

  constexpr auto length() const noexcept -> std::optional<float> {
    return length_;
  }

  friend constexpr auto operator==(const ListSectionSpacing&,
                                   const ListSectionSpacing&) -> bool = default;

 private:
  constexpr explicit ListSectionSpacing(std::optional<float> length) noexcept
      : length_(length) {}

  std::optional<float> length_;
};

inline constexpr ListSectionSpacing ListSectionSpacing::default_{std::nullopt};
inline constexpr ListSectionSpacing ListSectionSpacing::compact{
    ListSectionSpacing::compact_length};

}
