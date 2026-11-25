#include <arisa/components/environment.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/theme/colour_value.hpp>

#include <utility>

namespace arisa::components {

GlassPanel::GlassPanel(PanelRadius radius, composition::Element content)
    : Element(std::make_unique<Content>(nullptr, radius)) {
  inherit_identity(*this, content);
  inherit(*this, content);
  edit<Content>().hold(
      std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(content)));
}

auto GlassPanel::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto GlassPanel::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const theme::Glass glass =
      context.environment(Environment::theme).glass().panel;
  composition::Element panel = ZStack(
      composition::Element{composition::PlaceholderContentView(content_)});
  if (glass.variant() == theme::Glass::Variant::identity) {
    panel = std::move(panel).background(theme::ColourRole::surface_raised,
                                        GlassPanel::shape(radius_));
  } else {
    panel = std::move(panel).glass_effect(glass, GlassPanel::shape(radius_));
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(panel));
  return produced;
}

}

namespace arisa::composition {

auto Element::glass_panel(components::PanelRadius radius) && -> Element {
  return components::GlassPanel(radius, std::move(*this));
}

}
