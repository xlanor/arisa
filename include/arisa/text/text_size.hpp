#pragma once

namespace arisa::text {

struct TextSize {
  float width{0.0F};
  float height{0.0F};

  friend constexpr auto operator==(const TextSize&, const TextSize&)
      -> bool = default;
};

}
