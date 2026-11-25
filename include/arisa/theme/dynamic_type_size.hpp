#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace arisa::theme {

enum class DynamicTypeSize : std::uint8_t {
  x_small,
  small,
  medium,
  large,
  x_large,
  xx_large,
  xxx_large,
  accessibility1,
  accessibility2,
  accessibility3,
  accessibility4,
  accessibility5,
};

inline constexpr std::size_t dynamic_type_size_count = 12;

enum class ContentSizeCategory : std::uint8_t {
  extra_small,
  small,
  medium,
  large,
  extra_large,
  extra_extra_large,
  extra_extra_extra_large,
  accessibility_medium,
  accessibility_large,
  accessibility_extra_large,
  accessibility_extra_extra_large,
  accessibility_extra_extra_extra_large,
};

class ContentSizeCategories {
 public:
  static constexpr std::array<ContentSizeCategory, dynamic_type_size_count>
      all_cases{ContentSizeCategory::extra_small,
                ContentSizeCategory::small,
                ContentSizeCategory::medium,
                ContentSizeCategory::large,
                ContentSizeCategory::extra_large,
                ContentSizeCategory::extra_extra_large,
                ContentSizeCategory::extra_extra_extra_large,
                ContentSizeCategory::accessibility_medium,
                ContentSizeCategory::accessibility_large,
                ContentSizeCategory::accessibility_extra_large,
                ContentSizeCategory::accessibility_extra_extra_large,
                ContentSizeCategory::accessibility_extra_extra_extra_large};

  static constexpr auto size(ContentSizeCategory category) noexcept
      -> DynamicTypeSize {
    return static_cast<DynamicTypeSize>(category);
  }

  static constexpr auto category(DynamicTypeSize size) noexcept
      -> ContentSizeCategory {
    return static_cast<ContentSizeCategory>(size);
  }

  static constexpr auto is_accessibility_category(
      ContentSizeCategory category) noexcept -> bool {
    return category >= ContentSizeCategory::accessibility_medium;
  }
};

class DynamicTypeRange {
 public:
  constexpr DynamicTypeRange(DynamicTypeSize lower,
                             DynamicTypeSize upper) noexcept
      : lower_(std::min(lower, upper)), upper_(std::max(lower, upper)) {}

  static constexpr auto up_to(DynamicTypeSize upper) noexcept
      -> DynamicTypeRange {
    return DynamicTypeRange{DynamicTypeSize::x_small, upper};
  }

  static constexpr auto from(DynamicTypeSize lower) noexcept
      -> DynamicTypeRange {
    return DynamicTypeRange{lower, DynamicTypeSize::accessibility5};
  }

  static constexpr auto all() noexcept -> DynamicTypeRange {
    return DynamicTypeRange{DynamicTypeSize::x_small,
                            DynamicTypeSize::accessibility5};
  }

  constexpr auto lower() const noexcept -> DynamicTypeSize { return lower_; }

  constexpr auto upper() const noexcept -> DynamicTypeSize { return upper_; }

  constexpr auto clamp(DynamicTypeSize size) const noexcept -> DynamicTypeSize {
    return std::clamp(size, lower_, upper_);
  }

  constexpr auto contains(DynamicTypeSize size) const noexcept -> bool {
    return size >= lower_ && size <= upper_;
  }

  constexpr auto intersect(DynamicTypeRange inner) const noexcept
      -> DynamicTypeRange {
    return DynamicTypeRange{clamp(inner.lower_), clamp(inner.upper_)};
  }

  friend constexpr auto operator==(const DynamicTypeRange&,
                                   const DynamicTypeRange&) -> bool = default;

 private:
  DynamicTypeSize lower_;
  DynamicTypeSize upper_;
};

}
