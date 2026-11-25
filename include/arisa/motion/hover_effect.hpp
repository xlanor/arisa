#pragma once

#include <arisa/motion/angle.hpp>

#include <cstdint>

namespace arisa::motion {

class HoverEffect {
 public:
  enum class Kind : std::uint8_t {
    automatic,
    highlight,
    lift,
  };

  static const HoverEffect automatic;
  static const HoverEffect highlight;
  static const HoverEffect lift;

  static constexpr float lift_scale = 0.10F;
  static constexpr float lift_shadow_radius = 8.0F;
  static constexpr float lift_shadow_drop = 16.0F;
  static constexpr float lift_shadow_opacity = 0.38F;
  static constexpr float glow_softness = 9.0F;
  static constexpr float glow_opacity = 0.45F;
  static constexpr float highlight_tint = 0.35F;
  static constexpr double default_tilt_degrees = 8.0;
  static constexpr float default_tilt_shift = 14.0F;

  constexpr auto tilt(Angle maximum = Angle::degrees(default_tilt_degrees),
                      float shift = default_tilt_shift) const noexcept
      -> HoverEffect {
    HoverEffect tilted = *this;
    tilted.tilts_ = true;
    tilted.tilt_angle_ = maximum;
    tilted.tilt_shift_ = shift;
    return tilted;
  }

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto lifts() const noexcept -> bool {
    return kind_ == Kind::lift || kind_ == Kind::automatic;
  }

  constexpr auto tilts() const noexcept -> bool { return tilts_; }

  constexpr auto tilt_angle() const noexcept -> Angle { return tilt_angle_; }

  constexpr auto tilt_shift() const noexcept -> float { return tilt_shift_; }

  friend constexpr auto operator==(const HoverEffect&, const HoverEffect&)
      -> bool = default;

 private:
  constexpr explicit HoverEffect(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  bool tilts_{false};
  Angle tilt_angle_{};
  float tilt_shift_{0.0F};
};

inline constexpr HoverEffect HoverEffect::automatic{
    HoverEffect::Kind::automatic};
inline constexpr HoverEffect HoverEffect::highlight{
    HoverEffect::Kind::highlight};
inline constexpr HoverEffect HoverEffect::lift{HoverEffect::Kind::lift};

}
