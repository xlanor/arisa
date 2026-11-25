#pragma once

#include <arisa/components/group.hpp>
#include <arisa/components/layout_subviews.hpp>
#include <arisa/components/proposed_view_size.hpp>
#include <arisa/components/stack_options.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>

#include <utility>
#include <vector>

namespace arisa::components {

class ZStackLayout {
 public:
  constexpr ZStackLayout() noexcept = default;

  constexpr explicit ZStackLayout(ZStackOptions options) noexcept
      : options_(options) {}

  constexpr auto options() const noexcept -> const ZStackOptions& {
    return options_;
  }

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  auto operator()(Children&&... children) const -> composition::Element {
    return (*this)(Group::members(Group(std::forward<Children>(children)...)));
  }

  auto operator()(std::vector<composition::Element> children) const
      -> composition::Element {
    return ZStack(ZStack::Switchable{}, options_, std::move(children));
  }

  auto size_that_fits(ProposedViewSize proposal,
                      const LayoutSubviews& subviews) const
      -> composition::Size;

  void place_subviews(composition::Rect bounds, ProposedViewSize proposal,
                      const LayoutSubviews& subviews) const;

  friend constexpr auto operator==(const ZStackLayout&, const ZStackLayout&)
      -> bool = default;

 private:
  ZStackOptions options_{};
};

}
