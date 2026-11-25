#pragma once

#include <arisa/composition/element_key.hpp>
#include <arisa/composition/ut_type.hpp>

#include <optional>

namespace arisa::composition {

struct ReorderItem {
  ElementKey id;
  std::optional<ElementKey> collection;

  static auto content_type() -> UTType { return UTType{"arisa.reorder-item"}; }

  friend auto operator==(const ReorderItem&, const ReorderItem&)
      -> bool = default;
};

}
