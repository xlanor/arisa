#pragma once

#include <arisa/theme/space.hpp>

#include <cstdint>
#include <optional>

namespace arisa::composition {

struct EdgeLengths {
  std::optional<float> top;
  std::optional<float> leading;
  std::optional<float> bottom;
  std::optional<float> trailing;

  void set(theme::Edge edges, std::optional<float> length) noexcept {
    const auto bits = static_cast<std::uint8_t>(edges);
    const auto has = [bits](theme::Edge edge) {
      return (bits & static_cast<std::uint8_t>(edge)) != 0U;
    };
    if (has(theme::Edge::top)) {
      top = length;
    }
    if (has(theme::Edge::leading)) {
      leading = length;
    }
    if (has(theme::Edge::bottom)) {
      bottom = length;
    }
    if (has(theme::Edge::trailing)) {
      trailing = length;
    }
  }

  auto empty() const noexcept -> bool {
    return !top.has_value() && !leading.has_value() && !bottom.has_value() &&
           !trailing.has_value();
  }

  auto over(theme::Insets base) const noexcept -> theme::Insets {
    if (top.has_value()) {
      base.top = theme::Space{*top};
    }
    if (leading.has_value()) {
      base.leading = theme::Space{*leading};
    }
    if (bottom.has_value()) {
      base.bottom = theme::Space{*bottom};
    }
    if (trailing.has_value()) {
      base.trailing = theme::Space{*trailing};
    }
    return base;
  }

  friend auto operator==(const EdgeLengths&, const EdgeLengths&)
      -> bool = default;
};

}
