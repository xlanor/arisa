#include <capture_clock.hpp>

namespace arisa::examples {

auto CaptureClock::now() const noexcept -> provider::Instant {
  return monotonic_.now();
}

auto CaptureClock::wall_now() const noexcept
    -> std::chrono::system_clock::time_point {
  return fixed_wall;
}

auto CaptureClock::local_offset() const
    -> std::expected<std::chrono::seconds, provider::ProviderError> {
  return std::chrono::seconds{0};
}

}
