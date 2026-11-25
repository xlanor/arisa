#pragma once

#include <arisa/provider/input/navigation_tuning.hpp>
#include <arisa/provider/input/stick.hpp>

#include <optional>

namespace arisa::provider {

class InputTuning {
 public:
  static constexpr float default_deadzone = 0.15F;

  constexpr InputTuning() noexcept = default;

  static auto create(float deadzone) noexcept -> std::optional<InputTuning>;

  static auto create(float deadzone, NavigationTuning navigation) noexcept
      -> std::optional<InputTuning>;

  constexpr auto deadzone() const noexcept -> float { return deadzone_; }

  constexpr auto navigation() const noexcept -> const NavigationTuning& {
    return navigation_;
  }

  auto apply(Stick raw) const noexcept -> Stick;

  friend constexpr auto operator==(const InputTuning&, const InputTuning&)
      -> bool = default;

 private:
  constexpr InputTuning(float deadzone, NavigationTuning navigation) noexcept
      : deadzone_(deadzone), navigation_(navigation) {}

  float deadzone_{default_deadzone};
  NavigationTuning navigation_{};
};

}
