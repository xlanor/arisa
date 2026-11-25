#pragma once

#include <string>

#include <arisa/provider/clock/provider.hpp>

namespace arisa::provider::standard {

class StandardClock final : public ClockProvider {
 public:
  auto now() const noexcept -> Instant override;

  auto wall_now() const noexcept
      -> std::chrono::system_clock::time_point override;

  [[nodiscard]] auto local_offset() const
      -> std::expected<std::chrono::seconds, ProviderError> override;

  [[nodiscard]] auto zone_name() const
      -> std::expected<std::string, ProviderError> override;
};

}
