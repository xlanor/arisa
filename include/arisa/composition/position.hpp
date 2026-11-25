#pragma once

#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/point.hpp>

namespace arisa::composition {

class Position final : public ElementContent {
 public:
  explicit Position(Point centre) noexcept : centre_(centre) {}

  auto type() const noexcept -> ElementType override;

  auto paints_like(const ElementContent& other) const -> bool override;

  auto centre() const noexcept -> Point { return centre_; }

 private:
  Point centre_;
};

}
