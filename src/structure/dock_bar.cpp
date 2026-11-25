#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/default_focus_evaluation_priority.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/structure/dock_bar.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <memory>
#include <utility>

namespace arisa::structure {

DockBar::DockBar(composition::Binding<std::size_t> selection,
                 std::vector<Item> items)
    : DockBar(std::move(selection), std::move(items), {}) {}

DockBar::DockBar(composition::Binding<std::size_t> selection,
                 std::vector<Item> items, std::vector<Action> trailing)
    : DockBar(std::move(selection), std::move(items), std::move(trailing),
              Options{}) {}

DockBar::DockBar(composition::Binding<std::size_t> selection,
                 std::vector<Item> items, std::vector<Action> trailing,
                 Options options)
    : Element(std::make_unique<Content>(std::move(selection), std::move(items),
                                        std::move(trailing), options)) {}

auto DockBar::key_of(std::string_view kind, std::size_t index) -> std::string {
  return std::format("{}.{}", kind, index);
}

auto DockBar::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto DockBar::Content::label(const std::string& title,
                             const std::string& symbol, bool labelled,
                             const Metrics& sizes) -> composition::Element {
  composition::Element icon =
      components::Image(components::Image::SystemName{symbol})
          .font(theme::Font::system(sizes.icon_size,
                                    theme::Font::Weight::medium));
  if (!labelled) {
    return std::move(icon)
        .frame({.width = sizes.icon_tab_width, .height = sizes.icon_tab_height})
        .accessibility_label(composition::AccessibilityText{title});
  }
  const bool compact = sizes.tab_width < tab_width;
  return components::VStack(
             components::VStackOptions{.spacing =
                                           theme::Space{sizes.label_spacing}},
             std::move(icon),
             components::Text(title)
                 .font(compact ? theme::Font::caption2 : theme::Font::caption)
                 .line_limit(1))
      .frame({.width = sizes.tab_width, .height = sizes.tab_height});
}

auto DockBar::Content::tab(
    std::size_t index, bool selected, bool labelled, const Metrics& sizes,
    const composition::FocusBinding<std::size_t>& focus) const
    -> composition::Element {
  const Item& item = items_[index];
  composition::Element shown =
      label(item.title, item.system_image, labelled, sizes)
          .background(selected ? theme::ColourValue{selection_fill}.opacity(
                                     selection_opacity)
                               : theme::ColourValue{theme::Colour{0.0F, 0.0F,
                                                                  0.0F, 0.0F}},
                      theme::Shape::capsule())
          .content_shape(composition::ContentShapeKinds::interaction,
                         theme::Shape::rectangle());
  composition::Element button =
      components::Button(
          [selection = selection_, index] { selection.set(index); },
          std::move(shown))
          .button_style(components::ButtonStyle::plain())
          .content_shape(composition::ContentShapeKinds::focus_effect,
                         theme::Shape::capsule())
          .foreground_style(selected ? theme::ColourRole::text_primary
                                     : theme::ColourRole::text_secondary)
          .focused(focus, index)
          .id(DockBar::key_of(tab_key, index));
  if (selected) {
    button = std::move(button).accessibility_add_traits(
        composition::AccessibilityTrait::is_selected);
  }
  return button;
}

auto DockBar::Content::action(const Action& chosen, std::size_t index,
                              bool labelled, const Metrics& sizes)
    -> composition::Element {
  return components::Button(
             [perform = chosen.perform] {
               if (perform) {
                 perform();
               }
             },
             label(chosen.title, chosen.system_image, labelled, sizes)
                 .content_shape(composition::ContentShapeKinds::interaction,
                                theme::Shape::rectangle()))
      .button_style(components::ButtonStyle::plain())
      .content_shape(composition::ContentShapeKinds::focus_effect,
                     theme::Shape::capsule())
      .foreground_style(theme::ColourRole::text_secondary)
      .id(DockBar::key_of(action_key, index));
}

auto DockBar::Content::divider(const Metrics& sizes) const
    -> composition::Element {
  const bool vertical = options_.axis == composition::Axis::vertical;
  composition::Element rule =
      components::ZStack()
          .frame({.width = vertical ? sizes.divider_length : divider_width,
                  .height = vertical ? divider_width : sizes.divider_length})
          .background(theme::ColourValue{rule_colour}.opacity(rule_opacity));
  if (vertical) {
    return components::VStack(std::move(rule))
        .padding(theme::Edge::vertical, theme::Space{divider_padding});
  }
  return components::HStack(std::move(rule))
      .padding(theme::Edge::horizontal, theme::Space{divider_padding});
}

auto DockBar::Content::row_width(bool labelled,
                                 const Metrics& sizes) const noexcept -> float {
  const float tab = labelled ? sizes.tab_width : sizes.icon_tab_width;
  std::size_t cells = items_.size();
  float width = static_cast<float>(items_.size()) * tab;
  if (!trailing_.empty()) {
    cells += trailing_.size() + 1;
    width += divider_width + (2.0F * divider_padding) +
             (static_cast<float>(trailing_.size()) * tab);
  }
  if (cells > 1) {
    width += static_cast<float>(cells - 1) * sizes.tab_spacing;
  }
  return width + (2.0F * sizes.padding);
}

auto DockBar::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const bool labelled =
      context.environment(composition::ControlEnvironment::labels_visibility) !=
      composition::Visibility::hidden;
  const components::ControlSize size =
      context.environment(components::Environment::control_size);
  const Metrics sizes = metrics(size == components::ControlSize::small ||
                                size == components::ControlSize::mini);
  const bool vertical = options_.axis == composition::Axis::vertical;
  const composition::FocusBinding<std::size_t> focus = context.state(tab_focus);
  const std::size_t selected = selection_.get();
  std::vector<composition::Element> row;
  row.reserve(items_.size() + trailing_.size() + 2);
  for (std::size_t index = 0; index < items_.size(); ++index) {
    row.push_back(tab(index, index == selected, labelled, sizes, focus));
  }
  if (!trailing_.empty()) {
    if (vertical) {
      row.emplace_back(components::Spacer());
    }
    row.push_back(divider(sizes));
    for (std::size_t index = 0; index < trailing_.size(); ++index) {
      row.push_back(action(trailing_[index], index, labelled, sizes));
    }
  }
  const theme::Glass glass =
      context.environment(components::Environment::theme).glass().control;
  const theme::Shape shape = vertical ? theme::Shape::rounded(rail_radius, true)
                                      : theme::Shape::capsule();
  composition::Element dock =
      vertical ? composition::Element{components::VStack(
                     components::VStackOptions{
                         .alignment = components::HorizontalAlignment::center,
                         .spacing = theme::Space{sizes.tab_spacing}},
                     std::move(row))}
               : composition::Element{components::HStack(
                     components::HStackOptions{
                         .alignment = components::VerticalAlignment::center,
                         .spacing = theme::Space{sizes.tab_spacing}},
                     std::move(row))};
  dock = std::move(dock).padding(theme::Space{sizes.padding});
  if (vertical) {
    dock = std::move(dock).frame({.max_height = composition::infinity});
  } else if (options_.scrolls) {
    dock = components::ScrollView(composition::Axes::horizontal, false,
                                  std::move(dock))
               .frame({.max_width = row_width(labelled, sizes)});
  }
  if (glass.variant() == theme::Glass::Variant::identity) {
    dock = std::move(dock).background(theme::ColourRole::surface_raised, shape);
  } else {
    dock = std::move(dock).glass_effect(glass, shape);
  }
  dock =
      std::move(dock)
          .shadow(
              theme::ColourValue{theme::Colour{0.0F, 0.0F, 0.0F, 1.0F}}.opacity(
                  shadow_opacity),
              shadow_radius, 0.0F, shadow_offset)
          .accessibility_add_traits(composition::AccessibilityTrait::is_tab_bar)
          .focus_section();
  if (selected < items_.size()) {
    dock = std::move(dock)
               .default_focus(focus, selected)
               .on_change(selected, [focus, selected] {
                 const std::optional<std::size_t>& held = focus.get();
                 if (held.has_value() && *held != selected) {
                   focus.set(selected);
                 }
               });
  }
  if (options_.follows_focus) {
    dock = std::move(dock).on_change(
        focus.get(), [selection = selection_, count = items_.size()](
                         const std::optional<std::size_t>&,
                         const std::optional<std::size_t>& now) {
          if (now.has_value() && *now < count && *now != selection.get()) {
            selection.set(*now);
          }
        });
  }
  std::vector<composition::Element> produced;
  if (vertical) {
    produced.push_back(std::move(dock).fixed_size(true, false));
  } else if (!options_.scrolls) {
    produced.push_back(std::move(dock).fixed_size());
  } else {
    produced.push_back(
        components::HStack(
            components::HStackOptions{.spacing = theme::space::none},
            components::Spacer(0.0F),
            std::move(dock).fixed_size(false, true).id(std::string{bar_key}),
            components::Spacer(0.0F))
            .fixed_size(false, true));
  }
  return produced;
}

}
