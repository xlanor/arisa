#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>

#include <cstddef>
#include <functional>

namespace arisa::composition {

class Retained : public Element {
 public:
  using Key = std::move_only_function<ElementKey(std::size_t) const>;
  using Build = std::move_only_function<Element(std::size_t) const>;

  class Content;

  Retained(std::size_t count, Key key, Build build);
};

class Retained::Content final : public ElementContent {
 public:
  Content(std::size_t count, Key key, Build build) noexcept
      : count_(count), key_(std::move(key)), build_(std::move(build)) {}

  auto type() const noexcept -> ElementType override;

  auto count() const noexcept -> std::size_t { return count_; }

  auto key(std::size_t index) const -> ElementKey;

  auto build(std::size_t index) const -> Element;

 private:
  std::size_t count_;
  Key key_;
  Build build_;
};

}
