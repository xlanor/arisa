#pragma once

#include <arisa/composition/scroll_geometry.hpp>
#include <arisa/composition/size.hpp>

#include <optional>

namespace arisa::composition {

struct ScrollPhaseChangeContext {
  ScrollGeometry geometry{};
  std::optional<Size> velocity{};
};

}
