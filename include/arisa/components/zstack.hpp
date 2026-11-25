#pragma once

#include <arisa/components/alignment.hpp>
#include <arisa/components/stack_options.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>

#include <concepts>
#include <utility>
#include <vector>

namespace arisa::components {

class ZStackLayout;

class ZStack : public composition::Element {
 public:
  class Content;

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit ZStack(Children&&... children)
      : ZStack(ZStackOptions{}, collect(std::forward<Children>(children)...)) {}

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit ZStack(ZStackOptions options, Children&&... children)
      : ZStack(options, collect(std::forward<Children>(children)...)) {}

  ZStack(ZStackOptions options, std::vector<composition::Element> children);

 private:
  friend class ZStackLayout;

  struct Switchable {};

  ZStack(Switchable, ZStackOptions options,
         std::vector<composition::Element> children);
};

class ZStack::Content final : public composition::ElementContent {
 public:
  explicit Content(Alignment alignment, bool switchable = false) noexcept
      : options_{.alignment = alignment}, switchable_(switchable) {}

  explicit Content(ZStackOptions options, bool switchable = false) noexcept
      : options_(options), switchable_(switchable) {}

  auto type() const noexcept -> composition::ElementType override;

  auto draws() const noexcept -> bool override { return false; }

  auto kind() const noexcept -> composition::ElementType override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto alignment() const noexcept -> Alignment { return options_.alignment; }

  auto options() const noexcept -> const ZStackOptions& { return options_; }

 private:
  ZStackOptions options_;
  bool switchable_;
};

}
