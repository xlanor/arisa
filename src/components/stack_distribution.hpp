#pragma once

#include <arisa/components/layout_subviews.hpp>
#include <arisa/components/proposed_view_size.hpp>
#include <arisa/composition/alignment_key.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/theme/space.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace arisa::components {

class StackDistribution {
 public:
  StackDistribution(const LayoutSubviews& subviews, composition::Axis axis,
                    std::optional<theme::Space> spacing);

  auto size(ProposedViewSize proposal) -> composition::Size;

  void place(composition::Rect bounds, const composition::AlignmentKey& key);

  static auto fraction(const composition::AlignmentKey& key) noexcept -> float;

 private:
  auto along(composition::Size size) const noexcept -> float;

  auto across(composition::Size size) const noexcept -> float;

  auto proposal_for(std::optional<float> main,
                    std::optional<float> cross) const noexcept
      -> ProposedViewSize;

  void distribute(ProposedViewSize proposal);

  std::reference_wrapper<const LayoutSubviews> subviews_;
  composition::Axis axis_;
  std::vector<float> gaps_;
  std::vector<composition::Size> sizes_;
  std::optional<float> cross_;
};

}
