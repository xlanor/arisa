#pragma once

#include <arisa/provider/gpu/alignment.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace arisa::provider::gpu {

struct ByteRange {
  std::uint64_t offset{0};
  std::uint64_t size{0};

  friend constexpr auto operator==(const ByteRange&, const ByteRange&)
      -> bool = default;
};

class FreeRanges {
 public:
  FreeRanges(std::uint64_t base, std::uint64_t capacity);

  [[nodiscard]] auto take(std::uint64_t size, Alignment alignment)
      -> std::optional<ByteRange>;

  void give(ByteRange range);

  auto capacity() const noexcept -> std::uint64_t { return capacity_; }

  auto unused() const noexcept -> bool;

  auto largest() const noexcept -> std::uint64_t;

  auto fragments() const noexcept -> std::size_t { return free_.size(); }

 private:
  std::uint64_t base_;
  std::uint64_t capacity_;
  std::vector<ByteRange> free_;
};

}
