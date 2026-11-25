#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/list_row.hpp>

namespace arisa::composition {

class SwipeHost {
 public:
  SwipeHost() = default;
  SwipeHost(const SwipeHost&) = delete;
  auto operator=(const SwipeHost&) -> SwipeHost& = delete;
  SwipeHost(SwipeHost&&) = delete;
  auto operator=(SwipeHost&&) -> SwipeHost& = delete;
  virtual ~SwipeHost() = default;

  virtual auto wrap(Element row, const ListRow& attributes) const
      -> Element = 0;
};

}
