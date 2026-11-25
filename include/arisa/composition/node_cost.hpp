#pragma once

#include <cstdint>

namespace arisa::composition {

class NodeCost {
 public:
  struct Counts {
    std::uint64_t evaluations{0};
    std::uint64_t layouts{0};
    std::uint64_t records{0};
    std::uint64_t shaping{0};

    constexpr auto total() const noexcept -> std::uint64_t {
      return evaluations + layouts + records + shaping;
    }

    friend constexpr auto operator-(const Counts& left,
                                    const Counts& right) noexcept -> Counts {
      return Counts{.evaluations = left.evaluations - right.evaluations,
                    .layouts = left.layouts - right.layouts,
                    .records = left.records - right.records,
                    .shaping = left.shaping - right.shaping};
    }

    friend constexpr auto operator+(const Counts& left,
                                    const Counts& right) noexcept -> Counts {
      return Counts{.evaluations = left.evaluations + right.evaluations,
                    .layouts = left.layouts + right.layouts,
                    .records = left.records + right.records,
                    .shaping = left.shaping + right.shaping};
    }

    friend constexpr auto operator==(const Counts&, const Counts&)
        -> bool = default;
  };

  constexpr auto counts() const noexcept -> Counts { return counts_; }

  constexpr void evaluated() const noexcept { ++counts_.evaluations; }

  constexpr void laid_out() const noexcept { ++counts_.layouts; }

  constexpr void recorded() const noexcept { ++counts_.records; }

  constexpr void shaped(std::uint64_t runs) const noexcept {
    counts_.shaping += runs;
  }

 private:
  mutable Counts counts_{};
};

}
