#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>

namespace arisa::components {

class Divider : public composition::Element {
 public:
  class Content;

  Divider();
};

class Divider::Content final : public composition::ElementContent {
 public:
  auto type() const noexcept -> composition::ElementType override;
};

}
