#pragma once

#include <arisa/components/stack_options.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>

#include <concepts>
#include <utility>
#include <vector>

namespace arisa::components {

class VStackLayout;

class VStack : public composition::Element {
 public:
  class Content;

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit VStack(Children&&... children)
      : VStack(VStackOptions{}, collect(std::forward<Children>(children)...)) {}

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit VStack(VStackOptions options, Children&&... children)
      : VStack(options, collect(std::forward<Children>(children)...)) {}

  explicit VStack(std::vector<composition::Element> children);

  VStack(VStackOptions options, std::vector<composition::Element> children);

 private:
  friend class VStackLayout;

  struct Switchable {};

  VStack(Switchable, VStackOptions options,
         std::vector<composition::Element> children);
};

class VStack::Content final : public composition::ElementContent {
 public:
  explicit Content(VStackOptions options, bool switchable = false) noexcept
      : options_(options), switchable_(switchable) {}

  auto type() const noexcept -> composition::ElementType override;

  auto draws() const noexcept -> bool override { return false; }

  auto kind() const noexcept -> composition::ElementType override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto options() const noexcept -> const VStackOptions& { return options_; }

 private:
  VStackOptions options_;
  bool switchable_;
};

}
