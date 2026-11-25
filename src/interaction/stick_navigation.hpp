#pragma once

#include <arisa/composition/move_command_direction.hpp>
#include <arisa/provider/input/navigation_tuning.hpp>
#include <arisa/provider/input/stick.hpp>

#include <optional>

namespace arisa::interaction {

class StickNavigation {
 public:
  auto update(provider::Stick stick, const provider::NavigationTuning& tuning)
      -> std::optional<composition::MoveCommandDirection>;

  auto direction() const noexcept
      -> std::optional<composition::MoveCommandDirection> {
    return held_;
  }

 private:
  static auto dominant(provider::Stick stick) noexcept
      -> composition::MoveCommandDirection;

  static auto along(provider::Stick stick,
                    composition::MoveCommandDirection direction) noexcept
      -> float;

  std::optional<composition::MoveCommandDirection> held_;
};

}
