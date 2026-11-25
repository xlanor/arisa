#pragma once

#include <arisa/composition/axis.hpp>

#include <optional>

namespace arisa::components {

struct LayoutProperties {
  std::optional<composition::Axis> stack_orientation{};

  friend constexpr auto operator==(const LayoutProperties&,
                                   const LayoutProperties&) -> bool = default;
};

}
