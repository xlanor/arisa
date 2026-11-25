#pragma once

#include <arisa/components/stack_options.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>

#include <concepts>
#include <utility>
#include <vector>

namespace arisa::components {

class HStackLayout;

class HStack : public composition::Element {
 public:
  class Content;

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit HStack(Children&&... children)
      : HStack(HStackOptions{}, collect(std::forward<Children>(children)...)) {}

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit HStack(HStackOptions options, Children&&... children)
      : HStack(options, collect(std::forward<Children>(children)...)) {}

  explicit HStack(std::vector<composition::Element> children);

  HStack(HStackOptions options, std::vector<composition::Element> children);

 private:
  friend class HStackLayout;

  struct Switchable {};

  HStack(Switchable, HStackOptions options,
         std::vector<composition::Element> children);
};

class HStack::Content final : public composition::ElementContent {
 public:
  explicit Content(HStackOptions options, bool switchable = false) noexcept
      : options_(options), switchable_(switchable) {}

  auto type() const noexcept -> composition::ElementType override;

  auto draws() const noexcept -> bool override { return false; }

  auto kind() const noexcept -> composition::ElementType override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto options() const noexcept -> const HStackOptions& { return options_; }

 private:
  HStackOptions options_;
  bool switchable_;
};

}
