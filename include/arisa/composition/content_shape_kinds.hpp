#pragma once

#include <cstdint>

namespace arisa::composition {

class ContentShapeKinds {
 public:
  static const ContentShapeKinds interaction;
  static const ContentShapeKinds focus_effect;
  static const ContentShapeKinds hover_effect;
  static const ContentShapeKinds drag_preview;
  static const ContentShapeKinds accessibility;
  static const ContentShapeKinds context_menu_preview;

  constexpr auto contains(ContentShapeKinds other) const noexcept -> bool {
    return (bits_ & other.bits_) == other.bits_;
  }

  friend constexpr auto operator|(ContentShapeKinds left,
                                  ContentShapeKinds right) noexcept
      -> ContentShapeKinds {
    return ContentShapeKinds{
        static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(ContentShapeKinds, ContentShapeKinds)
      -> bool = default;

 private:
  constexpr explicit ContentShapeKinds(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_;
};

inline constexpr ContentShapeKinds ContentShapeKinds::interaction{1U};
inline constexpr ContentShapeKinds ContentShapeKinds::focus_effect{2U};
inline constexpr ContentShapeKinds ContentShapeKinds::hover_effect{4U};
inline constexpr ContentShapeKinds ContentShapeKinds::drag_preview{8U};
inline constexpr ContentShapeKinds ContentShapeKinds::accessibility{16U};
inline constexpr ContentShapeKinds ContentShapeKinds::context_menu_preview{32U};

}
