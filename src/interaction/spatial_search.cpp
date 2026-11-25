#include <arisa/interaction/spatial_search.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace arisa::interaction {

auto SpatialSearch::qualifies(const composition::Rect& from,
                              composition::MoveCommandDirection direction,
                              const composition::Rect& candidate) noexcept
    -> bool {
  const float from_right = from.x + from.width;
  const float from_bottom = from.y + from.height;
  const float right = candidate.x + candidate.width;
  const float bottom = candidate.y + candidate.height;
  switch (direction) {
    case composition::MoveCommandDirection::right:
      return (from.x + edge_tolerance < candidate.x ||
              from_right <= candidate.x + edge_tolerance) &&
             from_right + edge_tolerance < right;
    case composition::MoveCommandDirection::left:
      return (right + edge_tolerance < from_right ||
              right <= from.x + edge_tolerance) &&
             candidate.x + edge_tolerance < from.x;
    case composition::MoveCommandDirection::down:
      return (from.y + edge_tolerance < candidate.y ||
              from_bottom <= candidate.y + edge_tolerance) &&
             from_bottom + edge_tolerance < bottom;
    case composition::MoveCommandDirection::up:
      return (bottom + edge_tolerance < from_bottom ||
              bottom <= from.y + edge_tolerance) &&
             candidate.y + edge_tolerance < from.y;
  }
  return false;
}

auto SpatialSearch::gap(const composition::Rect& from,
                        composition::MoveCommandDirection direction,
                        const composition::Rect& candidate) noexcept -> float {
  float major = 0.0F;
  switch (direction) {
    case composition::MoveCommandDirection::right:
      major = candidate.x - (from.x + from.width);
      break;
    case composition::MoveCommandDirection::left:
      major = from.x - (candidate.x + candidate.width);
      break;
    case composition::MoveCommandDirection::down:
      major = candidate.y - (from.y + from.height);
      break;
    case composition::MoveCommandDirection::up:
      major = from.y - (candidate.y + candidate.height);
      break;
  }
  return std::max(major, 0.0F);
}

auto SpatialSearch::score(const composition::Rect& from,
                          composition::MoveCommandDirection direction,
                          const composition::Rect& candidate) const noexcept
    -> Score {
  const bool horizontal =
      direction == composition::MoveCommandDirection::left ||
      direction == composition::MoveCommandDirection::right;
  const float major = gap(from, direction, candidate);
  const float from_start = horizontal ? from.y : from.x;
  const float from_end = from_start + (horizontal ? from.height : from.width);
  const float start = horizontal ? candidate.y : candidate.x;
  const float end = start + (horizontal ? candidate.height : candidate.width);
  const bool aligned =
      std::min(from_end, end) - std::max(from_start, start) > edge_tolerance;
  const float minor =
      std::abs(((start + end) * 0.5F) - ((from_start + from_end) * 0.5F));
  const float weight =
      aligned ? alignment_bias_ * aligned_minor_share : alignment_bias_;
  return Score{.aligned = aligned, .distance = major + (weight * minor)};
}

auto SpatialSearch::nearest(const composition::Rect& from,
                            composition::MoveCommandDirection direction,
                            std::span<const composition::Rect> candidates) const
    -> std::optional<std::size_t> {
  std::optional<std::size_t> best;
  Score best_score{};
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    if (!qualifies(from, direction, candidates[index])) {
      continue;
    }
    const Score current = score(from, direction, candidates[index]);
    const bool better = !best.has_value() ||
                        (current.aligned && !best_score.aligned) ||
                        (current.aligned == best_score.aligned &&
                         current.distance < best_score.distance);
    if (better) {
      best = index;
      best_score = current;
    }
  }
  return best;
}

auto SpatialSearch::closest(const composition::Rect& from,
                            std::span<const composition::Rect> candidates)
    -> std::optional<std::size_t> {
  std::optional<std::size_t> best;
  float best_distance = std::numeric_limits<float>::max();
  const float x = from.x + (from.width * 0.5F);
  const float y = from.y + (from.height * 0.5F);
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const composition::Rect& candidate = candidates[index];
    const float dx = candidate.x + (candidate.width * 0.5F) - x;
    const float dy = candidate.y + (candidate.height * 0.5F) - y;
    const float distance = std::hypot(dx, dy);
    if (distance < best_distance) {
      best_distance = distance;
      best = index;
    }
  }
  return best;
}

}
