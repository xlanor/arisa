#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>

namespace arisa::support {

template <class T>
class TripleBuffer {
 public:
  auto back() noexcept -> T& { return slots_[back_]; }

  void publish() noexcept {
    const std::uint8_t previous = middle_.exchange(
        static_cast<std::uint8_t>(back_ | fresh), std::memory_order_acq_rel);
    back_ = static_cast<std::uint8_t>(previous & index);
  }

  auto latest() noexcept -> std::optional<std::reference_wrapper<const T>> {
    if ((middle_.load(std::memory_order_acquire) & fresh) != 0) {
      const std::uint8_t previous =
          middle_.exchange(front_, std::memory_order_acq_rel);
      front_ = static_cast<std::uint8_t>(previous & index);
      seen_ = true;
    }
    if (!seen_) {
      return std::nullopt;
    }
    return std::cref(slots_[front_]);
  }

 private:
  static constexpr std::uint8_t fresh = 0x4U;
  static constexpr std::uint8_t index = 0x3U;

  std::array<T, 3> slots_{};
  std::uint8_t back_{0};
  std::atomic<std::uint8_t> middle_{1};
  std::uint8_t front_{2};
  bool seen_{false};
};

}
