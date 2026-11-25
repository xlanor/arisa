#include <arisa/composition/error_boundary.hpp>

#include <cassert>
#include <memory>
#include <utility>

namespace arisa::composition {

ErrorBoundary::ErrorBoundary(Element content, Fallback fallback)
    : Element(std::make_unique<Content>(std::move(fallback)),
              collect(std::move(content))) {}

ErrorBoundary::Content::Content(Fallback fallback) noexcept
    : fallback_(std::move(fallback)) {}

auto ErrorBoundary::Content::type() const noexcept -> ElementType {
  return ElementType::of<Content>();
}

auto ErrorBoundary::Content::fallback(const UiError& error) -> Element {
  assert(fallback_);
  return fallback_(error);
}

}
