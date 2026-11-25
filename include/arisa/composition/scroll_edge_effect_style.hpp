#pragma once

#include <arisa/theme/space.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace arisa::composition {

enum class ScrollEdgeEffectStyle : std::uint8_t {
  automatic,
  soft,
  hard,
};

class ScrollEdgeEffects {
 public:
  static constexpr float soft_depth = 24.0F;
  static constexpr float hard_depth = 2.0F;

  void style(ScrollEdgeEffectStyle style, theme::Edge edges) noexcept {
    for (std::size_t index = 0; index < edge_count; ++index) {
      if ((std::to_underlying(edges) & (1U << index)) != 0U) {
        styles_[index] = style;
      }
    }
  }

  void hide(bool hidden, theme::Edge edges) noexcept {
    for (std::size_t index = 0; index < edge_count; ++index) {
      if ((std::to_underlying(edges) & (1U << index)) != 0U) {
        hidden_[index] = hidden;
      }
    }
  }

  auto at(theme::Edge edge) const noexcept
      -> std::optional<ScrollEdgeEffectStyle> {
    for (std::size_t index = 0; index < edge_count; ++index) {
      if (std::to_underlying(edge) == (1U << index)) {
        if (hidden_[index].value_or(false) || !styles_[index].has_value()) {
          return std::nullopt;
        }
        return *styles_[index] == ScrollEdgeEffectStyle::automatic
                   ? ScrollEdgeEffectStyle::soft
                   : styles_[index];
      }
    }
    return std::nullopt;
  }

  auto empty() const noexcept -> bool {
    for (std::size_t index = 0; index < edge_count; ++index) {
      if (styles_[index].has_value() && !hidden_[index].value_or(false)) {
        return false;
      }
    }
    return true;
  }

  auto over(const ScrollEdgeEffects& outer) const noexcept
      -> ScrollEdgeEffects {
    ScrollEdgeEffects merged = outer;
    for (std::size_t index = 0; index < edge_count; ++index) {
      if (styles_[index].has_value()) {
        merged.styles_[index] = styles_[index];
      }
      if (hidden_[index].has_value()) {
        merged.hidden_[index] = hidden_[index];
      }
    }
    return merged;
  }

  static auto merged(const ScrollEdgeEffects& inherited,
                     const ScrollEdgeEffects& declared) -> ScrollEdgeEffects {
    return declared.over(inherited);
  }

  friend constexpr auto operator==(const ScrollEdgeEffects&,
                                   const ScrollEdgeEffects&) -> bool = default;

 private:
  static constexpr std::size_t edge_count = 4;

  std::array<std::optional<ScrollEdgeEffectStyle>, edge_count> styles_{};
  std::array<std::optional<bool>, edge_count> hidden_{};
};

}
