#pragma once

#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node_id.hpp>

namespace arisa::composition {

struct DuplicateKey {
  NodeId parent{0};
  ElementKey key{0};

  friend auto operator==(const DuplicateKey&, const DuplicateKey&)
      -> bool = default;
};

}
