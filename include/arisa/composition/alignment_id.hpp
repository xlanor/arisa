#pragma once

#include <arisa/composition/axis.hpp>
#include <arisa/composition/identity.hpp>

#include <optional>
#include <span>
#include <string_view>

namespace arisa::composition {

class ViewDimensions;

class AlignmentID {
 public:
  using Fallback = float (*)(const ViewDimensions& dimensions);
  using Combine =
      std::optional<float> (*)(std::span<const std::optional<float>> values);

  constexpr AlignmentID(std::string_view name, Axis axis,
                        Fallback fallback) noexcept
      : name_(name), axis_(axis), fallback_(fallback) {}

  constexpr AlignmentID(std::string_view name, Axis axis, Fallback fallback,
                        Combine combine) noexcept
      : name_(name), axis_(axis), fallback_(fallback), combine_(combine) {}

  AlignmentID(const AlignmentID&) = delete;
  auto operator=(const AlignmentID&) -> AlignmentID& = delete;
  AlignmentID(AlignmentID&&) = delete;
  auto operator=(AlignmentID&&) -> AlignmentID& = delete;
  ~AlignmentID() = default;

  constexpr auto name() const noexcept -> std::string_view { return name_; }

  constexpr auto axis() const noexcept -> Axis { return axis_; }

  auto default_value(const ViewDimensions& dimensions) const -> float {
    return fallback_(dimensions);
  }

  auto combine_explicit(std::span<const std::optional<float>> values) const
      -> std::optional<float> {
    if (combine_ != nullptr) {
      return combine_(values);
    }
    return mean(values);
  }

  static constexpr auto mean(
      std::span<const std::optional<float>> values) noexcept
      -> std::optional<float> {
    float total = 0.0F;
    int count = 0;
    for (const std::optional<float>& value : values) {
      if (value.has_value()) {
        total += *value;
        ++count;
      }
    }
    if (count == 0) {
      return std::nullopt;
    }
    return total / static_cast<float>(count);
  }

  auto identity() const noexcept -> Identity { return Identity{this}; }

 private:
  std::string_view name_;
  Axis axis_;
  Fallback fallback_;
  Combine combine_{nullptr};
};

}
