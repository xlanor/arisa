#include <arisa/provider/diagnostics/heap_meter.hpp>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__SWITCH2__)
extern "C" {
#include <switch2/memory/heap.h>
}
#elif defined(__SWITCH__) || defined(__linux__)
#include <malloc.h>
#endif

namespace arisa::provider {

auto HeapMeter::sample() noexcept -> std::optional<HeapUsage> {
#if defined(__APPLE__)
  malloc_statistics_t statistics{};
  malloc_zone_statistics(nullptr, &statistics);
  return HeapUsage{.blocks = statistics.blocks_in_use,
                   .bytes = statistics.size_in_use};
#elif defined(__SWITCH2__)
  return from_capacity(heapGetTotalSize(), heapGetFreeSize());
#elif defined(__SWITCH__)
  const struct mallinfo info = mallinfo();
  return HeapUsage{.blocks = std::nullopt,
                   .bytes = static_cast<std::uint64_t>(info.uordblks)};
#elif defined(__linux__)
  const struct mallinfo2 info = mallinfo2();
  return HeapUsage{.blocks = std::nullopt,
                   .bytes = static_cast<std::uint64_t>(info.uordblks)};
#else
  return std::nullopt;
#endif
}

auto HeapMeter::from_capacity(std::uint64_t total,
                              std::uint64_t unused) noexcept -> HeapUsage {
  return HeapUsage{.blocks = std::nullopt,
                   .bytes = total > unused ? total - unused : 0};
}

}
