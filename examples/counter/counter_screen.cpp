#include <counter_screen.hpp>

#include <arisa/async/awaitables.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/error_boundary.hpp>
#include <arisa/composition/ui_error.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/typography.hpp>

#include <chrono>
#include <format>
#include <ranges>
#include <string>

namespace arisa::examples {

using namespace std::chrono_literals;

auto CounterScreen::body(composition::Context& context) const
    -> composition::Element {
  const auto clicks = context.state(count);
  const auto seconds = context.state(uptime);
  const auto requests = context.state(name_requests);
  const auto shown = context.state(name);
  const auto failing = context.state(exploding);
  const auto attempts = context.state(attempt);
  return components::VStack(
             {.alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::md},
             components::Text(std::format("count: {}", clicks.get()))
                 .font(theme::Font::title),
             components::Button(
                 "increment",
                 [clicks] { clicks.update([](int& value) { ++value; }); })
                 .id(increment),
             components::Text(std::format("uptime: {} s", seconds.get()))
                 .font(theme::Font::callout)
                 .foreground_style(theme::ColourRole::text_secondary)
                 .id("uptime")
                 .task([seconds] { return tick(seconds); }),
             components::HStack(
                 {.spacing = theme::space::md},
                 components::Button("name",
                                    [requests] {
                                      requests.update(
                                          [](int& value) { ++value; });
                                    })
                     .id(name_button),
                 components::Text(std::format("name: {}", shown.get())))
                 .task(requests.get(),
                       [request = requests.get(), text_input = text_input_,
                        shown] { return ask(request, text_input, shown); }),
             components::Button("fail", [failing] { failing.set(true); })
                 .id(fail_button),
             composition::ErrorBoundary(
                 Fragile{failing.get()},
                 [failing, attempts](const composition::UiError& error) {
                   return components::HStack(
                       {.spacing = theme::space::md},
                       components::Text(
                           std::format("recovered: {}", error.message()))
                           .foreground_style(theme::ColourRole::destructive),
                       components::Button("retry", [failing, attempts] {
                         failing.set(false);
                         attempts.update([](int& value) { ++value; });
                       }).id(retry_button));
                 })
                 .id(attempts.get()),
             components::VStack(
                 {.alignment = components::HorizontalAlignment::leading,
                  .spacing = theme::space::xxs},
                 components::ForEach(
                     std::views::iota(0, clicks.get()),
                     [](int item) { return item; },
                     [](int item) {
                       return components::Text(std::format("item {}", item + 1))
                           .font(theme::Font::footnote);
                     })),
             Badge{})
      .padding(theme::space::xl)
      .frame({.min_width = 420.0F})
      .background(
          theme::ColourRole::surface_raised,
          theme::RoundedRectangle(theme::radius::card,
                                  theme::RoundedCornerStyle::continuous))
      .border(theme::ColourRole::separator, theme::Stroke::hairline())
      .shadow(theme::Elevation::medium)
      .environment(accent, accent_name);
}

auto CounterScreen::tick(composition::StateHandle<int> seconds)
    -> async::Task<void> {
  while (true) {
    co_await async::sleep(1s);
    seconds.update([](int& value) { ++value; });
  }
}

auto CounterScreen::ask(int request, runtime::TextInput text_input,
                        composition::StateHandle<std::string> shown)
    -> async::Task<void> {
  if (request == 0) {
    co_return;
  }
  const runtime::TextInput::Result result =
      co_await text_input.request(provider::TextInputRequest{
          .initial = "",
          .hint = "Your name",
          .max_length = provider::TextInputLimit::of<32>(),
          .kind = provider::TextInputKind::text});
  shown.set(result.has_value()
                ? *result
                : std::format("({})", result.error().description()));
}

auto CounterScreen::Badge::body(composition::Context&) const
    -> composition::Element {
  return components::HStack(
      components::Text("accent:")
          .font(theme::Font::caption)
          .foreground_style(theme::ColourRole::text_secondary),
      Label{});
}

auto CounterScreen::Label::body(composition::Context& context) const
    -> composition::Element {
  return components::Text(std::string{context.environment(accent)})
      .font(theme::Font::caption)
      .foreground_style(theme::ColourRole::accent);
}

auto CounterScreen::Fragile::body(composition::Context& context) const
    -> composition::Element {
  if (exploding_) {
    context.fail(composition::UiError{"the fragile child failed on purpose"});
  }
  return components::Text("fragile child: fine");
}

}
