#pragma once

#include <arisa/components/stack_options.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>

#include <concepts>
#include <utility>
#include <vector>

namespace arisa::components {

class ScrollView : public composition::Element {
 public:
  class Content;

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit ScrollView(Children&&... children)
      : ScrollView(ScrollViewOptions{},
                   collect(std::forward<Children>(children)...)) {}

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit ScrollView(composition::Axes axes, Children&&... children)
      : ScrollView(ScrollViewOptions{.axes = axes, .shows_indicators = true},
                   collect(std::forward<Children>(children)...)) {}

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  ScrollView(composition::Axes axes, bool shows_indicators,
             Children&&... children)
      : ScrollView(ScrollViewOptions{.axes = axes,
                                     .shows_indicators = shows_indicators},
                   collect(std::forward<Children>(children)...)) {}

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit ScrollView(ScrollViewOptions options, Children&&... children)
      : ScrollView(options, collect(std::forward<Children>(children)...)) {}

  ScrollView(ScrollViewOptions options,
             std::vector<composition::Element> children);

  auto content_view() const noexcept -> const composition::Element& {
    return children().front();
  }

 private:
  static auto arrange(const ScrollViewOptions& options,
                      std::vector<composition::Element> children)
      -> std::vector<composition::Element>;

  static auto consumed(composition::Axes axes, composition::Element content)
      -> composition::Element;
};

class ScrollView::Content final : public composition::ElementContent {
 public:
  explicit Content(ScrollViewOptions options) noexcept : options_(options) {}

  auto type() const noexcept -> composition::ElementType override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto options() const noexcept -> const ScrollViewOptions& { return options_; }

 private:
  ScrollViewOptions options_;
};

}
