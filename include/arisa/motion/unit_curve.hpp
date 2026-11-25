#pragma once

#include <cstdint>

namespace arisa::motion {

class UnitCurve {
 public:
  static const UnitCurve linear;
  static const UnitCurve ease_in;
  static const UnitCurve ease_out;
  static const UnitCurve ease_in_out;
  static const UnitCurve circular_ease_in;
  static const UnitCurve circular_ease_out;
  static const UnitCurve circular_ease_in_out;
  static const UnitCurve back_ease_in;
  static const UnitCurve back_ease_out;
  static const UnitCurve back_ease_in_out;
  static const UnitCurve elastic_ease_in;
  static const UnitCurve elastic_ease_out;
  static const UnitCurve elastic_ease_in_out;
  static const UnitCurve bounce_ease_in;
  static const UnitCurve bounce_ease_out;
  static const UnitCurve bounce_ease_in_out;

  static constexpr double back_overshoot = 1.70158;
  static constexpr double bounce_amplitude = 7.5625;
  static constexpr double bounce_divisor = 2.75;
  static constexpr double derivative_step = 1.0e-4;

  static auto bezier(double c0x, double c0y, double c1x, double c1y) noexcept
      -> UnitCurve;

  auto value(double progress) const noexcept -> double;

  auto velocity(double progress) const noexcept -> double;

  constexpr auto inverse() const noexcept -> UnitCurve {
    if (family_ == Family::bezier) {
      return UnitCurve{c0y_, c0x_, c1y_, c1x_};
    }
    switch (phase_) {
      case Phase::in:
        return UnitCurve{family_, Phase::out};
      case Phase::out:
        return UnitCurve{family_, Phase::in};
      case Phase::in_out:
        return *this;
    }
    return *this;
  }

  friend constexpr auto operator==(const UnitCurve&, const UnitCurve&)
      -> bool = default;

 private:
  enum class Family : std::uint8_t {
    bezier,
    circular,
    back,
    elastic,
    bounce,
  };

  enum class Phase : std::uint8_t {
    in,
    out,
    in_out,
  };

  constexpr UnitCurve(double c0x, double c0y, double c1x, double c1y) noexcept
      : c0x_(c0x), c0y_(c0y), c1x_(c1x), c1y_(c1y) {}

  constexpr UnitCurve(Family family, Phase phase) noexcept
      : family_(family), phase_(phase) {}

  static auto component(double first, double second, double time) noexcept
      -> double;

  static auto slope(double first, double second, double time) noexcept
      -> double;

  static auto entry(Family family, double time) noexcept -> double;

  static auto bounce_out(double time) noexcept -> double;

  auto solve(double progress) const noexcept -> double;

  auto preset(double progress) const noexcept -> double;

  double c0x_{0.0};
  double c0y_{0.0};
  double c1x_{1.0};
  double c1y_{1.0};
  Family family_{Family::bezier};
  Phase phase_{Phase::in};
};

inline constexpr UnitCurve UnitCurve::linear{0.0, 0.0, 1.0, 1.0};
inline constexpr UnitCurve UnitCurve::ease_in{0.42, 0.0, 1.0, 1.0};
inline constexpr UnitCurve UnitCurve::ease_out{0.0, 0.0, 0.58, 1.0};
inline constexpr UnitCurve UnitCurve::ease_in_out{0.42, 0.0, 0.58, 1.0};
inline constexpr UnitCurve UnitCurve::circular_ease_in{Family::circular,
                                                       Phase::in};
inline constexpr UnitCurve UnitCurve::circular_ease_out{Family::circular,
                                                        Phase::out};
inline constexpr UnitCurve UnitCurve::circular_ease_in_out{Family::circular,
                                                           Phase::in_out};
inline constexpr UnitCurve UnitCurve::back_ease_in{Family::back, Phase::in};
inline constexpr UnitCurve UnitCurve::back_ease_out{Family::back, Phase::out};
inline constexpr UnitCurve UnitCurve::back_ease_in_out{Family::back,
                                                       Phase::in_out};
inline constexpr UnitCurve UnitCurve::elastic_ease_in{Family::elastic,
                                                      Phase::in};
inline constexpr UnitCurve UnitCurve::elastic_ease_out{Family::elastic,
                                                       Phase::out};
inline constexpr UnitCurve UnitCurve::elastic_ease_in_out{Family::elastic,
                                                          Phase::in_out};
inline constexpr UnitCurve UnitCurve::bounce_ease_in{Family::bounce, Phase::in};
inline constexpr UnitCurve UnitCurve::bounce_ease_out{Family::bounce,
                                                      Phase::out};
inline constexpr UnitCurve UnitCurve::bounce_ease_in_out{Family::bounce,
                                                         Phase::in_out};

}
