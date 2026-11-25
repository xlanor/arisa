#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/command_mapping.hpp>
#include <arisa/components/control_metrics.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/composition/popover_placement.hpp>
#include <arisa/composition/scroll_view_proxy.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/menu_style.hpp>
#include <arisa/presentation/presentation_layer.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/shape.hpp>

#include <memory>
#include <optional>
#include <utility>

namespace arisa::controls {

Menu::Menu(std::string title, Builder content)
    : Menu(std::optional<std::string>{std::move(title)}, std::move(content),
           std::nullopt, Action{}) {}

Menu::Menu(Builder content, composition::Element label)
    : Menu(std::nullopt, std::move(content),
           std::optional<composition::Element>{std::move(label)}, Action{}) {}

Menu::Menu(std::string title, components::Image::SystemName system_image,
           Builder content)
    : Menu(std::move(content),
           components::Label(std::move(title),
                             components::Image{std::move(system_image)})) {}

Menu::Menu(std::string title, components::Image::Resource image,
           Builder content)
    : Menu(std::move(content),
           components::Label(std::move(title),
                             components::Image{std::move(image)})) {}

Menu::Menu(std::string title, Builder content, Action primary_action)
    : Menu(std::optional<std::string>{std::move(title)}, std::move(content),
           std::nullopt, std::move(primary_action)) {}

Menu::Menu(Builder content, composition::Element label, Action primary_action)
    : Menu(std::nullopt, std::move(content),
           std::optional<composition::Element>{std::move(label)},
           std::move(primary_action)) {}

Menu::Menu(std::optional<std::string> title, Builder content,
           std::optional<composition::Element> label, Action primary_action)
    : Element(std::make_unique<Content>(
          std::move(title), std::move(content),
          label.has_value()
              ? std::make_shared<composition::PlaceholderContentView::Source>(
                    std::move(*label))
              : nullptr,
          std::move(primary_action))) {}

auto Menu::items(composition::Element declared,
                 composition::Binding<bool> shown) -> composition::Element {
  return presentation::MenuPanel::items(std::move(declared), std::move(shown));
}

auto Menu::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Menu::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto open = context.state(shown);
  const MenuStyle& style = context.environment(MenuStyle::key);
  const composition::Visibility indicator =
      context.environment(composition::ControlEnvironment::menu_indicator);
  const bool upward =
      context.environment(composition::ControlEnvironment::menu_opens_upward);
  const theme::Edge declared = upward ? theme::Edge::bottom : theme::Edge::top;
  const composition::StateHandle<std::optional<theme::Edge>> landed =
      context.state(placed);
  const theme::Edge composed = landed.get().value_or(declared);
  const bool reversed =
      composed == theme::Edge::bottom &&
      context.environment(composition::ControlEnvironment::menu_order) ==
          composition::MenuOrder::priority;
  const bool dismisses =
      context.environment(
          composition::ControlEnvironment::menu_action_dismiss_behavior) !=
      composition::MenuActionDismissBehavior::disabled;
  const bool chevron = label_ != nullptr && !style.pulls_down()
                           ? indicator == composition::Visibility::visible
                           : indicator != composition::Visibility::hidden;
  std::vector<composition::Element> parts;
  parts.push_back(
      label_ != nullptr
          ? composition::Element{composition::PlaceholderContentView(label_)}
          : composition::Element{
                components::Text(title_.value_or(""))
                    .font(theme::Font{components::Button::label_role})});
  if (chevron) {
    parts.push_back(components::Glyph::chevron_down(
        theme::ColourRole::text_secondary,
        chevron_extent *
            components::ControlMetrics::of(
                context.environment(components::Environment::control_size))
                .font_scale));
  }
  composition::Element label =
      parts.size() == 1
          ? std::move(parts.front())
          : composition::Element{components::HStack(
                components::HStackOptions{
                    .alignment = components::VerticalAlignment::center,
                    .spacing = theme::space::xs},
                std::move(parts))};
  const composition::ScrollViewProxy proxy = context.scroll_view_proxy();
  const auto presents = [open, proxy] {
    proxy.scroll_to(composition::ElementKey{anchor_key});
    open.set(true);
  };
  const components::ButtonStyle& inherited =
      context.environment(components::ButtonStyle::key);
  composition::Element body = style.make_body(MenuStyle::Configuration{
      .label = std::move(label),
      .open = primary_action_ ? Action{primary_action_} : Action{presents},
      .button_style = inherited.is_automatic()
                          ? std::nullopt
                          : std::optional<components::ButtonStyle>{inherited}});
  if (primary_action_) {
    body = std::move(body)
               .on_command(components::CommandMapping::options,
                           std::string{menu_label}, presents)
               .on_command(components::CommandMapping::options_hold,
                           std::string{menu_label}, presents,
                           components::CommandMapping::hold)
               .on_long_press_gesture(presents);
  }
  std::vector<composition::Element> produced;
  produced.push_back(
      std::move(body)
          .id(anchor_key)
          .popover(open, composition::PopoverAttachmentAnchor::rect(), declared,
                   [content = content_, open, reversed, dismisses, composed,
                    landed] {
                     return presentation::PresentationLayer::placed(
                         presentation::MenuPanel::items(content(), open,
                                                        reversed, dismisses),
                         composition::PopoverPlacement{
                             composed,
                             [landed](theme::Edge edge) { landed.set(edge); }});
                   }));
  return produced;
}

}
