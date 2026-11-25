#pragma once

#include <arisa/theme/metrics.hpp>
#include <arisa/theme/space.hpp>

#include <utility>

namespace arisa::composition {

class ContentMarginGuide {
 public:
  static const ContentMarginGuide container;

  constexpr auto insets(theme::Edge edges = theme::Edge::all) const noexcept
      -> theme::Edges {
    const auto chosen = [edges](theme::Edge edge) {
      return (std::to_underlying(edges) & std::to_underlying(edge)) != 0U;
    };
    return theme::Edges{
        .top = chosen(theme::Edge::top) ? margins_.top : 0.0F,
        .leading = chosen(theme::Edge::leading) ? margins_.leading : 0.0F,
        .bottom = chosen(theme::Edge::bottom) ? margins_.bottom : 0.0F,
        .trailing = chosen(theme::Edge::trailing) ? margins_.trailing : 0.0F};
  }

  constexpr auto padding(theme::Edge edges = theme::Edge::all) const noexcept
      -> theme::Insets {
    const theme::Edges resolved = insets(edges);
    return theme::Insets{.top = resolved.top,
                         .leading = resolved.leading,
                         .bottom = resolved.bottom,
                         .trailing = resolved.trailing};
  }

  friend constexpr auto operator==(const ContentMarginGuide&,
                                   const ContentMarginGuide&) -> bool = default;

 private:
  constexpr explicit ContentMarginGuide(theme::Edges margins) noexcept
      : margins_(margins) {}

  theme::Edges margins_;
};

inline constexpr ContentMarginGuide ContentMarginGuide::container{
    theme::Edges{.top = 0.0F,
                 .leading = theme::metrics::list.grouped_margin,
                 .bottom = 0.0F,
                 .trailing = theme::metrics::list.grouped_margin}};

}
