#pragma once

#include <arisa/composition/component_content.hpp>
#include <arisa/composition/element.hpp>

namespace arisa::composition {

class Context;

class Component {
 public:
  virtual ~Component() = default;

  virtual auto body(Context& context) const -> Element = 0;

  auto operator==(const Component&) const -> bool = default;
};

}
