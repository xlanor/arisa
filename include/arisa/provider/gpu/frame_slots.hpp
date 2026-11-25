#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>

namespace arisa::provider {

class FrameSlots {
 public:
  struct Claim {
    std::uint32_t slot{0};
    std::uint64_t frame{0};
  };

  explicit FrameSlots(std::uint32_t count) noexcept;

  FrameSlots(const FrameSlots&) = delete;
  auto operator=(const FrameSlots&) -> FrameSlots& = delete;
  FrameSlots(FrameSlots&&) = delete;
  auto operator=(FrameSlots&&) -> FrameSlots& = delete;
  ~FrameSlots() = default;

  auto count() const noexcept -> std::uint32_t { return count_; }

  [[nodiscard]] auto claim() -> std::optional<Claim>;

  [[nodiscard]] auto await() -> std::optional<std::uint32_t>;

  void interrupt() noexcept;

  auto next() const -> Claim;

  void abandon() noexcept;

  void seal() noexcept;

  void release(std::uint64_t frame) noexcept;

  auto claimed() const -> bool;

 private:
  std::uint32_t count_;
  mutable std::mutex mutex_;
  std::condition_variable ready_;
  std::uint64_t built_{0};
  std::uint64_t allowed_{1};
  std::uint64_t interrupts_{0};
  bool claimed_{false};
};

}
