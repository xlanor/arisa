#include <arisa/components/divider.hpp>

#include <memory>

namespace arisa::components {

Divider::Divider() : Element(std::make_unique<Content>()) {}

auto Divider::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

}
