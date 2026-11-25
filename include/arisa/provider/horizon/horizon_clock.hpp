#pragma once

#include <string>

#include <arisa/provider/clock/provider.hpp>
#include <arisa/provider/horizon/service_session.hpp>

#include <switch.h>

#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>

namespace arisa::provider::horizon {

class HorizonClock final : public ClockProvider {
 public:
  [[nodiscard]] static auto create()
      -> std::expected<HorizonClock, ProviderError>;

  auto now() const noexcept -> Instant override;

  auto wall_now() const noexcept
      -> std::chrono::system_clock::time_point override;

  [[nodiscard]] auto local_offset() const
      -> std::expected<std::chrono::seconds, ProviderError> override;

  [[nodiscard]] auto zone_name() const
      -> std::expected<std::string, ProviderError> override;

 private:
  struct WallAnchor {
    std::atomic<std::int64_t> offset{0};
    std::atomic<std::uint64_t> seen{0};
    std::atomic<bool> aligned{false};
  };

  static constexpr std::int64_t resync_limit = 2'000'000'000;

  struct TimeService {
    static auto open() noexcept -> Result { return timeInitialize(); }

    static void close() noexcept { timeExit(); }
  };

  explicit HorizonClock(ServiceSession<TimeService> time) noexcept;

  ServiceSession<TimeService> time_;
  std::unique_ptr<WallAnchor> anchor_ = std::make_unique<WallAnchor>();
};

}
