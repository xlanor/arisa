#include <arisa/async/awaitables.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/status/wireless.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <string>

namespace arisa::components::status {

auto Wireless::symbol(const provider::NetworkState& state) noexcept
    -> std::string_view {
  constexpr std::array<std::string_view, 4> bars{
      "signal_wifi_0_bar", "network_wifi_1_bar", "network_wifi_2_bar",
      "signal_wifi_4_bar"};
  switch (state.kind) {
    case provider::NetworkKind::none:
      return "wifi_off";
    case provider::NetworkKind::ethernet:
      return "lan";
    case provider::NetworkKind::wifi:
      return bars[state.signal_bars.value()];
  }
  return "wifi_off";
}

auto Wireless::body(composition::Context& context) const
    -> composition::Element {
  const auto tick = context.state(ticks);
  const auto state = context.environment(Environment::system_status).network();
  if (!state.has_value()) {
    return Group();
  }
  return Image::system_name(std::string{symbol(*state)})
      .font(font_)
      .task([tick] { return refresh(tick); });
}

auto Wireless::refresh(composition::StateHandle<int> count)
    -> async::Task<void> {
  while (true) {
    co_await async::sleep(interval);
    count.update([](int& value) { ++value; });
  }
}

}
