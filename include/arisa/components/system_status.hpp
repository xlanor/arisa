#pragma once

#include <arisa/provider/clock/provider.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/system_info/provider.hpp>

#include <chrono>
#include <expected>
#include <functional>
#include <optional>

namespace arisa::components {

class SystemStatus {
 public:
  using LocalTime = std::chrono::local_seconds;

  SystemStatus() = default;

  SystemStatus(const provider::SystemInfoProvider& info,
               const provider::ClockProvider& clock) noexcept
      : info_(info), clock_(clock) {}

  [[nodiscard]] auto battery() const
      -> std::expected<provider::BatteryState, provider::ProviderError>;

  [[nodiscard]] auto network() const
      -> std::expected<provider::NetworkState, provider::ProviderError>;

  auto local_time() const -> std::optional<LocalTime>;

  auto wall_time() const
      -> std::optional<std::chrono::system_clock::time_point>;

  friend auto operator==(const SystemStatus& left, const SystemStatus& right)
      -> bool;

 private:
  std::optional<std::reference_wrapper<const provider::SystemInfoProvider>>
      info_;
  std::optional<std::reference_wrapper<const provider::ClockProvider>> clock_;
};

}
