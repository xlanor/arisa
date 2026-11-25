#pragma once

#include <arisa/components/layout_subview.hpp>
#include <arisa/composition/alignment.hpp>

#include <algorithm>
#include <cstddef>
#include <span>

namespace arisa::components {

class LayoutSubviews {
 public:
  explicit LayoutSubviews(
      std::span<const LayoutSubview> subviews,
      composition::LayoutDirection direction =
          composition::LayoutDirection::left_to_right) noexcept
      : subviews_(subviews), direction_(direction) {}

  auto size() const noexcept -> std::size_t { return subviews_.size(); }

  auto empty() const noexcept -> bool { return subviews_.empty(); }

  auto operator[](std::size_t index) const -> const LayoutSubview& {
    return subviews_[index];
  }

  auto begin() const noexcept { return subviews_.begin(); }

  auto end() const noexcept { return subviews_.end(); }

  auto subrange(std::size_t first, std::size_t count) const -> LayoutSubviews {
    return LayoutSubviews{subviews_.subspan(first, count), direction_};
  }

  auto layout_direction() const noexcept -> composition::LayoutDirection {
    return direction_;
  }

  auto spacing() const -> ViewSpacing {
    if (subviews_.empty()) {
      return ViewSpacing{};
    }
    ViewSpacing united = subviews_.front().spacing();
    for (const LayoutSubview& subview : subviews_.subspan(1)) {
      united.form_union(subview.spacing());
    }
    return united;
  }

  friend auto operator==(const LayoutSubviews& left,
                         const LayoutSubviews& right) -> bool {
    return left.direction_ == right.direction_ &&
           std::ranges::equal(left.subviews_, right.subviews_);
  }

 private:
  std::span<const LayoutSubview> subviews_;
  composition::LayoutDirection direction_;
};

}
