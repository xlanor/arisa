#include <arisa/composition/view_modifier.hpp>

#include <utility>

namespace arisa::composition {

auto EmptyModifier::body(Context&, Content content) const -> Element {
  return content;
}

}
