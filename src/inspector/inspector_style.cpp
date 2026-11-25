#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/label_style.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/inspector/inspector_style.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <format>
#include <utility>

namespace arisa::inspector {

auto InspectorStyle::heading(std::string_view text) -> composition::Element {
  return components::Text(std::string{text}).font(theme::Font::headline);
}

auto InspectorStyle::line(std::string text) -> composition::Element {
  return components::Text(std::move(text)).font(theme::Font::footnote);
}

auto InspectorStyle::value(std::string_view label, std::string text)
    -> composition::Element {
  return composition::Element{
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::top,
              .spacing = theme::space::sm},
          composition::Element{
              components::Text(std::string{label}).font(theme::Font::footnote)}
              .frame({.width = 150.0F,
                      .alignment = composition::Alignment::leading}),
          composition::Element{
              components::Text(std::move(text)).font(theme::Font::footnote)}
              .frame({.max_width = composition::infinity,
                      .alignment = composition::Alignment::leading}))}
      .frame({.max_width = composition::infinity});
}

auto InspectorStyle::section(std::string_view title,
                             std::vector<composition::Element> rows)
    -> composition::Element {
  rows.insert(rows.begin(), heading(title));
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::xs},
          std::move(rows))}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

auto InspectorStyle::tab(std::string_view label, bool selected,
                         components::Button::Action action)
    -> composition::Element {
  return composition::Element{
      components::Button(
          std::move(action),
          components::Text(std::string{label}).font(theme::Font::footnote))}
      .button_style(selected ? components::ButtonStyle::bordered_prominent()
                             : components::ButtonStyle::bordered());
}

auto InspectorStyle::icon(std::string_view label, std::string_view symbol,
                          bool selected, components::Button::Action action)
    -> composition::Element {
  return composition::Element{
      components::Button(
          std::move(action),
          composition::Element{
              components::Label(
                  std::string{label},
                  composition::Element{
                      components::Image::system_name(std::string{symbol})}
                      .font(theme::Font::footnote))}
              .label_style(components::IconOnlyLabelStyle{}))}
      .button_style(selected ? components::ButtonStyle::bordered_prominent()
                             : components::ButtonStyle::bordered());
}

auto InspectorStyle::bytes(std::uint64_t count) -> std::string {
  constexpr std::array units{"B", "KB", "MB", "GB"};
  auto amount = static_cast<double>(count);
  std::size_t unit = 0;
  while (amount >= 1024.0 && unit + 1 < units.size()) {
    amount /= 1024.0;
    ++unit;
  }
  if (unit == 0) {
    return std::format("{} B", count);
  }
  return std::format("{:.1f} {}", amount, units.at(unit));
}

auto InspectorStyle::milliseconds(std::chrono::nanoseconds value)
    -> std::string {
  return std::format("{:.3f} ms",
                     std::chrono::duration<double, std::milli>(value).count());
}

auto InspectorStyle::rect(const composition::Rect& rect) -> std::string {
  return std::format("{:.1f}, {:.1f}  {:.1f} x {:.1f}", rect.x, rect.y,
                     rect.width, rect.height);
}

auto InspectorStyle::space(const theme::Space& space) -> std::string {
  if (const float* points = space.points()) {
    return std::format("{:g}", *points);
  }
  if (const theme::SpaceToken* token = space.token()) {
    constexpr std::array names{"none", "xxs", "xs", "sm",
                               "md",   "lg",  "xl", "xxl"};
    return std::string{names.at(static_cast<std::size_t>(*token))};
  }
  return "scaled";
}

auto InspectorStyle::insets(const std::optional<theme::Insets>& insets)
    -> std::string {
  if (!insets.has_value()) {
    return "none";
  }
  return std::format("{} {} {} {}", space(insets->top), space(insets->leading),
                     space(insets->bottom), space(insets->trailing));
}

auto InspectorStyle::flags(const runtime::Introspection::Flags& flags)
    -> std::string {
  std::string joined;
  const auto add = [&joined](bool set, std::string_view name) {
    if (!set) {
      return;
    }
    if (!joined.empty()) {
      joined += ", ";
    }
    joined += name;
  };
  add(flags.focusable, "focusable");
  add(flags.clips, "clip");
  add(flags.glass, "glass");
  add(flags.promoted, "layer-promoted");
  add(flags.layer, "overlay");
  add(flags.animating, "animating");
  return joined.empty() ? std::string{"none"} : joined;
}

}
