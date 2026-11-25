#include <arisa/components/text_proxy.hpp>

#include <algorithm>

namespace arisa::components {

auto TextProxy::size_that_fits(composition::ProposedViewSize proposal) const
    -> composition::Size {
  const composition::Size natural = layout_.extent();
  return composition::Size{
      .width = proposal.width.has_value()
                   ? std::min(natural.width, std::max(*proposal.width, 0.0F))
                   : natural.width,
      .height = natural.height};
}

}
