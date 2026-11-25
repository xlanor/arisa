#pragma once

#include <arisa/components/subview.hpp>
#include <arisa/composition/element.hpp>

#include <algorithm>
#include <cstddef>
#include <span>

namespace arisa::components {

class SubviewsCollectionSlice {
 public:
  explicit SubviewsCollectionSlice(std::span<Subview> subviews) noexcept
      : subviews_(subviews) {}

  auto size() const noexcept -> std::size_t { return subviews_.size(); }

  auto empty() const noexcept -> bool { return subviews_.empty(); }

  auto operator[](std::size_t index) const -> Subview& {
    return subviews_[index];
  }

  auto begin() const noexcept { return subviews_.begin(); }

  auto end() const noexcept { return subviews_.end(); }

  auto slice(std::size_t first) const -> SubviewsCollectionSlice {
    return slice(first, subviews_.size());
  }

  auto slice(std::size_t first, std::size_t last) const
      -> SubviewsCollectionSlice {
    const std::size_t end = std::min(last, subviews_.size());
    const std::size_t begin = std::min(first, end);
    return SubviewsCollectionSlice{subviews_.subspan(begin, end - begin)};
  }

  operator composition::Element() &&;

 private:
  std::span<Subview> subviews_;
};

}
