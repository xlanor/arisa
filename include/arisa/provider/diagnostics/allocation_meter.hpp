#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace arisa::provider {

class AllocationMeter {
 public:
  static auto count() noexcept -> std::optional<std::uint64_t>;

  static auto live() noexcept -> std::optional<std::int64_t>;

  static auto bytes() noexcept -> std::optional<std::uint64_t>;

  static auto counting() noexcept -> bool;

 private:
  static auto allocate(std::size_t size, std::size_t alignment) noexcept
      -> void*;

  static void release(void* pointer) noexcept;

  static std::atomic<std::uint64_t> allocations_;

  static std::atomic<std::uint64_t> releases_;

  static std::atomic<std::uint64_t> bytes_;

  friend auto ::operator new(std::size_t size) -> void*;
  friend auto ::operator new[](std::size_t size) -> void*;
  friend auto ::operator new(std::size_t size, std::align_val_t alignment)
      -> void*;
  friend auto ::operator new[](std::size_t size, std::align_val_t alignment)
      -> void*;
  friend auto ::operator new(std::size_t size, const std::nothrow_t&) noexcept
      -> void*;
  friend auto ::operator new[](std::size_t size, const std::nothrow_t&) noexcept
      -> void*;
  friend void ::operator delete(void* pointer) noexcept;
  friend void ::operator delete[](void* pointer) noexcept;
  friend void ::operator delete(void* pointer, std::size_t size) noexcept;
  friend void ::operator delete[](void* pointer, std::size_t size) noexcept;
  friend void ::operator delete(void* pointer,
                                std::align_val_t alignment) noexcept;
  friend void ::operator delete[](void* pointer,
                                  std::align_val_t alignment) noexcept;
  friend void ::operator delete(void* pointer, std::size_t size,
                                std::align_val_t alignment) noexcept;
  friend void ::operator delete[](void* pointer, std::size_t size,
                                  std::align_val_t alignment) noexcept;
};

}
