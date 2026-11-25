#include <arisa/async/awaitables.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/status/clock.hpp>
#include <arisa/components/text.hpp>
#include <arisa/theme/typography.hpp>

#include <format>

namespace arisa::components::status {

auto Clock::text(SystemStatus::LocalTime time) -> std::string {
  return std::format("{:%H:%M}", time);
}

auto Clock::until_next_minute(SystemStatus::LocalTime time) noexcept
    -> std::chrono::seconds {
  const auto into = time.time_since_epoch() % minute;
  return minute - into;
}

auto Clock::body(composition::Context& context) const -> composition::Element {
  const auto tick = context.state(ticks);
  const SystemStatus& status = context.environment(Environment::system_status);
  const auto now = status.local_time();
  if (!now.has_value()) {
    return Group();
  }
  return Text(text(*now)).font(font_).task([tick, status] {
    return refresh(tick, status);
  });
}

auto Clock::refresh(composition::StateHandle<int> count, SystemStatus status)
    -> async::Task<void> {
  while (true) {
    const auto now = status.local_time();
    co_await async::sleep(now.has_value() ? until_next_minute(*now) : minute);
    count.update([](int& value) { ++value; });
  }
}

}
