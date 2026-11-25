#pragma once

#include <arisa/composition/text_selection_affinity.hpp>

#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace arisa::composition {

class TextSelection {
 public:
  struct Range {
    std::size_t lower{0};
    std::size_t upper{0};

    constexpr auto empty() const noexcept -> bool { return lower == upper; }

    constexpr auto length() const noexcept -> std::size_t {
      return upper - lower;
    }

    constexpr auto clamped(std::size_t size) const noexcept -> Range {
      const std::size_t low = std::min(lower, size);
      return Range{.lower = low, .upper = std::clamp(upper, low, size)};
    }

    friend constexpr auto operator==(const Range&, const Range&)
        -> bool = default;
  };

  struct InsertionPoint {
    std::size_t index{0};

    friend constexpr auto operator==(const InsertionPoint&,
                                     const InsertionPoint&) -> bool = default;
  };

  class Indices {
   public:
    static auto selection(Range range) -> Indices {
      return Indices{std::vector<Range>{range}, false};
    }

    static auto multi_selection(std::vector<Range> ranges) -> Indices {
      std::ranges::sort(ranges, {}, &Range::lower);
      return Indices{std::move(ranges), true};
    }

    auto is_multi_selection() const noexcept -> bool { return multi_; }

    auto ranges() const noexcept -> std::span<const Range> { return ranges_; }

    auto range() const noexcept -> Range {
      if (ranges_.empty()) {
        return Range{};
      }
      return Range{.lower = ranges_.front().lower,
                   .upper = std::ranges::max(ranges_, {}, &Range::upper).upper};
    }

    friend auto operator==(const Indices&, const Indices&) -> bool = default;

   private:
    Indices(std::vector<Range> ranges, bool multi) noexcept
        : ranges_(std::move(ranges)), multi_(multi) {}

    std::vector<Range> ranges_;
    bool multi_{false};
  };

  TextSelection() : TextSelection(InsertionPoint{}) {}

  explicit TextSelection(Range range) : indices_(Indices::selection(range)) {}

  explicit TextSelection(std::vector<Range> ranges)
      : indices_(Indices::multi_selection(std::move(ranges))) {}

  explicit TextSelection(InsertionPoint point)
      : indices_(Indices::selection(
            Range{.lower = point.index, .upper = point.index})) {}

  auto indices() const noexcept -> const Indices& { return indices_; }

  void set_indices(Indices indices) { indices_ = std::move(indices); }

  auto affinity() const noexcept -> TextSelectionAffinity { return affinity_; }

  auto with_affinity(TextSelectionAffinity affinity) const -> TextSelection {
    TextSelection changed = *this;
    changed.affinity_ = affinity;
    return changed;
  }

  auto is_insertion() const noexcept -> bool {
    return !indices_.is_multi_selection() && indices_.range().empty();
  }

  auto range() const noexcept -> Range { return indices_.range(); }

  friend auto operator==(const TextSelection&, const TextSelection&)
      -> bool = default;

 private:
  Indices indices_;
  TextSelectionAffinity affinity_{TextSelectionAffinity::automatic};
};

}
