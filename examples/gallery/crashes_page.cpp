#include <crashes_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using provider::CrashDrill;
using provider::CrashReporter;
using provider::ExceptionDelivery;

auto CrashesPage::key(CrashDrill drill) -> std::string {
  return std::format("crashes-{}", static_cast<int>(drill));
}

auto CrashesPage::title(CrashDrill drill) noexcept -> std::string_view {
  switch (drill) {
    case CrashDrill::null_read:
      return "Null read";
    case CrashDrill::trap:
      return "Trap";
    case CrashDrill::abort:
      return "Abort";
    case CrashDrill::terminate:
      return "Terminate";
    case CrashDrill::failed_assert:
      return "Failed assert";
    case CrashDrill::stack_overflow:
      return "Stack overflow";
    case CrashDrill::diag_abort:
      return "Diag abort";
  }
  return "Abort";
}

auto CrashesPage::card(std::string_view title, Element content) -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             components::Text(std::string{title}).font(theme::Font::headline),
             std::move(content))
      .padding(theme::space::lg)
      .frame(
          {.max_width = infinity, .alignment = composition::Alignment::leading})
      .background(
          theme::ColourRole::surface_raised,
          theme::RoundedRectangle(theme::radius::card,
                                  theme::RoundedCornerStyle::continuous))
      .border(theme::ColourRole::separator, theme::Stroke::hairline());
}

auto CrashesPage::drills() -> Element {
  std::vector<Element> buttons;
  for (const CrashDrill drill : CrashReporter::drills) {
    buttons.push_back(components::Button(std::string{title(drill)},
                                         [drill] {
                                           if (CrashReporter::armed()) {
                                             CrashReporter::drill(drill);
                                           }
                                         })
                          .button_style(components::ButtonStyle::bordered())
                          .id(key(drill)));
  }
  std::vector<Element> rows;
  rows.push_back(components::HStack(
      components::HStackOptions{.spacing = theme::space::sm, .wrap = true},
      std::move(buttons)));
  if (!CrashReporter::armed()) {
    rows.push_back(components::Text("Reporter not installed")
                       .font(theme::Font::callout)
                       .foreground_style(theme::ColourRole::secondary_label));
  }
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::md},
      std::move(rows));
}

auto CrashesPage::probe(composition::StateHandle<int> answer) -> Element {
  const std::string_view shown = answer.get() == 0   ? "Not checked"
                                 : answer.get() == 1 ? "Delivered"
                                                     : "Unsupported";
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::lg},
      components::Button("Check delivery",
                         [answer] {
                           answer.set(CrashReporter::exception_delivery() ==
                                              ExceptionDelivery::delivered
                                          ? 1
                                          : 2);
                         })
          .button_style(components::ButtonStyle::bordered())
          .id(delivery_key),
      components::Text(std::string{shown}).font(theme::Font::callout));
}

auto CrashesPage::body(composition::Context& context) const -> Element {
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::lg},
                 card("Exception delivery", probe(context.state(delivery))),
                 card("Crash", drills()))
                 .frame({.max_width = infinity,
                         .alignment = composition::Alignment::leading}))
      .content_margins(theme::space::sm)
      .frame({.max_width = infinity, .max_height = infinity});
}

}
