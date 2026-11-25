#pragma once

#include <arisa/provider/clock/provider.hpp>
#include <arisa/provider/standard/standard_clock.hpp>

#include <chrono>
#include <expected>

namespace arisa::examples {

class CaptureClock final : public provider::ClockProvider {
 public:
  static constexpr std::chrono::sys_seconds fixed_wall{
      std::chrono::sys_days{std::chrono::year{2026} / std::chrono::January /
                            1} +
      std::chrono::hours{9} + std::chrono::minutes{41}};

  auto now() const noexcept -> provider::Instant override;

  auto wall_now() const noexcept
      -> std::chrono::system_clock::time_point override;

  [[nodiscard]] auto local_offset() const
      -> std::expected<std::chrono::seconds, provider::ProviderError> override;

 private:
  provider::standard::StandardClock monotonic_;
};

}
