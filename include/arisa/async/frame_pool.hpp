#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace arisa::async {

class FramePool {
 public:
  struct Statistics {
    std::size_t live_frames{0};
    std::size_t live_bytes{0};
    std::uint64_t frames{0};
    std::uint64_t upstream_allocations{0};
    std::size_t smallest_frame{0};
    std::size_t largest_frame{0};
  };

  static auto shared() noexcept -> FramePool&;

  FramePool();

  FramePool(const FramePool&) = delete;
  auto operator=(const FramePool&) -> FramePool& = delete;
  FramePool(FramePool&&) = delete;
  auto operator=(FramePool&&) -> FramePool& = delete;
  ~FramePool();

  auto allocate(std::size_t bytes) -> void*;

  void deallocate(void* frame, std::size_t bytes) noexcept;

  void limit(std::optional<std::size_t> bytes) noexcept;

  auto statistics() const noexcept -> Statistics;

 private:
  class Resources;

  [[noreturn]] static void exhausted(std::size_t bytes,
                                     std::size_t live) noexcept;

  void record_size(std::size_t bytes) noexcept;

  std::unique_ptr<Resources> resources_;
  std::atomic<std::size_t> live_frames_{0};
  std::atomic<std::size_t> live_bytes_{0};
  std::atomic<std::uint64_t> frames_{0};
  std::atomic<std::size_t> smallest_{0};
  std::atomic<std::size_t> largest_{0};
  std::atomic<std::size_t> limit_{0};
};

}
