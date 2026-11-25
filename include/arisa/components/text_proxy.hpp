#pragma once

#include <arisa/components/text_layout.hpp>
#include <arisa/composition/proposed_view_size.hpp>
#include <arisa/composition/size.hpp>

namespace arisa::components {

class TextProxy {
 public:
  explicit TextProxy(Text::Layout layout) : layout_(std::move(layout)) {}

  auto size_that_fits(composition::ProposedViewSize proposal) const
      -> composition::Size;

  auto layout() const noexcept -> const Text::Layout& { return layout_; }

 private:
  Text::Layout layout_;
};

}
