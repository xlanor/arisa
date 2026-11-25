#include <arisa/composition/decoration.hpp>

namespace arisa::composition {

auto Decoration::paints_like(const ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Decoration&>(other);
  return alignment_ == theirs.alignment_ && base_ == theirs.base_ &&
         masks_ == theirs.masks_ && inverts_ == theirs.inverts_;
}

auto Decoration::type() const noexcept -> ElementType {
  return ElementType::of<Decoration>();
}

}
