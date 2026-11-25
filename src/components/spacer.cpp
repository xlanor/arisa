#include <arisa/components/spacer.hpp>

#include <arisa/theme/metrics.hpp>

#include <algorithm>
#include <memory>

namespace arisa::components {

Spacer::Spacer() : Spacer(theme::metrics::stack.spacer_minimum) {}

Spacer::Spacer(float minimum_length)
    : Element(std::make_unique<Content>(std::max(minimum_length, 0.0F))) {}

auto Spacer::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Spacer::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return minimum_length_ == theirs.minimum_length_;
}

}
