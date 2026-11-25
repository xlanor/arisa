#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/structure/disclosure_group.hpp>
#include <arisa/structure/disclosure_group_style.hpp>
#include <arisa/theme/colour_value.hpp>

#include <utility>

namespace arisa::structure {

DisclosureGroup::DisclosureGroup(std::string label, Builder content)
    : DisclosureGroup(components::Text(std::move(label)), std::nullopt,
                      std::move(content), true) {}

DisclosureGroup::DisclosureGroup(composition::Element label, Builder content)
    : DisclosureGroup(std::move(label), std::nullopt, std::move(content),
                      true) {}

DisclosureGroup::DisclosureGroup(std::string label,
                                 composition::Binding<bool> is_expanded,
                                 Builder content)
    : DisclosureGroup(
          components::Text(std::move(label)),
          std::optional<composition::Binding<bool>>{std::move(is_expanded)},
          std::move(content), true) {}

DisclosureGroup::DisclosureGroup(composition::Element label,
                                 composition::Binding<bool> is_expanded,
                                 Builder content)
    : DisclosureGroup(
          std::move(label),
          std::optional<composition::Binding<bool>>{std::move(is_expanded)},
          std::move(content), true) {}

DisclosureGroup::DisclosureGroup(
    composition::Element label,
    std::optional<composition::Binding<bool>> is_expanded, Builder content,
    bool)
    : Element(std::make_unique<Content>(
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(label)),
          std::move(is_expanded), std::move(content))) {}

auto DisclosureGroup::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto DisclosureGroup::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto local = context.state(expanded);
  const DisclosureGroupStyle& style =
      context.environment(DisclosureGroupStyle::key);
  std::vector<composition::Element> produced;
  produced.push_back(style.make_body(DisclosureGroupStyle::Configuration{
      .label =
          composition::Element{composition::PlaceholderContentView(label_)},
      .content = content_,
      .is_expanded = is_expanded_.has_value()
                         ? *is_expanded_
                         : composition::Binding<bool>{local}}));
  return produced;
}

}
