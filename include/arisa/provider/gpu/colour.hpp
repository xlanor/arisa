#pragma once

namespace arisa::provider {

struct Colour {
  float red{0.0F};
  float green{0.0F};
  float blue{0.0F};
  float alpha{1.0F};

  friend constexpr auto operator==(const Colour&, const Colour&)
      -> bool = default;
};

}
