#pragma once

#include <arisa/theme/scaled_metric.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>

namespace arisa::theme {

enum class SpaceToken : std::uint8_t {
  none,
  xxs,
  xs,
  sm,
  md,
  lg,
  xl,
  xxl,
};

inline constexpr std::size_t space_token_count = 8;

class Space {
 public:
  constexpr Space(float points) noexcept : value_(points) {}

  constexpr Space(int points) noexcept : value_(static_cast<float>(points)) {}

  constexpr Space(double points) noexcept
      : value_(static_cast<float>(points)) {}

  constexpr Space(SpaceToken token) noexcept : value_(token) {}

  constexpr Space(ScaledMetric metric) noexcept : value_(metric) {}

  constexpr Space(MetricToken token) noexcept : value_(ScaledMetric{token}) {}

  constexpr auto token() const noexcept -> const SpaceToken* {
    return std::get_if<SpaceToken>(&value_);
  }

  constexpr auto points() const noexcept -> const float* {
    return std::get_if<float>(&value_);
  }

  constexpr auto scaled() const noexcept -> const ScaledMetric* {
    return std::get_if<ScaledMetric>(&value_);
  }

  friend constexpr auto operator==(const Space&, const Space&)
      -> bool = default;

 private:
  std::variant<float, SpaceToken, ScaledMetric> value_;
};

enum class Edge : std::uint8_t {
  top = 1U,
  leading = 2U,
  bottom = 4U,
  trailing = 8U,
  horizontal = 10U,
  vertical = 5U,
  all = 15U,
};

inline constexpr std::array<Edge, 4> all_edges{Edge::top, Edge::leading,
                                               Edge::bottom, Edge::trailing};

struct Insets {
  Space top{0.0F};
  Space leading{0.0F};
  Space bottom{0.0F};
  Space trailing{0.0F};

  static constexpr auto all(Space space) noexcept -> Insets {
    return Insets{
        .top = space, .leading = space, .bottom = space, .trailing = space};
  }

  static constexpr auto symmetric(Space horizontal, Space vertical) noexcept
      -> Insets {
    return Insets{.top = vertical,
                  .leading = horizontal,
                  .bottom = vertical,
                  .trailing = horizontal};
  }

  constexpr auto replacing(Edge edges, Space amount) const noexcept -> Insets {
    const auto chosen = [edges](Edge edge) {
      return (static_cast<unsigned>(edges) & static_cast<unsigned>(edge)) != 0U;
    };
    Insets result = *this;
    if (chosen(Edge::top)) {
      result.top = amount;
    }
    if (chosen(Edge::leading)) {
      result.leading = amount;
    }
    if (chosen(Edge::bottom)) {
      result.bottom = amount;
    }
    if (chosen(Edge::trailing)) {
      result.trailing = amount;
    }
    return result;
  }

  friend constexpr auto operator==(const Insets&, const Insets&)
      -> bool = default;
};

using EdgeInsets = Insets;

struct CornerSize {
  float width{0.0F};
  float height{0.0F};

  friend constexpr auto operator==(const CornerSize&, const CornerSize&)
      -> bool = default;
};

struct RectangleCornerInsets {
  CornerSize top_leading{};
  CornerSize top_trailing{};
  CornerSize bottom_leading{};
  CornerSize bottom_trailing{};

  friend constexpr auto operator==(const RectangleCornerInsets&,
                                   const RectangleCornerInsets&)
      -> bool = default;
};

struct Edges {
  float top{0.0F};
  float leading{0.0F};
  float bottom{0.0F};
  float trailing{0.0F};

  constexpr auto horizontal() const noexcept -> float {
    return leading + trailing;
  }

  constexpr auto vertical() const noexcept -> float { return top + bottom; }

  constexpr auto positive() const noexcept -> Edges {
    return Edges{.top = top > 0.0F ? top : 0.0F,
                 .leading = leading > 0.0F ? leading : 0.0F,
                 .bottom = bottom > 0.0F ? bottom : 0.0F,
                 .trailing = trailing > 0.0F ? trailing : 0.0F};
  }

  constexpr auto negative() const noexcept -> Edges {
    return Edges{.top = top < 0.0F ? top : 0.0F,
                 .leading = leading < 0.0F ? leading : 0.0F,
                 .bottom = bottom < 0.0F ? bottom : 0.0F,
                 .trailing = trailing < 0.0F ? trailing : 0.0F};
  }

  constexpr auto inset(const RectangleCornerInsets& corners,
                       Edge edges = Edge::all) const noexcept -> Edges {
    const auto chosen = [edges](Edge edge) {
      return (static_cast<unsigned>(edges) & static_cast<unsigned>(edge)) != 0U;
    };
    const auto larger = [](float first, float second) {
      return first > second ? first : second;
    };
    Edges result = *this;
    if (chosen(Edge::top)) {
      result.top +=
          larger(corners.top_leading.height, corners.top_trailing.height);
    }
    if (chosen(Edge::leading)) {
      result.leading +=
          larger(corners.top_leading.width, corners.bottom_leading.width);
    }
    if (chosen(Edge::bottom)) {
      result.bottom +=
          larger(corners.bottom_leading.height, corners.bottom_trailing.height);
    }
    if (chosen(Edge::trailing)) {
      result.trailing +=
          larger(corners.top_trailing.width, corners.bottom_trailing.width);
    }
    return result;
  }

  friend constexpr auto operator==(const Edges&, const Edges&)
      -> bool = default;
};

namespace space {

inline constexpr Space none{SpaceToken::none};
inline constexpr Space xxs{SpaceToken::xxs};
inline constexpr Space xs{SpaceToken::xs};
inline constexpr Space sm{SpaceToken::sm};
inline constexpr Space md{SpaceToken::md};
inline constexpr Space lg{SpaceToken::lg};
inline constexpr Space xl{SpaceToken::xl};
inline constexpr Space xxl{SpaceToken::xxl};

}

}
