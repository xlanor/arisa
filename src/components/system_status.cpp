#include <arisa/components/system_status.hpp>

namespace arisa::components {

auto SystemStatus::battery() const
    -> std::expected<provider::BatteryState, provider::ProviderError> {
  if (!info_.has_value()) {
    return std::unexpected(provider::ProviderError::unsupported_capability());
  }
  return info_->get().battery();
}

auto SystemStatus::network() const
    -> std::expected<provider::NetworkState, provider::ProviderError> {
  if (!info_.has_value()) {
    return std::unexpected(provider::ProviderError::unsupported_capability());
  }
  return info_->get().network();
}

auto SystemStatus::local_time() const -> std::optional<LocalTime> {
  if (!clock_.has_value()) {
    return std::nullopt;
  }
  const provider::ClockProvider& clock = clock_->get();
  const auto wall = std::chrono::floor<std::chrono::seconds>(clock.wall_now());
  const std::chrono::seconds offset =
      clock.local_offset().value_or(std::chrono::seconds{0});
  return LocalTime{wall.time_since_epoch() + offset};
}

auto SystemStatus::wall_time() const
    -> std::optional<std::chrono::system_clock::time_point> {
  if (!clock_.has_value()) {
    return std::nullopt;
  }
  return clock_->get().wall_now();
}

auto operator==(const SystemStatus& left, const SystemStatus& right) -> bool {
  const auto same = [](const auto& one, const auto& other) {
    if (!one.has_value() || !other.has_value()) {
      return one.has_value() == other.has_value();
    }
    return &one->get() == &other->get();
  };
  return same(left.info_, right.info_) && same(left.clock_, right.clock_);
}

}
