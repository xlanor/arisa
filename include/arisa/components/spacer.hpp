#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>

namespace arisa::components {

class Spacer : public composition::Element {
 public:
  class Content;

  Spacer();

  explicit Spacer(float minimum_length);
};

class Spacer::Content final : public composition::ElementContent {
 public:
  explicit Content(float minimum_length) noexcept
      : minimum_length_(minimum_length) {}

  auto type() const noexcept -> composition::ElementType override;

  auto draws() const noexcept -> bool override { return false; }

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto minimum_length() const noexcept -> float { return minimum_length_; }

 private:
  float minimum_length_;
};

}
