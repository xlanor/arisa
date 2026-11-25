#pragma once

namespace arisa::composition {

class KeyframeRepeat {
 public:
  constexpr explicit KeyframeRepeat(bool repeating = true) noexcept
      : repeating_(repeating) {}

  constexpr auto repeating() const noexcept -> bool { return repeating_; }

  friend constexpr auto operator==(KeyframeRepeat, KeyframeRepeat)
      -> bool = default;

 private:
  bool repeating_{true};
};

}
