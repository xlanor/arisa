#include <arisa/provider/diagnostics/allocation_meter.hpp>

#if defined(__SANITIZE_ADDRESS__)
#define ARISA_ALLOCATION_METER 0
#elif defined(__APPLE__)
#define ARISA_ALLOCATION_METER 1
#include <malloc/malloc.h>
#elif defined(__SWITCH__)
#define ARISA_ALLOCATION_METER 1
#include <malloc.h>
#include <reent.h>
#elif defined(__GLIBC__)
#define ARISA_ALLOCATION_METER 1
extern "C" auto __libc_memalign(std::size_t alignment, std::size_t size)
    -> void*;
extern "C" void __libc_free(void* pointer);
#else
#define ARISA_ALLOCATION_METER 0
#endif

#include <cstdlib>

namespace arisa::provider {

std::atomic<std::uint64_t> AllocationMeter::allocations_{0};

std::atomic<std::uint64_t> AllocationMeter::releases_{0};

std::atomic<std::uint64_t> AllocationMeter::bytes_{0};

auto AllocationMeter::counting() noexcept -> bool {
  return ARISA_ALLOCATION_METER != 0;
}

auto AllocationMeter::count() noexcept -> std::optional<std::uint64_t> {
  if (!counting()) {
    return std::nullopt;
  }
  return allocations_.load(std::memory_order_relaxed);
}

auto AllocationMeter::live() noexcept -> std::optional<std::int64_t> {
  if (!counting()) {
    return std::nullopt;
  }
  const std::uint64_t released = releases_.load(std::memory_order_relaxed);
  const std::uint64_t allocated = allocations_.load(std::memory_order_relaxed);
  return static_cast<std::int64_t>(allocated - released);
}

auto AllocationMeter::bytes() noexcept -> std::optional<std::uint64_t> {
  if (!counting()) {
    return std::nullopt;
  }
  return bytes_.load(std::memory_order_relaxed);
}

#if ARISA_ALLOCATION_METER

auto AllocationMeter::allocate(std::size_t size, std::size_t alignment) noexcept
    -> void* {
  allocations_.fetch_add(1, std::memory_order_relaxed);
  bytes_.fetch_add(size, std::memory_order_relaxed);
  const std::size_t wanted = size == 0 ? 1 : size;
  const std::size_t aligned = alignment < alignof(std::max_align_t)
                                  ? alignof(std::max_align_t)
                                  : alignment;
#if defined(__APPLE__)
  malloc_zone_t* zone = malloc_default_zone();
  if (aligned == alignof(std::max_align_t)) {
    return malloc_zone_malloc(zone, wanted);
  }
  return malloc_zone_memalign(zone, aligned, wanted);
#elif defined(__SWITCH__)
  return _memalign_r(_REENT, aligned, wanted);
#else
  return __libc_memalign(aligned, wanted);
#endif
}

void AllocationMeter::release(void* pointer) noexcept {
  if (pointer == nullptr) {
    return;
  }
  releases_.fetch_add(1, std::memory_order_relaxed);
#if defined(__APPLE__)
  malloc_zone_free(malloc_default_zone(), pointer);
#elif defined(__SWITCH__)
  _free_r(_REENT, pointer);
#else
  __libc_free(pointer);
#endif
}

#endif

}

#if ARISA_ALLOCATION_METER

auto operator new(std::size_t size) -> void* {
  void* pointer = arisa::provider::AllocationMeter::allocate(
      size, alignof(std::max_align_t));
  if (pointer == nullptr) {
    std::abort();
  }
  return pointer;
}

auto operator new[](std::size_t size) -> void* {
  void* pointer = arisa::provider::AllocationMeter::allocate(
      size, alignof(std::max_align_t));
  if (pointer == nullptr) {
    std::abort();
  }
  return pointer;
}

auto operator new(std::size_t size, std::align_val_t alignment) -> void* {
  void* pointer = arisa::provider::AllocationMeter::allocate(
      size, static_cast<std::size_t>(alignment));
  if (pointer == nullptr) {
    std::abort();
  }
  return pointer;
}

auto operator new[](std::size_t size, std::align_val_t alignment) -> void* {
  void* pointer = arisa::provider::AllocationMeter::allocate(
      size, static_cast<std::size_t>(alignment));
  if (pointer == nullptr) {
    std::abort();
  }
  return pointer;
}

auto operator new(std::size_t size, const std::nothrow_t&) noexcept -> void* {
  return arisa::provider::AllocationMeter::allocate(size,
                                                    alignof(std::max_align_t));
}

auto operator new[](std::size_t size, const std::nothrow_t&) noexcept -> void* {
  return arisa::provider::AllocationMeter::allocate(size,
                                                    alignof(std::max_align_t));
}

void operator delete(void* pointer) noexcept {
  arisa::provider::AllocationMeter::release(pointer);
}

void operator delete[](void* pointer) noexcept {
  arisa::provider::AllocationMeter::release(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
  arisa::provider::AllocationMeter::release(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
  arisa::provider::AllocationMeter::release(pointer);
}

void operator delete(void* pointer, std::align_val_t) noexcept {
  arisa::provider::AllocationMeter::release(pointer);
}

void operator delete[](void* pointer, std::align_val_t) noexcept {
  arisa::provider::AllocationMeter::release(pointer);
}

void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
  arisa::provider::AllocationMeter::release(pointer);
}

void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
  arisa::provider::AllocationMeter::release(pointer);
}

#endif
