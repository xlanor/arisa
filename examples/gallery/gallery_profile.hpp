#pragma once

#include <cstdint>

namespace arisa::examples {

struct GalleryProfile {
  enum class Plan : std::uint8_t {
    basic,
    plus,
    pro,
  };

  bool notifications{true};
  double volume{0.5};
  int devices{2};
  Plan plan{Plan::plus};

  friend auto operator==(const GalleryProfile&, const GalleryProfile&)
      -> bool = default;
};

}
