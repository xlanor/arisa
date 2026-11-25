#include <arisa/composition/layer.hpp>
#include <arisa/composition/layer_host.hpp>

#include <memory>
#include <utility>

namespace arisa::composition {

auto Layer::default_animation() noexcept -> motion::Animation {
  return motion::Animation::smooth(0.35);
}

Layer::Layer(OverlayLayer layer, OverlayInput input, Alignment alignment,
             std::vector<Element> children, std::optional<Anchoring> anchoring,
             std::optional<PopoverPlacement> placement)
    : Element(std::make_unique<Content>(layer, input, alignment, anchoring,
                                        std::move(placement)),
              std::move(children)) {
  animate_transitions(default_animation());
}

auto Layer::Content::paints_like(const ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return layer_ == theirs.layer_ && alignment_ == theirs.alignment_ &&
         anchoring_ == theirs.anchoring_ &&
         placement_.transform(&PopoverPlacement::composed) ==
             theirs.placement_.transform(&PopoverPlacement::composed);
}

auto Layer::Content::type() const noexcept -> ElementType {
  return ElementType::of<Content>();
}

LayerHost::LayerHost(Element presenter, std::optional<Element> layer)
    : Element(std::make_unique<Content>()) {
  inherit(*this, presenter);
  std::vector<Element> children;
  children.push_back(std::move(presenter));
  if (layer.has_value()) {
    children.push_back(std::move(*layer));
  }
  replace_children(std::move(children));
}

auto LayerHost::Content::type() const noexcept -> ElementType {
  return ElementType::of<Content>();
}

}
