#include <arisa/text/point_size.hpp>

#include <cmath>

namespace arisa::text {

auto PointSize::create(float points) noexcept -> std::optional<PointSize> {
  if (!std::isfinite(points) || points <= 0.0F || points > maximum) {
    return std::nullopt;
  }
  return PointSize{points};
}

}
