#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/theme/colour_value.hpp>

#include <optional>

namespace arisa::composition {

struct ToastOptions {
  Alignment alignment{Alignment::bottom};
  std::optional<theme::ColourValue> tint{};
};

}
