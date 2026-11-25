#pragma once

#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/rect.hpp>

#include <cstddef>
#include <optional>
#include <span>

namespace arisa::interaction {

class SpatialSearch {
 public:
  static constexpr float default_alignment_bias = 2.0F;
  static constexpr float aligned_minor_share = 0.025F;
  static constexpr float edge_tolerance = 0.5F;

  constexpr explicit SpatialSearch(
      float alignment_bias = default_alignment_bias) noexcept
      : alignment_bias_(alignment_bias < 0.0F ? 0.0F : alignment_bias) {}

  auto nearest(const composition::Rect& from,
               composition::MoveCommandDirection direction,
               std::span<const composition::Rect> candidates) const
      -> std::optional<std::size_t>;

  static auto closest(const composition::Rect& from,
                      std::span<const composition::Rect> candidates)
      -> std::optional<std::size_t>;

  static auto gap(const composition::Rect& from,
                  composition::MoveCommandDirection direction,
                  const composition::Rect& candidate) noexcept -> float;

  static auto qualifies(const composition::Rect& from,
                        composition::MoveCommandDirection direction,
                        const composition::Rect& candidate) noexcept -> bool;

  constexpr auto alignment_bias() const noexcept -> float {
    return alignment_bias_;
  }

 private:
  struct Score {
    bool aligned{false};
    float distance{0.0F};
  };

  auto score(const composition::Rect& from,
             composition::MoveCommandDirection direction,
             const composition::Rect& candidate) const noexcept -> Score;

  float alignment_bias_;
};

}
