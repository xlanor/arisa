#include <arisa/composition/element.hpp>
#include <arisa/composition/image_environment.hpp>
#include <arisa/composition/symbol_effect.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/unit_curve.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace arisa::composition {

auto SymbolEffect::bounce(Direction direction) noexcept -> SymbolEffect {
  return SymbolEffect{Kind::bounce, direction};
}

auto SymbolEffect::pulse() noexcept -> SymbolEffect {
  return SymbolEffect{Kind::pulse, Direction::up};
}

auto SymbolEffect::variable_color() noexcept -> SymbolEffect {
  return SymbolEffect{Kind::variable_color, Direction::up};
}

auto SymbolEffect::scale(Direction direction) noexcept -> SymbolEffect {
  return SymbolEffect{Kind::scale, direction};
}

auto SymbolEffect::appear() noexcept -> SymbolEffect {
  return SymbolEffect{Kind::appear, Direction::up};
}

auto SymbolEffect::disappear() noexcept -> SymbolEffect {
  return SymbolEffect{Kind::disappear, Direction::down};
}

auto SymbolEffect::replace() noexcept -> SymbolEffect {
  return SymbolEffect{Kind::replace, Direction::up};
}

auto SymbolEffect::wiggle() noexcept -> SymbolEffect {
  return SymbolEffect{Kind::wiggle, Direction::up};
}

auto SymbolEffect::rotate() noexcept -> SymbolEffect {
  return SymbolEffect{Kind::rotate, Direction::up};
}

auto SymbolEffect::breathe() noexcept -> SymbolEffect {
  return SymbolEffect{Kind::breathe, Direction::up};
}

auto SymbolEffect::speed(double factor) const noexcept -> SymbolEffect {
  SymbolEffect faster = *this;
  faster.speed_ = std::max(speed_ * factor, motion::Animation::minimum_speed);
  return faster;
}

auto SymbolEffect::repeat(int count) const noexcept -> SymbolEffect {
  SymbolEffect repeated = *this;
  repeated.repeats_ = std::max(count, 1);
  return repeated;
}

auto SymbolEffect::custom(motion::Angle angle) const noexcept -> SymbolEffect {
  SymbolEffect tilted = *this;
  tilted.wiggle_radians_ = angle.radians();
  return tilted;
}

auto SymbolEffect::cycle() const noexcept -> double {
  switch (kind_) {
    case Kind::bounce:
      return 0.75;
    case Kind::pulse:
      return 1.0;
    case Kind::variable_color:
      return 1.2;
    case Kind::scale:
      return 0.5;
    case Kind::appear:
    case Kind::disappear:
      return 0.4;
    case Kind::replace:
      return 0.35;
    case Kind::wiggle:
      return 0.6;
    case Kind::rotate:
      return 1.0;
    case Kind::breathe:
      return 1.4;
  }
  return 1.0;
}

auto SymbolEffect::duration() const noexcept -> double {
  return cycle() * static_cast<double>(repeats_) / speed_;
}

auto SymbolEffect::lap(double local) const noexcept -> Pose {
  constexpr double pi = std::numbers::pi;
  const double length = cycle();
  const double unit = std::clamp(local / length, 0.0, 1.0);
  const double sign = direction_ == Direction::up ? 1.0 : -1.0;
  switch (kind_) {
    case Kind::bounce: {
      const double lift =
          std::exp(-5.0 * local) * std::sin(2.0 * pi * local / 0.5);
      return Pose{.scale = 1.0 + (0.15 * lift),
                  .offset_y = -6.0 * sign * lift,
                  .rotation = 0.0,
                  .opacity = 1.0};
    }
    case Kind::pulse: {
      const double dip = std::sin(pi * unit);
      return Pose{.opacity = 1.0 - (0.65 * dip * dip)};
    }
    case Kind::variable_color:
      return Pose{.layer_phase = unit};
    case Kind::scale:
      return Pose{.scale = 1.0 + (0.2 * sign * std::sin(pi * unit))};
    case Kind::appear:
    case Kind::replace: {
      const double grown = motion::UnitCurve::ease_out.value(unit);
      return Pose{.scale = 0.5 + (0.5 * grown), .opacity = grown};
    }
    case Kind::disappear: {
      const double shrunk = 1.0 - motion::UnitCurve::ease_in_out.value(
                                      std::abs((2.0 * unit) - 1.0));
      return Pose{.scale = 1.0 - (0.5 * shrunk), .opacity = 1.0 - shrunk};
    }
    case Kind::wiggle:
      return Pose{.rotation = wiggle_radians_ *
                              std::sin(2.0 * pi * 3.0 * unit) * (1.0 - unit)};
    case Kind::rotate:
      return Pose{.rotation =
                      unit >= 1.0
                          ? 0.0
                          : 2.0 * pi *
                                motion::UnitCurve::ease_in_out.value(unit)};
    case Kind::breathe: {
      const double swell = std::sin(pi * unit);
      return Pose{.scale = 1.0 + (0.12 * swell * swell),
                  .opacity = 1.0 - (0.3 * swell * swell)};
    }
  }
  return Pose{};
}

auto SymbolEffect::pose(double elapsed) const noexcept -> Pose {
  const double scaled = std::max(elapsed, 0.0) * speed_;
  const double length = cycle();
  if (scaled >= length * static_cast<double>(repeats_)) {
    return Pose{};
  }
  return lap(std::fmod(scaled, length));
}

auto SymbolEffect::runs_on_presentation() const noexcept -> bool {
  return kind_ != Kind::bounce && kind_ != Kind::wiggle &&
         kind_ != Kind::variable_color;
}

auto SymbolEffect::active_pose() const noexcept -> Pose {
  const double sign = direction_ == Direction::up ? 1.0 : -1.0;
  switch (kind_) {
    case Kind::pulse:
      return Pose{.opacity = 0.35};
    case Kind::variable_color:
      return Pose{.layer_phase = 0.0};
    case Kind::scale:
      return Pose{.scale = 1.0 + (0.2 * sign)};
    case Kind::disappear:
      return Pose{.scale = 0.5, .opacity = 0.0};
    case Kind::rotate:
      return Pose{.rotation = 2.0 * std::numbers::pi};
    case Kind::breathe:
      return Pose{.scale = 1.12, .opacity = 0.7};
    case Kind::bounce:
    case Kind::appear:
    case Kind::replace:
    case Kind::wiggle:
      return Pose{};
  }
  return Pose{};
}

auto SymbolEffect::active_animation() const noexcept
    -> std::optional<motion::Animation> {
  switch (kind_) {
    case Kind::pulse:
      return motion::Animation::ease_in_out(0.5 * cycle() / speed_)
          .repeat_forever();
    case Kind::variable_color:
      return motion::Animation::ease_in_out(0.5 * cycle() / speed_)
          .repeat_forever();
    case Kind::breathe:
      return motion::Animation::ease_in_out(0.5 * cycle() / speed_)
          .repeat_forever();
    case Kind::rotate:
      return motion::Animation::linear(cycle() / speed_).repeat_forever(false);
    case Kind::scale:
    case Kind::disappear:
    case Kind::appear:
    case Kind::replace:
      return motion::Animation::smooth(0.35 / speed_);
    case Kind::bounce:
    case Kind::wiggle:
      return std::nullopt;
  }
  return std::nullopt;
}

auto SymbolEffect::posed(Element content, const Pose& pose) -> Element {
  if (pose.layer_phase.has_value()) {
    content = std::move(content).environment(
        ImageEnvironment::symbol_layer_phase, pose.layer_phase);
  }
  return std::move(content)
      .scale_effect(static_cast<float>(pose.scale))
      .rotation_effect(motion::Angle::radians(pose.rotation))
      .offset(0.0F, static_cast<float>(pose.offset_y))
      .opacity(static_cast<float>(pose.opacity));
}

}
