#pragma once

#include <arisa/components/section_collection.hpp>
#include <arisa/components/subviews_collection.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::components {

class Group : public composition::Element {
 public:
  class Content;

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit Group(Children&&... children)
      : Group(collect(std::forward<Children>(children)...)) {}

  explicit Group(std::vector<composition::Element> children);

  template <class Transform>
    requires std::convertible_to<
        std::invoke_result_t<Transform&, SubviewsCollection>,
        composition::Element>
  Group(SubviewsCollection subviews, Transform transform)
      : Group(collect(composition::Element{
            std::invoke(transform, std::move(subviews))})) {}

  template <class Transform>
    requires std::convertible_to<
        std::invoke_result_t<Transform&, SectionCollection>,
        composition::Element>
  Group(SectionCollection sections, Transform transform)
      : Group(collect(composition::Element{
            std::invoke(transform, std::move(sections))})) {}

  static auto members(composition::Element element)
      -> std::vector<composition::Element>;

  static auto is_empty(const composition::Element& element) noexcept -> bool;
};

class Group::Content final : public composition::ElementContent {
 public:
  auto type() const noexcept -> composition::ElementType override;

  auto passes_through() const noexcept -> bool override { return true; }
};

}
