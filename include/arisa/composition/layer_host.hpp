#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>

#include <optional>

namespace arisa::composition {

class LayerHost : public Element {
 public:
  class Content;

  LayerHost(Element presenter, std::optional<Element> layer);
};

class LayerHost::Content final : public ElementContent {
 public:
  auto type() const noexcept -> ElementType override;
};

}
