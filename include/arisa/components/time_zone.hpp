#pragma once

#include <chrono>
#include <string>
#include <utility>

namespace arisa::components {

class TimeZone {
 public:
  static auto gmt() noexcept -> TimeZone { return TimeZone{}; }

  static auto seconds_from_gmt(std::chrono::seconds offset) noexcept
      -> TimeZone {
    return TimeZone{offset};
  }

  static auto named(std::string name, std::chrono::seconds offset) -> TimeZone {
    TimeZone zone{offset};
    zone.name_ = std::move(name);
    return zone;
  }

  auto offset() const noexcept -> std::chrono::seconds { return offset_; }

  auto identifier() const -> std::string;

  friend auto operator==(const TimeZone&, const TimeZone&) -> bool = default;

 private:
  TimeZone() noexcept = default;

  explicit TimeZone(std::chrono::seconds offset) noexcept : offset_(offset) {}

  std::chrono::seconds offset_{0};
  std::string name_;
};

}
