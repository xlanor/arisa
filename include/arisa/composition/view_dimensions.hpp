#pragma once

#include <arisa/composition/alignment_guide.hpp>
#include <arisa/composition/alignment_key.hpp>

#include <algorithm>
#include <optional>
#include <span>

namespace arisa::composition {

struct TextBaselines {
  std::optional<float> first{};
  std::optional<float> last{};

  friend constexpr auto operator==(const TextBaselines&, const TextBaselines&)
      -> bool = default;
};

struct PropagatedGuide {
  AlignmentKey key;
  float value{0.0F};

  friend constexpr auto operator==(const PropagatedGuide&,
                                   const PropagatedGuide&) -> bool = default;
};

class ViewDimensions {
 public:
  using Baselines = TextBaselines;
  using Propagated = PropagatedGuide;

  constexpr ViewDimensions(float width, float height) noexcept
      : width_(width), height_(height) {}

  constexpr ViewDimensions(float width, float height,
                           Baselines baselines) noexcept
      : width_(width), height_(height), baselines_(baselines) {}

  ViewDimensions(float width, float height, Baselines baselines,
                 std::span<const AlignmentGuide> guides,
                 std::span<const Propagated> propagated = {}) noexcept
      : width_(width),
        height_(height),
        baselines_(baselines),
        guides_(guides),
        propagated_(propagated) {}

  constexpr auto width() const noexcept -> float { return width_; }

  constexpr auto height() const noexcept -> float { return height_; }

  auto operator[](const AlignmentKey& key) const -> float;

  auto explicit_value(const AlignmentKey& key) const -> std::optional<float>;

  auto default_value(const AlignmentKey& key) const -> float;

  friend auto operator==(const ViewDimensions& left,
                         const ViewDimensions& right) -> bool {
    return left.width_ == right.width_ && left.height_ == right.height_ &&
           left.baselines_ == right.baselines_ &&
           std::ranges::equal(left.guides_, right.guides_) &&
           std::ranges::equal(left.propagated_, right.propagated_);
  }

 private:
  float width_;
  float height_;
  Baselines baselines_{};
  std::span<const AlignmentGuide> guides_;
  std::span<const Propagated> propagated_;
};

}
