#pragma once

namespace arisa::motion {

class TransitionProperties {
 public:
  constexpr explicit TransitionProperties(bool has_motion = true) noexcept
      : has_motion_(has_motion) {}

  constexpr auto has_motion() const noexcept -> bool { return has_motion_; }

  constexpr void set_has_motion(bool has_motion) noexcept {
    has_motion_ = has_motion;
  }

  friend constexpr auto operator==(TransitionProperties, TransitionProperties)
      -> bool = default;

 private:
  bool has_motion_;
};

}
