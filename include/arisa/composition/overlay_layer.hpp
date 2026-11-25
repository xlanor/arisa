#pragma once

#include <compare>
#include <cstdint>

namespace arisa::composition {

class OverlayLayer {
 public:
  constexpr explicit OverlayLayer(std::int32_t z) noexcept : z_(z) {}

  static const OverlayLayer base;
  static const OverlayLayer content;
  static const OverlayLayer chrome;
  static const OverlayLayer sheet;
  static const OverlayLayer dialog;
  static const OverlayLayer toast;
  static const OverlayLayer hud;
  static const OverlayLayer drag;
  static const OverlayLayer focus;
  static const OverlayLayer debug;
  static const OverlayLayer pointer;

  constexpr auto z() const noexcept -> std::int32_t { return z_; }

  friend constexpr auto operator==(OverlayLayer, OverlayLayer)
      -> bool = default;
  friend constexpr auto operator<=>(OverlayLayer, OverlayLayer) = default;

 private:
  std::int32_t z_;
};

inline constexpr OverlayLayer OverlayLayer::base{0};
inline constexpr OverlayLayer OverlayLayer::content{100};
inline constexpr OverlayLayer OverlayLayer::chrome{200};
inline constexpr OverlayLayer OverlayLayer::sheet{300};
inline constexpr OverlayLayer OverlayLayer::dialog{400};
inline constexpr OverlayLayer OverlayLayer::toast{500};
inline constexpr OverlayLayer OverlayLayer::hud{600};
inline constexpr OverlayLayer OverlayLayer::drag{650};
inline constexpr OverlayLayer OverlayLayer::focus{700};
inline constexpr OverlayLayer OverlayLayer::debug{800};
inline constexpr OverlayLayer OverlayLayer::pointer{900};

}
