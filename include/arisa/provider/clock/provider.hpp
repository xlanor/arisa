#pragma once

#include <arisa/provider/clock/instant.hpp>
#include <arisa/provider/error.hpp>

#include <chrono>
#include <expected>
#include <string>

namespace arisa::provider {

class ClockProvider {
 public:
  virtual ~ClockProvider() = default;

  virtual auto now() const noexcept -> Instant = 0;

  virtual auto wall_now() const noexcept
      -> std::chrono::system_clock::time_point = 0;

  [[nodiscard]] virtual auto local_offset() const
      -> std::expected<std::chrono::seconds, ProviderError> = 0;

  virtual auto stepped() const noexcept -> bool { return false; }

  [[nodiscard]] virtual auto zone_name() const
      -> std::expected<std::string, ProviderError> {
    return std::unexpected(ProviderError::unsupported_capability());
  }
};

}
