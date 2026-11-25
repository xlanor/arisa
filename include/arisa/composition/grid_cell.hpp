#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/motion/unit_point.hpp>

#include <cstdint>
#include <optional>

namespace arisa::composition {

struct GridCell {
  std::uint32_t columns{1};
  std::optional<HorizontalAlignment> column_alignment{};
  std::optional<motion::UnitPoint> anchor{};
  Axes unsized_axes{};

  friend constexpr auto operator==(const GridCell&, const GridCell&)
      -> bool = default;
};

}
