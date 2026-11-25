#include <interaction/stick_navigation.hpp>

#include <cmath>

namespace arisa::interaction {

auto StickNavigation::dominant(provider::Stick stick) noexcept
    -> composition::MoveCommandDirection {
  if (std::abs(stick.x) >= std::abs(stick.y)) {
    return stick.x >= 0.0F ? composition::MoveCommandDirection::right
                           : composition::MoveCommandDirection::left;
  }
  return stick.y >= 0.0F ? composition::MoveCommandDirection::up
                         : composition::MoveCommandDirection::down;
}

auto StickNavigation::along(
    provider::Stick stick, composition::MoveCommandDirection direction) noexcept
    -> float {
  switch (direction) {
    case composition::MoveCommandDirection::right:
      return stick.x;
    case composition::MoveCommandDirection::left:
      return -stick.x;
    case composition::MoveCommandDirection::up:
      return stick.y;
    case composition::MoveCommandDirection::down:
      return -stick.y;
  }
  return 0.0F;
}

auto StickNavigation::update(provider::Stick stick,
                             const provider::NavigationTuning& tuning)
    -> std::optional<composition::MoveCommandDirection> {
  if (!std::isfinite(stick.x) || !std::isfinite(stick.y)) {
    held_.reset();
    return held_;
  }
  const float magnitude = std::hypot(stick.x, stick.y);
  if (!held_.has_value()) {
    if (magnitude >= tuning.stick_engage) {
      held_ = dominant(stick);
    }
    return held_;
  }
  if (magnitude < tuning.stick_release) {
    held_.reset();
    return held_;
  }
  const composition::MoveCommandDirection candidate = dominant(stick);
  if (candidate == *held_) {
    return held_;
  }
  const float current = along(stick, *held_);
  const float challenger = along(stick, candidate);
  if (current <= 0.0F || challenger > current * tuning.direction_hysteresis) {
    held_ = magnitude >= tuning.stick_engage
                ? std::optional<composition::MoveCommandDirection>{candidate}
                : std::nullopt;
  }
  return held_;
}

}
