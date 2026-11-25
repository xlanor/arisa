#pragma once

#include <arisa/composition/rect.hpp>

#include <cstddef>

namespace arisa::composition {

struct LinkFrame {
  std::size_t link{0};
  Rect frame{};

  friend auto operator==(const LinkFrame&, const LinkFrame&) -> bool = default;
};

}
