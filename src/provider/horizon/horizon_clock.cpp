#include <arisa/provider/horizon/horizon_clock.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace arisa::provider::horizon {

HorizonClock::HorizonClock(ServiceSession<TimeService> time) noexcept
    : time_(std::move(time)) {}

auto HorizonClock::create() -> std::expected<HorizonClock, ProviderError> {
  return ServiceSession<TimeService>::create().transform(
      [](ServiceSession<TimeService> time) {
        return HorizonClock{std::move(time)};
      });
}

auto HorizonClock::now() const noexcept -> Instant {
  return Instant{
      std::chrono::nanoseconds{static_cast<std::chrono::nanoseconds::rep>(
          armTicksToNs(armGetSystemTick()))}};
}

auto HorizonClock::wall_now() const noexcept
    -> std::chrono::system_clock::time_point {
  u64 seconds = 0;
  if (R_FAILED(timeGetCurrentTime(TimeType_Default, &seconds))) {
    return std::chrono::system_clock::now();
  }
  const auto monotonic =
      static_cast<std::int64_t>(armTicksToNs(armGetSystemTick()));
  const std::int64_t reported =
      static_cast<std::int64_t>(seconds) * 1'000'000'000;
  WallAnchor& anchor = *anchor_;
  if (!anchor.aligned.load(std::memory_order_acquire)) {
    const std::uint64_t previous =
        anchor.seen.exchange(seconds, std::memory_order_acq_rel);
    if (previous != 0 && previous != seconds) {
      anchor.offset.store(reported - monotonic, std::memory_order_release);
      anchor.aligned.store(true, std::memory_order_release);
    } else if (previous == 0) {
      anchor.offset.store(reported - monotonic, std::memory_order_release);
    }
  }
  std::int64_t estimate =
      anchor.offset.load(std::memory_order_acquire) + monotonic;
  if (estimate - reported > resync_limit || reported - estimate > resync_limit) {
    anchor.offset.store(reported - monotonic, std::memory_order_release);
    estimate = reported;
  }
  return std::chrono::system_clock::time_point{
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::nanoseconds{estimate})};
}

auto HorizonClock::local_offset() const
    -> std::expected<std::chrono::seconds, ProviderError> {
  u64 seconds = 0;
  const Result current = timeGetCurrentTime(TimeType_Default, &seconds);
  if (R_FAILED(current)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, current});
  }
  TimeCalendarTime calendar{};
  TimeCalendarAdditionalInfo info{};
  const Result converted =
      timeToCalendarTimeWithMyRule(seconds, &calendar, &info);
  if (R_FAILED(converted)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, converted});
  }
  return std::chrono::seconds{info.offset};
}

auto HorizonClock::zone_name() const
    -> std::expected<std::string, ProviderError> {
  TimeLocationName name{};
  const Result fetched = timeGetDeviceLocationName(&name);
  if (R_FAILED(fetched)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, fetched});
  }
  const std::string_view text{
      name.name, std::char_traits<char>::length(name.name) < sizeof(name.name)
                     ? std::char_traits<char>::length(name.name)
                     : sizeof(name.name)};
  if (text.empty()) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  return std::string{text};
}

}
