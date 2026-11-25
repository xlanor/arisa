#pragma once

#include <atomic>
#include <cstddef>

namespace arisa::support {

class SparseRecycling {
 public:
  class Suspension {
   public:
    Suspension() noexcept {
      suspensions_.fetch_add(1, std::memory_order_relaxed);
    }

    Suspension(const Suspension&) = delete;
    auto operator=(const Suspension&) -> Suspension& = delete;
    Suspension(Suspension&&) = delete;
    auto operator=(Suspension&&) -> Suspension& = delete;

    ~Suspension() { suspensions_.fetch_sub(1, std::memory_order_relaxed); }
  };

  static auto active() noexcept -> bool {
    return !sanitized && suspensions_.load(std::memory_order_relaxed) == 0;
  }

 private:
#if defined(__SANITIZE_ADDRESS__)
  static constexpr bool sanitized = true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
  static constexpr bool sanitized = true;
#else
  static constexpr bool sanitized = false;
#endif
#else
  static constexpr bool sanitized = false;
#endif

  static constinit inline std::atomic<std::size_t> suspensions_{0};
};

}
