#include <arisa/motion/unit_curve.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace arisa::motion {

auto UnitCurve::bezier(double c0x, double c0y, double c1x, double c1y) noexcept
    -> UnitCurve {
  return UnitCurve{std::clamp(c0x, 0.0, 1.0), c0y, std::clamp(c1x, 0.0, 1.0),
                   c1y};
}

auto UnitCurve::component(double first, double second, double time) noexcept
    -> double {
  const double proportional = 3.0 * first;
  const double quadratic = (3.0 * (second - first)) - proportional;
  const double cubic = 1.0 - proportional - quadratic;
  return ((cubic * time + quadratic) * time + proportional) * time;
}

auto UnitCurve::slope(double first, double second, double time) noexcept
    -> double {
  const double proportional = 3.0 * first;
  const double quadratic = (3.0 * (second - first)) - proportional;
  const double cubic = 1.0 - proportional - quadratic;
  return ((3.0 * cubic * time) + (2.0 * quadratic)) * time + proportional;
}

auto UnitCurve::solve(double progress) const noexcept -> double {
  constexpr int newton_steps = 8;
  constexpr int bisection_steps = 48;
  constexpr double tolerance = 1.0e-7;
  const double first = std::clamp(c0x_, 0.0, 1.0);
  const double second = std::clamp(c1x_, 0.0, 1.0);
  double time = progress;
  for (int step = 0; step < newton_steps; ++step) {
    const double error = component(first, second, time) - progress;
    if (std::abs(error) < tolerance) {
      return time;
    }
    const double derivative = slope(first, second, time);
    if (std::abs(derivative) < 1.0e-6) {
      break;
    }
    time -= error / derivative;
  }
  double low = 0.0;
  double high = 1.0;
  time = progress;
  for (int step = 0; step < bisection_steps; ++step) {
    const double value = component(first, second, time);
    if (std::abs(value - progress) < tolerance) {
      return time;
    }
    if (value < progress) {
      low = time;
    } else {
      high = time;
    }
    time = (low + high) * 0.5;
  }
  return time;
}

auto UnitCurve::bounce_out(double time) noexcept -> double {
  if (time < 1.0 / bounce_divisor) {
    return bounce_amplitude * time * time;
  }
  if (time < 2.0 / bounce_divisor) {
    const double shifted = time - (1.5 / bounce_divisor);
    return (bounce_amplitude * shifted * shifted) + 0.75;
  }
  if (time < 2.5 / bounce_divisor) {
    const double shifted = time - (2.25 / bounce_divisor);
    return (bounce_amplitude * shifted * shifted) + 0.9375;
  }
  const double shifted = time - (2.625 / bounce_divisor);
  return (bounce_amplitude * shifted * shifted) + 0.984375;
}

auto UnitCurve::entry(Family family, double time) noexcept -> double {
  constexpr double elastic_frequency = (2.0 * std::numbers::pi) / 3.0;
  switch (family) {
    case Family::circular:
      return 1.0 - std::sqrt(std::max(1.0 - (time * time), 0.0));
    case Family::back:
      return ((back_overshoot + 1.0) * time * time * time) -
             (back_overshoot * time * time);
    case Family::elastic:
      if (time <= 0.0 || time >= 1.0) {
        return time;
      }
      return -std::pow(2.0, (10.0 * time) - 10.0) *
             std::sin(((time * 10.0) - 10.75) * elastic_frequency);
    case Family::bounce:
      return 1.0 - bounce_out(1.0 - time);
    case Family::bezier:
      return time;
  }
  return time;
}

auto UnitCurve::preset(double progress) const noexcept -> double {
  switch (phase_) {
    case Phase::in:
      return entry(family_, progress);
    case Phase::out:
      return 1.0 - entry(family_, 1.0 - progress);
    case Phase::in_out:
      return progress < 0.5
                 ? entry(family_, 2.0 * progress) * 0.5
                 : 1.0 - (entry(family_, 2.0 - (2.0 * progress)) * 0.5);
  }
  return progress;
}

auto UnitCurve::value(double progress) const noexcept -> double {
  if (progress <= 0.0) {
    return 0.0;
  }
  if (progress >= 1.0) {
    return 1.0;
  }
  if (family_ != Family::bezier) {
    return preset(progress);
  }
  return component(c0y_, c1y_, solve(progress));
}

auto UnitCurve::velocity(double progress) const noexcept -> double {
  if (family_ != Family::bezier) {
    const double low = std::clamp(progress - derivative_step, 0.0, 1.0);
    const double high = std::clamp(progress + derivative_step, 0.0, 1.0);
    if (high <= low) {
      return 0.0;
    }
    return (preset(high) - preset(low)) / (high - low);
  }
  const double time = solve(std::clamp(progress, 0.0, 1.0));
  const double horizontal =
      slope(std::clamp(c0x_, 0.0, 1.0), std::clamp(c1x_, 0.0, 1.0), time);
  if (std::abs(horizontal) < 1.0e-9) {
    return 0.0;
  }
  return slope(c0y_, c1y_, time) / horizontal;
}

}
