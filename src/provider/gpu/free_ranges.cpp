#include <arisa/provider/gpu/free_ranges.hpp>

#include <algorithm>
#include <iterator>

namespace arisa::provider::gpu {

FreeRanges::FreeRanges(std::uint64_t base, std::uint64_t capacity)
    : base_(base), capacity_(capacity) {
  if (capacity > 0) {
    free_.push_back(ByteRange{.offset = 0, .size = capacity});
  }
}

auto FreeRanges::take(std::uint64_t size, Alignment alignment)
    -> std::optional<ByteRange> {
  if (size == 0) {
    return std::nullopt;
  }
  for (auto at = free_.begin(); at != free_.end(); ++at) {
    const std::uint64_t start = alignment.up(base_ + at->offset) - base_;
    const std::uint64_t limit = at->offset + at->size;
    if (start > limit || limit - start < size) {
      continue;
    }
    const ByteRange before{.offset = at->offset, .size = start - at->offset};
    const ByteRange after{.offset = start + size, .size = limit - start - size};
    at = free_.erase(at);
    if (after.size > 0) {
      at = free_.insert(at, after);
    }
    if (before.size > 0) {
      free_.insert(at, before);
    }
    return ByteRange{.offset = start, .size = size};
  }
  return std::nullopt;
}

void FreeRanges::give(ByteRange range) {
  if (range.size == 0) {
    return;
  }
  auto at = free_.insert(
      std::ranges::lower_bound(free_, range.offset, {}, &ByteRange::offset),
      range);
  if (const auto next = std::next(at);
      next != free_.end() && at->offset + at->size == next->offset) {
    at->size += next->size;
    free_.erase(next);
  }
  if (at != free_.begin()) {
    if (const auto previous = std::prev(at);
        previous->offset + previous->size == at->offset) {
      previous->size += at->size;
      free_.erase(at);
    }
  }
}

auto FreeRanges::unused() const noexcept -> bool {
  return capacity_ == 0 ||
         (free_.size() == 1 && free_.front().size == capacity_);
}

auto FreeRanges::largest() const noexcept -> std::uint64_t {
  return free_.empty() ? 0 : std::ranges::max(free_, {}, &ByteRange::size).size;
}

}
