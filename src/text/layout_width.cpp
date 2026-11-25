#include <arisa/text/layout_width.hpp>

#include <cmath>

namespace arisa::text {

auto LayoutWidth::points(float value) noexcept -> std::optional<LayoutWidth> {
  if (std::isnan(value) || value < 0.0F) {
    return std::nullopt;
  }
  return LayoutWidth{value};
}

}
