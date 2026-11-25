#include <arisa/components/any_layout.hpp>
#include <arisa/components/vstack.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

VStack::VStack(std::vector<composition::Element> children)
    : VStack(VStackOptions{}, std::move(children)) {}

VStack::VStack(VStackOptions options,
               std::vector<composition::Element> children)
    : Element(std::make_unique<Content>(options), std::move(children)) {}

VStack::VStack(Switchable, VStackOptions options,
               std::vector<composition::Element> children)
    : Element(std::make_unique<Content>(options, true), std::move(children)) {}

auto VStack::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto VStack::Content::kind() const noexcept -> composition::ElementType {
  return switchable_ ? composition::ElementType::of<AnyLayout>() : type();
}

auto VStack::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return options_ == theirs.options_;
}

}
