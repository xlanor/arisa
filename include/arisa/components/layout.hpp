#pragma once

#include <arisa/components/layout_subviews.hpp>
#include <arisa/components/proposed_view_size.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>

#include <concepts>
#include <type_traits>

namespace arisa::components {

template <class L>
concept CachingLayout =
    requires { typename L::Cache; } &&
    requires(const L& layout, const LayoutSubviews& subviews,
             typename L::Cache& cache, ProposedViewSize proposal,
             composition::Rect bounds) {
      { layout.make_cache(subviews) } -> std::convertible_to<typename L::Cache>;
      {
        layout.size_that_fits(proposal, subviews, cache)
      } -> std::convertible_to<composition::Size>;
      layout.place_subviews(bounds, proposal, subviews, cache);
    };

template <class L>
concept StatelessLayout =
    requires(const L& layout, const LayoutSubviews& subviews,
             ProposedViewSize proposal, composition::Rect bounds) {
      {
        layout.size_that_fits(proposal, subviews)
      } -> std::convertible_to<composition::Size>;
      layout.place_subviews(bounds, proposal, subviews);
    };

template <class L>
concept Layout = std::copy_constructible<L> && std::equality_comparable<L> &&
                 (CachingLayout<L> || StatelessLayout<L>);

}
