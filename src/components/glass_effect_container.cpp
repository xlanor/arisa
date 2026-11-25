#include <arisa/components/glass_effect_container.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

GlassEffectContainer::GlassEffectContainer(
    theme::Space spacing, std::vector<composition::Element> children)
    : Element(std::make_unique<Content>(spacing), std::move(children)) {}

auto GlassEffectContainer::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  return other.type() == type() &&
         static_cast<const Content&>(other).spacing_ == spacing_;
}

auto GlassEffectContainer::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

}
