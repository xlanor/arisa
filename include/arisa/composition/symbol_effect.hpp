#pragma once

#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>

#include <cstdint>
#include <optional>

namespace arisa::composition {

class Element;

class SymbolEffect {
 public:
  enum class Kind : std::uint8_t {
    bounce,
    pulse,
    variable_color,
    scale,
    appear,
    disappear,
    replace,
    wiggle,
    rotate,
    breathe,
  };

  enum class Direction : std::uint8_t { up, down };

  enum class Playback : std::uint8_t { triggered, endless, presented };

  static constexpr double default_wiggle_radians = 0.21;

  struct Pose {
    double scale{1.0};
    double offset_y{0.0};
    double rotation{0.0};
    double opacity{1.0};
    std::optional<double> layer_phase{};

    friend auto operator==(const Pose&, const Pose&) -> bool = default;
  };

  static auto bounce(Direction direction = Direction::up) noexcept
      -> SymbolEffect;
  static auto pulse() noexcept -> SymbolEffect;
  static auto variable_color() noexcept -> SymbolEffect;
  static auto scale(Direction direction = Direction::up) noexcept
      -> SymbolEffect;
  static auto appear() noexcept -> SymbolEffect;
  static auto disappear() noexcept -> SymbolEffect;
  static auto replace() noexcept -> SymbolEffect;
  static auto wiggle() noexcept -> SymbolEffect;
  static auto rotate() noexcept -> SymbolEffect;
  static auto breathe() noexcept -> SymbolEffect;

  auto speed(double factor) const noexcept -> SymbolEffect;

  auto repeat(int count) const noexcept -> SymbolEffect;

  auto custom(motion::Angle angle) const noexcept -> SymbolEffect;

  auto wiggle_angle() const noexcept -> motion::Angle {
    return motion::Angle::radians(wiggle_radians_);
  }

  auto kind() const noexcept -> Kind { return kind_; }

  auto direction() const noexcept -> Direction { return direction_; }

  auto speed_factor() const noexcept -> double { return speed_; }

  auto repeat_count() const noexcept -> int { return repeats_; }

  auto cycle() const noexcept -> double;

  auto duration() const noexcept -> double;

  auto pose(double elapsed) const noexcept -> Pose;

  auto runs_on_presentation() const noexcept -> bool;

  auto active_pose() const noexcept -> Pose;

  auto active_animation() const noexcept -> std::optional<motion::Animation>;

  static auto posed(Element content, const Pose& pose) -> Element;

  friend auto operator==(const SymbolEffect&, const SymbolEffect&)
      -> bool = default;

 private:
  constexpr SymbolEffect(Kind kind, Direction direction) noexcept
      : kind_(kind), direction_(direction) {}

  auto lap(double local) const noexcept -> Pose;

  Kind kind_;
  Direction direction_;
  double speed_{1.0};
  int repeats_{1};
  double wiggle_radians_{default_wiggle_radians};
};

}
