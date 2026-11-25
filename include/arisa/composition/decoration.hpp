#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>

#include <cstddef>

namespace arisa::composition {

class Decoration final : public ElementContent {
 public:
  Decoration(Alignment alignment, std::size_t base, bool masks = false,
             bool inverts = false) noexcept
      : alignment_(alignment), base_(base), masks_(masks), inverts_(inverts) {}

  auto type() const noexcept -> ElementType override;

  auto paints_like(const ElementContent& other) const -> bool override;

  auto alignment() const noexcept -> Alignment { return alignment_; }

  auto base() const noexcept -> std::size_t { return base_; }

  auto masks() const noexcept -> bool { return masks_; }

  auto inverts() const noexcept -> bool { return inverts_; }

 private:
  Alignment alignment_;
  std::size_t base_;
  bool masks_;
  bool inverts_;
};

}
