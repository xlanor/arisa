#pragma once

#include <arisa/composition/animatable_data.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>

#include <utility>
#include <vector>

namespace arisa::composition {

template <class T>
class ComponentContent final : public ElementContent {
 public:
  explicit ComponentContent(T component) : component_(std::move(component)) {}

  auto type() const noexcept -> ElementType override {
    return ElementType::of<ComponentContent>();
  }

  auto composes() const noexcept -> bool override { return true; }

  auto passes_through() const noexcept -> bool override { return true; }

  auto contributes_section() const noexcept -> bool override {
    if constexpr (requires { T::is_section; }) {
      return T::is_section;
    } else {
      return false;
    }
  }

  auto body(Context& context) const -> std::vector<Element> override {
    std::vector<Element> produced;
    if constexpr (AnimatableView<T>) {
      produced.push_back(
          AnimatableData::presented(context, component_).body(context));
    } else {
      produced.push_back(component_.body(context));
    }
    return produced;
  }

  auto same_as(const ElementContent& other) const -> bool override {
    return other.type() == type() &&
           static_cast<const ComponentContent&>(other).component_ == component_;
  }

  auto component() const noexcept -> const T& { return component_; }

 private:
  T component_;
};

}
