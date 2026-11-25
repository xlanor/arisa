#pragma once

#include <arisa/provider/clock/provider.hpp>
#include <arisa/provider/error.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>

namespace arisa::provider::headless {

class ManualClock final : public ClockProvider {
 public:
  static constexpr std::chrono::sys_days default_wall{std::chrono::year{2026} /
                                                      std::chrono::January / 1};

  explicit ManualClock(
      std::chrono::system_clock::time_point wall = default_wall) noexcept;

  auto now() const noexcept -> Instant override;

  auto wall_now() const noexcept
      -> std::chrono::system_clock::time_point override;

  [[nodiscard]] auto local_offset() const
      -> std::expected<std::chrono::seconds, ProviderError> override;

  auto stepped() const noexcept -> bool override { return true; }

  [[nodiscard]] auto advance(std::chrono::nanoseconds step)
      -> std::expected<void, ProviderError>;

  void set_wall(std::chrono::system_clock::time_point wall) noexcept;

  void set_local_offset(std::chrono::seconds offset) noexcept;

 private:
  std::atomic<std::int64_t> offset_seconds_{0};
  std::atomic<std::int64_t> monotonic_nanoseconds_{0};
  std::atomic<std::chrono::system_clock::rep> wall_ticks_;
};

}
