#include <text_formats_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/date_interval.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/system_format_style.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/time_data_source.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/text/byte_count_format_style.hpp>
#include <arisa/text/line_height.hpp>
#include <arisa/text/line_pattern.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/text/measurement.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

namespace arisa::examples {

using composition::Element;
using composition::infinity;

auto TextFormatsPage::section(std::string_view title, Element content)
    -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             components::Text(std::string{title}).font(theme::Font::headline),
             std::move(content))
      .frame({.max_width = infinity,
              .alignment = composition::Alignment::leading});
}

auto TextFormatsPage::live(Date now) -> Element {
  const Date hour = std::chrono::floor<std::chrono::hours>(now);
  const Date day = std::chrono::floor<std::chrono::days>(now);
  const Date minute = std::chrono::floor<std::chrono::minutes>(now);
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::sm},
      components::Text::timer_interval(
          components::DateInterval{.start = hour,
                                   .end = hour + std::chrono::hours{1}})
          .font(theme::Font::title.monospaced_digit())
          .id(std::string{timer_text}),
      components::Text(hour, components::Text::DateStyle::relative)
          .id(std::string{relative_text}),
      components::Text(day, components::Text::DateStyle::offset)
          .id(std::string{offset_text}),
      components::Text(
          components::TimeDataSource<Date>::current_date(),
          components::SystemFormatStyle::Stopwatch{
              minute, true, 4, components::SystemFormatStyle::Duration{1.0}})
          .monospaced_digit()
          .id(std::string{stopwatch_text}),
      components::Text(components::TimeDataSource<Date>::current_date(),
                       components::SystemFormatStyle::DateReference{day}));
}

auto TextFormatsPage::formats(composition::StateHandle<int> count) -> Element {
  const int shown = count.get();
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::sm},
      components::Text(1234.5, text::NumberFormatStyle::currency("USD")),
      components::Text(
          1234.5, text::NumberFormatStyle::currency("EUR").presentation(
                      text::NumberFormatStyle::CurrencyPresentation::iso_code)),
      components::Text(
          std::int64_t{1500000},
          text::ByteCountFormatStyle{text::ByteCountFormatStyle::Style::file}),
      components::Text(
          text::Measurement{21.0, text::Measurement::Unit::kilometers},
          text::MeasurementFormatStyle{
              text::MeasurementFormatStyle::UnitWidth::wide}),
      components::Text(text::LocalizedStringKey{
          "{} items for {}",
          text::LocalizedStringKey::specified(std::int64_t{shown}, "%d"),
          text::LocalizedStringKey::formatted(
              shown * 2.5, text::NumberFormatStyle::currency("USD"))}),
      components::Button("Add item", [count] {
        count.set(count.get() + 1);
      }).id(std::string{count_button}));
}

auto TextFormatsPage::styled_runs() -> components::AttributedString {
  using Attributes = components::AttributedString::Attributes;
  components::AttributedString text{"Big "};
  text = text.setting<
      components::AttributedString::Scopes::SwiftUIAttributes::FontAttribute>(
      theme::Font::system(28.0F), 0, 3);
  text.append("spaced ", Attributes{.kern = 3.0F});
  text.append("marked ",
              Attributes{.background = theme::Colour::hex(0xFFE58A)});
  text.append("raised", Attributes{.baseline_offset = 6.0F});
  text.append(" dashed",
              Attributes{.underline_style = composition::TextDecoration{
                             .active = true,
                             .colour = theme::Colour::hex(0x2F6FD6),
                             .pattern = text::LinePattern::dash}});
  return text;
}

auto TextFormatsPage::runs() -> Element {
  return components::Text(styled_runs())
      .font(theme::Font::body)
      .id(std::string{runs_text});
}

auto TextFormatsPage::lines() -> Element {
  return components::HStack(
      components::HStackOptions{.alignment = components::VerticalAlignment::top,
                                .spacing = theme::space::xl},
      components::Text("Tight\nline\nheight")
          .line_height(text::LineHeight::tight),
      components::Text("Loose\nline\nheight")
          .line_height(text::LineHeight::loose),
      components::Text("Small Caps").font(theme::Font::title2.small_caps()),
      components::Text("Dotted")
          .underline(true, text::LinePattern::dot)
          .strikethrough(true, text::LinePattern::dash_dot),
      components::Text("One line")
          .line_limit(composition::LinesBetween{.minimum = 2, .maximum = 3}));
}

auto TextFormatsPage::body(composition::Context& context) const -> Element {
  const auto shown_flag = shown_;
  const auto count = context.state(items);
  const Date now = components::TimedWake::now(context);
  return presentation::NavigationStack([=] {
    return components::ScrollView(
               components::VStack(
                   components::VStackOptions{
                       .alignment = components::HorizontalAlignment::leading,
                       .spacing = theme::space::xl},
                   section("Live", live(now)),
                   section("Formats", formats(count)), section("Runs", runs()),
                   section("Lines", lines()))
                   .padding(theme::space::xl)
                   .frame({.max_width = infinity,
                           .alignment = composition::Alignment::leading}))
        .frame({.max_width = infinity, .max_height = infinity})
        .navigation_title("Text formats")
        .toolbar(composition::ToolbarItem(
            composition::ToolbarItemPlacement::cancellation_action,
            components::Button("Close", [shown_flag] {
              shown_flag.set(false);
            }).id(std::string{close_button})));
  });
}

auto TextFormatsPage::Opener::body(composition::Context& context) const
    -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Text formats", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown,
                         [shown] { return Element{TextFormatsPage{shown}}; });
}

}
