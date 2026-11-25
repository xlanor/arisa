#include <arisa/components/any_layout.hpp>
#include <arisa/components/hstack.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

HStack::HStack(std::vector<composition::Element> children)
    : HStack(HStackOptions{}, std::move(children)) {}

HStack::HStack(HStackOptions options,
               std::vector<composition::Element> children)
    : Element(std::make_unique<Content>(options), std::move(children)) {}

HStack::HStack(Switchable, HStackOptions options,
               std::vector<composition::Element> children)
    : Element(std::make_unique<Content>(options, true), std::move(children)) {}

auto HStack::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto HStack::Content::kind() const noexcept -> composition::ElementType {
  return switchable_ ? composition::ElementType::of<AnyLayout>() : type();
}

auto HStack::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return options_ == theirs.options_;
}

}
