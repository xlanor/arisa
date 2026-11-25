#include <arisa/async/awaitables.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/status/battery.hpp>
#include <arisa/components/text.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <format>
#include <string>
#include <utility>

namespace arisa::components::status {

auto Battery::symbol(const provider::BatteryState& state) noexcept
    -> std::string_view {
  constexpr std::array<std::string_view, 7> bars{
      "battery_0_bar", "battery_1_bar", "battery_2_bar", "battery_3_bar",
      "battery_4_bar", "battery_5_bar", "battery_6_bar"};
  const unsigned percent = state.percent.value();
  if (percent >= 95U) {
    return "battery_full";
  }
  return bars[(percent * bars.size()) / 100U];
}

auto Battery::body(composition::Context& context) const
    -> composition::Element {
  const auto tick = context.state(ticks);
  const auto state = context.environment(Environment::system_status).battery();
  if (!state.has_value()) {
    return Group();
  }
  auto level = Image::system_name(std::string{symbol(*state)}).font(symbol_);
  if (!state->charging) {
    return HStack(HStackOptions{.alignment = VerticalAlignment::center,
                                .spacing = theme::space::xxs},
                  Text(std::format("{}%", state->percent.value())).font(label_),
                  std::move(level))
        .task([tick] { return refresh(tick); });
  }
  return HStack(HStackOptions{.alignment = VerticalAlignment::center,
                              .spacing = theme::space::xxs},
                Text(std::format("{}%", state->percent.value())).font(label_),
                Image::system_name("bolt.fill")
                    .font(symbol_)
                    .foreground_style(theme::ColourRole::yellow),
                std::move(level).foreground_style(theme::ColourRole::green))
      .task([tick] { return refresh(tick); });
}

auto Battery::refresh(composition::StateHandle<int> count)
    -> async::Task<void> {
  while (true) {
    co_await async::sleep(interval);
    count.update([](int& value) { ++value; });
  }
}

}
