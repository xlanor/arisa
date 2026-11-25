#include <arisa/provider/headless/manual_clock.hpp>

namespace arisa::provider::headless {

ManualClock::ManualClock(std::chrono::system_clock::time_point wall) noexcept
    : wall_ticks_(wall.time_since_epoch().count()) {}

auto ManualClock::now() const noexcept -> Instant {
  return Instant{std::chrono::nanoseconds{monotonic_nanoseconds_.load()}};
}

auto ManualClock::wall_now() const noexcept
    -> std::chrono::system_clock::time_point {
  return std::chrono::system_clock::time_point{
      std::chrono::system_clock::duration{wall_ticks_.load()}};
}

auto ManualClock::advance(std::chrono::nanoseconds step)
    -> std::expected<void, ProviderError> {
  if (step < std::chrono::nanoseconds::zero()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  monotonic_nanoseconds_.fetch_add(step.count());
  wall_ticks_.fetch_add(
      std::chrono::duration_cast<std::chrono::system_clock::duration>(step)
          .count());
  return {};
}

auto ManualClock::local_offset() const
    -> std::expected<std::chrono::seconds, ProviderError> {
  return std::chrono::seconds{offset_seconds_.load()};
}

void ManualClock::set_local_offset(std::chrono::seconds offset) noexcept {
  offset_seconds_.store(offset.count());
}

void ManualClock::set_wall(
    std::chrono::system_clock::time_point wall) noexcept {
  wall_ticks_.store(wall.time_since_epoch().count());
}

}
