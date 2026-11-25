#pragma once

#include <chrono>
#include <compare>

namespace arisa::provider {

class Instant {
 public:
  constexpr explicit Instant(std::chrono::nanoseconds since_origin) noexcept
      : since_origin_(since_origin) {}

  constexpr auto since_origin() const noexcept -> std::chrono::nanoseconds {
    return since_origin_;
  }

  friend constexpr auto operator==(Instant, Instant) -> bool = default;
  friend constexpr auto operator<=>(Instant, Instant) = default;

  friend constexpr auto operator-(Instant later, Instant earlier) noexcept
      -> std::chrono::nanoseconds {
    return later.since_origin_ - earlier.since_origin_;
  }

  friend constexpr auto operator+(Instant instant,
                                  std::chrono::nanoseconds offset) noexcept
      -> Instant {
    return Instant{instant.since_origin_ + offset};
  }

  friend constexpr auto operator-(Instant instant,
                                  std::chrono::nanoseconds offset) noexcept
      -> Instant {
    return Instant{instant.since_origin_ - offset};
  }

 private:
  std::chrono::nanoseconds since_origin_;
};

}
