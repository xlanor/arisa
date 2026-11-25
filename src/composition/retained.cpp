#include <arisa/composition/retained.hpp>

#include <memory>
#include <utility>

namespace arisa::composition {

Retained::Retained(std::size_t count, Key key, Build build)
    : Element(
          std::make_unique<Content>(count, std::move(key), std::move(build))) {}

auto Retained::Content::type() const noexcept -> ElementType {
  return ElementType::of<Content>();
}

auto Retained::Content::key(std::size_t index) const -> ElementKey {
  return key_(index);
}

auto Retained::Content::build(std::size_t index) const -> Element {
  return build_(index);
}

}
