#include <arisa/components/button.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/presentation/menu_panel.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/scaled_metric.hpp>
#include <arisa/theme/shape.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace arisa::presentation {

auto MenuPanel::ItemStyle::make_body(
    components::ButtonStyle::Configuration configuration) const
    -> composition::Element {
  const bool lit = configuration.is_focused || configuration.is_pressed;
  const bool destructive =
      configuration.role == components::ButtonRole::destructive;
  theme::ColourValue ink = theme::ColourRole::label;
  if (lit) {
    ink = theme::ColourRole::selected_menu_item_text;
  } else if (destructive) {
    ink = theme::ColourRole::destructive;
  }
  composition::Element row =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading},
          std::move(configuration.label))
          .padding(theme::Edge::horizontal, theme::metric::menu_item_padding)
          .frame({.max_width = composition::infinity,
                  .min_height = theme::metric::menu_item_height,
                  .alignment = composition::Alignment::leading})
          .foreground_style(ink);
  if (lit) {
    row = std::move(row).background(
        destructive ? theme::ColourRole::destructive
                    : (configuration.is_pressed
                           ? theme::ColourRole::accent_pressed
                           : theme::ColourRole::selected_content_background),
        theme::Shape::rounded(theme::radius::control, true));
    if (destructive && configuration.is_pressed) {
      row = std::move(row).opacity(
          components::ButtonStyle::prominent_pressed_opacity);
    }
  }
  if (!configuration.is_enabled) {
    row = std::move(row).opacity(components::ButtonStyle::disabled_opacity);
  }
  return row;
}

auto MenuPanel::items(composition::Element declared,
                      composition::Binding<bool> shown)
    -> composition::Element {
  return items(std::move(declared), std::move(shown), false, true);
}

auto MenuPanel::items(composition::Element declared,
                      composition::Binding<bool> shown, bool reversed,
                      bool dismisses) -> composition::Element {
  std::vector<composition::Element> members =
      components::Group::members(std::move(declared));
  if (reversed) {
    std::ranges::reverse(members);
  }
  std::vector<composition::Element> rows;
  for (composition::Element& member : members) {
    if (const auto button = components::Button::content(member)) {
      const auto own = member.declared_environment(
          composition::ControlEnvironment::menu_action_dismiss_behavior);
      const bool closes =
          own.has_value()
              ? own->get() != composition::MenuActionDismissBehavior::disabled
              : dismisses;
      if (closes) {
        button->get().then([shown] { shown.set(false); });
      }
      rows.push_back(
          std::move(member).frame({.max_width = composition::infinity}));
      continue;
    }
    rows.push_back(std::move(member));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::none},
             std::move(rows))
      .padding(theme::Edge::vertical, theme::metric::menu_vertical_padding)
      .frame(
          {.width = panel_width, .alignment = composition::Alignment::leading})
      .button_style(ItemStyle{})
      .focus_effect_disabled();
}

}
