#pragma once

#include <arisa/composition/alignment.hpp>

namespace arisa::composition {

class LayoutSlot {
 public:
  virtual ~LayoutSlot() = default;

  virtual auto layout_direction() const noexcept -> LayoutDirection {
    return LayoutDirection::left_to_right;
  }
};

}
