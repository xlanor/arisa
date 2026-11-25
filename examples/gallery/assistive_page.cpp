#include <assistive_page.hpp>

#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/appearance_settings.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/accessibility_action_category.hpp>
#include <arisa/composition/accessibility_action_kind.hpp>
#include <arisa/composition/accessibility_adjustment_direction.hpp>
#include <arisa/composition/accessibility_rotor_entry.hpp>
#include <arisa/composition/accessibility_system_rotor.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using theme::ColourRole;

auto AssistivePage::Demos::card(std::string_view title, Element content)
    -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = composition::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             components::Text(std::string{title})
                 .font(theme::Font::headline)
                 .accessibility_heading(
                     composition::AccessibilityHeadingLevel::h2),
             std::move(content))
      .padding(theme::space::lg)
      .frame(
          {.max_width = infinity, .alignment = composition::Alignment::leading})
      .background(ColourRole::surface_raised,
                  theme::RoundedRectangle(theme::radius::card));
}

auto AssistivePage::Demos::settings(composition::Context& context) -> Element {
  const std::optional<appearance::AppearanceModel>& model =
      context.environment(components::Environment::appearance);
  if (!model.has_value()) {
    return components::Text("Settings");
  }
  const appearance::AppearanceSettings& current =
      context.read(model->settings());
  using Overrides = appearance::AppearanceOverrides;
  const auto toggle = [held = *model](std::string_view title, bool on,
                                      std::optional<bool> Overrides::* slot) {
    return Element{controls::Toggle(
        composition::Binding<bool>{[on] { return on; },
                                   [held, slot](bool value) {
                                     held.update([slot, value](Overrides& o) {
                                       o.*slot = value;
                                     });
                                   }},
        components::Text(text::LocalizedStringKey{std::string{title}}))};
  };
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::xs},
      toggle("VoiceOver", current.voice_over, &Overrides::voice_over),
      toggle("Switch Control", current.switch_control,
             &Overrides::switch_control),
      toggle("Invert colours", current.invert_colors,
             &Overrides::invert_colors),
      toggle("Play animated images", current.play_animated_images,
             &Overrides::play_animated_images),
      toggle("Prefer cross-fade transitions",
             current.prefers_cross_fade_transitions,
             &Overrides::prefers_cross_fade_transitions),
      toggle("Reduce highlighting effects", current.reduce_highlighting_effects,
             &Overrides::reduce_highlighting_effects),
      toggle("Show borders", current.show_borders, &Overrides::show_borders),
      toggle("Assistive Access", current.assistive_access,
             &Overrides::assistive_access));
}

auto AssistivePage::Demos::actions(composition::StateHandle<int> archived)
    -> Element {
  const int count = archived.get();
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::xs},
      Element{components::Button("Weekly report", [] {})}
          .accessibility_action(
              text::LocalizedStringKey{"Archive"},
              [archived] { archived.set(archived.get() + 1); })
          .accessibility_action(composition::AccessibilityActionKind::delete_,
                                [archived] { archived.set(0); })
          .accessibility_actions(
              composition::AccessibilityActionCategory::edit,
              [archived] {
                return Element{components::Button(
                    "Flag", [archived] { archived.set(archived.get() + 10); })};
              })
          .accessibility_input_labels({"Report", "Weekly"})
          .id(std::string{mail_row}),
      components::Text(std::format("Archived {}", count)));
}

auto AssistivePage::Demos::adjustable(composition::StateHandle<int> rating,
                                      composition::StateHandle<double> volume)
    -> Element {
  const int stars = rating.get();
  std::vector<Element> shown;
  for (int index = 0; index < 5; ++index) {
    shown.emplace_back(
        components::Image::system_name(index < stars ? "star.fill" : "star"));
  }
  const double level = volume.get();
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::sm},
      components::HStack(components::HStackOptions{.spacing = theme::space::xs},
                         std::move(shown))
          .accessibility_element()
          .accessibility_label("Rating")
          .accessibility_value(std::format("{} of 5", stars))
          .accessibility_adjustable_action(
              [rating](
                  composition::AccessibilityAdjustmentDirection direction) {
                const int step =
                    direction == composition::AccessibilityAdjustmentDirection::
                                     increment
                        ? 1
                        : -1;
                rating.set(std::clamp(rating.get() + step, 0, 5));
              })
          .focusable()
          .id(std::string{rating_row}),
      components::ZStack(
          components::ZStack()
              .frame({.width = static_cast<float>(level) * 240.0F,
                      .height = 12.0F})
              .background(ColourRole::accent, theme::Shape::capsule())
              .frame({.width = 240.0F,
                      .alignment = composition::Alignment::leading}))
          .frame({.width = 240.0F, .height = 12.0F})
          .background(ColourRole::system_fill, theme::Shape::capsule())
          .accessibility_representation([volume] {
            return Element{
                controls::Slider(composition::Binding<double>{volume})}
                .accessibility_label("Volume");
          })
          .focusable()
          .id(std::string{volume_row}));
}

auto AssistivePage::Demos::rotors() -> Element {
  const composition::Namespace::ID space = rotor_space.id();
  return components::VStack(
             components::VStackOptions{
                 .alignment = composition::HorizontalAlignment::leading,
                 .spacing = theme::space::xs},
             components::Text("Inbox").accessibility_rotor_entry("inbox",
                                                                 space),
             components::Text("Drafts").accessibility_rotor_entry("drafts",
                                                                  space),
             components::Text("Sent").accessibility_rotor_entry("sent", space))
      .accessibility_element(composition::AccessibilityChildBehavior::contain)
      .accessibility_rotor(
          composition::AccessibilityText{text::LocalizedStringKey{"Mailboxes"}},
          [space] {
            return std::vector<composition::AccessibilityRotorEntry>{
                composition::AccessibilityRotorEntry{"Inbox", "inbox", space},
                composition::AccessibilityRotorEntry{"Sent", "sent", space}};
          });
}

auto AssistivePage::Demos::focus(
    const composition::AccessibilityFocusBinding<bool>& summary) -> Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::sm},
      Element{components::Button("Jump to summary",
                                 [summary] { summary.set(true); })}
          .id(std::string{jump_button}),
      Element{components::Button("Summary", [] {})}
          .accessibility_focused(summary)
          .speech_spells_out_characters()
          .id(std::string{summary_button}));
}

auto AssistivePage::Demos::appearance() -> Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      components::ZStack()
          .frame({.width = 64.0F, .height = 64.0F})
          .background(theme::Colour::hex(0xFF9500),
                      theme::Shape::rounded(theme::radius::control))
          .accessibility_ignores_invert_colors()
          .accessibility_label("Photo")
          .id(std::string{swatch}),
      Element{components::Button("Glass", [] {})}.button_style(
          components::ButtonStyle::glass()),
      components::Text("Large text")
          .accessibility_shows_large_content_viewer());
}

auto AssistivePage::Demos::body(composition::Context& context) const
    -> Element {
  const composition::AccessibilityFocusBinding<bool> summary =
      context.state(summary_focus);
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::lg},
      card("Assistive settings", settings(context)),
      card("Custom actions", actions(context.state(archived_key))),
      card("Adjustable",
           adjustable(context.state(rating_key), context.state(volume_key))),
      card("Rotors", rotors()), card("Accessibility focus", focus(summary)),
      card("Appearance", appearance()));
}

auto AssistivePage::body(composition::Context&) const -> Element {
  const auto shown_flag = shown_;
  return presentation::NavigationStack([shown_flag] {
    return Element{
        components::ScrollView(Element{Demos{}}.padding(theme::space::lg))
            .navigation_title("Assistive technology")
            .toolbar(composition::ToolbarItem(
                composition::ToolbarItemPlacement::cancellation_action,
                components::Button("Close", [shown_flag] {
                  shown_flag.set(false);
                }).id(std::string{close_button})))};
  });
}

auto AssistivePage::Opener::body(composition::Context& context) const
    -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Assistive technology",
                            [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown,
                         [shown] { return Element{AssistivePage{shown}}; });
}

}
