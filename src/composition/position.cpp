#include <arisa/composition/position.hpp>

namespace arisa::composition {

auto Position::type() const noexcept -> ElementType {
  return ElementType::of<Position>();
}

auto Position::paints_like(const ElementContent& other) const -> bool {
  return other.type() == type() &&
         static_cast<const Position&>(other).centre_ == centre_;
}

}
