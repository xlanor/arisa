#include <arisa/async/frame_pool.hpp>

#include <cstdio>
#include <exception>
#include <limits>
#include <memory_resource>
#include <print>

namespace arisa::async {

class FramePool::Resources {
 public:
  Resources()
      : pool_(std::pmr::pool_options{.max_blocks_per_chunk = 64,
                                     .largest_required_pool_block = 4096},
              &upstream_) {}

  auto pool() noexcept -> std::pmr::synchronized_pool_resource& {
    return pool_;
  }

  auto upstream_allocations() const noexcept -> std::uint64_t {
    return upstream_.allocations();
  }

 private:
  class Upstream final : public std::pmr::memory_resource {
   public:
    auto allocations() const noexcept -> std::uint64_t {
      return allocations_.load(std::memory_order_relaxed);
    }

   private:
    auto do_allocate(std::size_t bytes, std::size_t alignment)
        -> void* override {
      allocations_.fetch_add(1, std::memory_order_relaxed);
      return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }

    void do_deallocate(void* block, std::size_t bytes,
                       std::size_t alignment) override {
      std::pmr::new_delete_resource()->deallocate(block, bytes, alignment);
    }

    auto do_is_equal(const std::pmr::memory_resource& other) const noexcept
        -> bool override {
      return this == &other;
    }

    std::atomic<std::uint64_t> allocations_{0};
  };

  Upstream upstream_;
  std::pmr::synchronized_pool_resource pool_;
};

auto FramePool::shared() noexcept -> FramePool& {
  static FramePool pool;
  return pool;
}

FramePool::FramePool()
    : resources_(std::make_unique<Resources>()),
      limit_(std::numeric_limits<std::size_t>::max()) {}

FramePool::~FramePool() = default;

auto FramePool::allocate(std::size_t bytes) -> void* {
  const std::size_t live =
      live_bytes_.fetch_add(bytes, std::memory_order_relaxed) + bytes;
  if (live > limit_.load(std::memory_order_relaxed)) {
    exhausted(bytes, live - bytes);
  }
  live_frames_.fetch_add(1, std::memory_order_relaxed);
  frames_.fetch_add(1, std::memory_order_relaxed);
  record_size(bytes);
  return resources_->pool().allocate(bytes, alignof(std::max_align_t));
}

void FramePool::deallocate(void* frame, std::size_t bytes) noexcept {
  resources_->pool().deallocate(frame, bytes, alignof(std::max_align_t));
  live_frames_.fetch_sub(1, std::memory_order_relaxed);
  live_bytes_.fetch_sub(bytes, std::memory_order_relaxed);
}

void FramePool::limit(std::optional<std::size_t> bytes) noexcept {
  limit_.store(bytes.value_or(std::numeric_limits<std::size_t>::max()),
               std::memory_order_relaxed);
}

auto FramePool::statistics() const noexcept -> Statistics {
  return Statistics{.live_frames = live_frames_.load(std::memory_order_relaxed),
                    .live_bytes = live_bytes_.load(std::memory_order_relaxed),
                    .frames = frames_.load(std::memory_order_relaxed),
                    .upstream_allocations = resources_->upstream_allocations(),
                    .smallest_frame = smallest_.load(std::memory_order_relaxed),
                    .largest_frame = largest_.load(std::memory_order_relaxed)};
}

void FramePool::exhausted(std::size_t bytes, std::size_t live) noexcept {
  std::println(stderr,
               "arisa: coroutine frame pool exhausted ({} bytes requested, {} "
               "bytes live)",
               bytes, live);
  std::terminate();
}

void FramePool::record_size(std::size_t bytes) noexcept {
  std::size_t smallest = smallest_.load(std::memory_order_relaxed);
  while ((smallest == 0 || bytes < smallest) &&
         !smallest_.compare_exchange_weak(smallest, bytes,
                                          std::memory_order_relaxed)) {
  }
  std::size_t largest = largest_.load(std::memory_order_relaxed);
  while (bytes > largest && !largest_.compare_exchange_weak(
                                largest, bytes, std::memory_order_relaxed)) {
  }
}

}
