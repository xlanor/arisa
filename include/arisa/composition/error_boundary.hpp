#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/ui_error.hpp>

#include <functional>

namespace arisa::composition {

class ErrorBoundary : public Element {
 public:
  using Fallback = std::move_only_function<Element(const UiError&)>;

  class Content;

  ErrorBoundary(Element content, Fallback fallback);
};

class ErrorBoundary::Content final : public ElementContent {
 public:
  explicit Content(Fallback fallback) noexcept;

  auto type() const noexcept -> ElementType override;

  auto fallback(const UiError& error) -> Element;

 private:
  Fallback fallback_;
};

}
