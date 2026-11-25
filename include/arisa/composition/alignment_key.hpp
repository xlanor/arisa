#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/alignment_id.hpp>
#include <arisa/composition/axis.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace arisa::composition {

class AlignmentKey {
 public:
  constexpr AlignmentKey(HorizontalAlignment alignment) noexcept
      : kind_(Kind::horizontal), horizontal_(alignment) {}

  constexpr AlignmentKey(VerticalAlignment alignment) noexcept
      : kind_(Kind::vertical), vertical_(alignment) {}

  constexpr AlignmentKey(const AlignmentID& custom) noexcept
      : kind_(Kind::custom), custom_(&custom) {}

  constexpr auto axis() const noexcept -> Axis {
    switch (kind_) {
      case Kind::horizontal:
        return Axis::horizontal;
      case Kind::vertical:
        return Axis::vertical;
      case Kind::custom:
        break;
    }
    return custom_->axis();
  }

  constexpr auto horizontal() const noexcept
      -> std::optional<HorizontalAlignment> {
    if (kind_ != Kind::horizontal) {
      return std::nullopt;
    }
    return horizontal_;
  }

  constexpr auto vertical() const noexcept -> std::optional<VerticalAlignment> {
    if (kind_ != Kind::vertical) {
      return std::nullopt;
    }
    return vertical_;
  }

  auto custom() const noexcept
      -> std::optional<std::reference_wrapper<const AlignmentID>> {
    if (kind_ != Kind::custom) {
      return std::nullopt;
    }
    return std::cref(*custom_);
  }

  auto combine_explicit(std::span<const std::optional<float>> values) const
      -> std::optional<float> {
    switch (kind_) {
      case Kind::horizontal:
        return std::nullopt;
      case Kind::vertical:
        return combine_vertical(values);
      case Kind::custom:
        break;
    }
    return custom_->combine_explicit(values);
  }

  friend auto operator<(const AlignmentKey& left,
                        const AlignmentKey& right) noexcept -> bool {
    if (left.kind_ != right.kind_) {
      return left.kind_ < right.kind_;
    }
    switch (left.kind_) {
      case Kind::horizontal:
        return left.horizontal_ < right.horizontal_;
      case Kind::vertical:
        return left.vertical_ < right.vertical_;
      case Kind::custom:
        break;
    }
    return std::less<const AlignmentID*>{}(left.custom_, right.custom_);
  }

  friend constexpr auto operator==(const AlignmentKey& left,
                                   const AlignmentKey& right) noexcept -> bool {
    if (left.kind_ != right.kind_) {
      return false;
    }
    switch (left.kind_) {
      case Kind::horizontal:
        return left.horizontal_ == right.horizontal_;
      case Kind::vertical:
        return left.vertical_ == right.vertical_;
      case Kind::custom:
        break;
    }
    return left.custom_ == right.custom_;
  }

 private:
  auto combine_vertical(std::span<const std::optional<float>> values) const
      -> std::optional<float> {
    if (vertical_ != VerticalAlignment::first_text_baseline &&
        vertical_ != VerticalAlignment::last_text_baseline) {
      return std::nullopt;
    }
    std::optional<float> combined;
    for (const std::optional<float>& value : values) {
      if (!value.has_value()) {
        continue;
      }
      if (!combined.has_value()) {
        combined = value;
      } else if (vertical_ == VerticalAlignment::first_text_baseline) {
        combined = std::min(*combined, *value);
      } else {
        combined = std::max(*combined, *value);
      }
    }
    return combined;
  }

  enum class Kind : std::uint8_t {
    horizontal,
    vertical,
    custom,
  };

  Kind kind_;
  HorizontalAlignment horizontal_{HorizontalAlignment::center};
  VerticalAlignment vertical_{VerticalAlignment::center};
  const AlignmentID* custom_{nullptr};
};

}
