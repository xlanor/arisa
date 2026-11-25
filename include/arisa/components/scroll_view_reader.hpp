#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/scroll_view_proxy.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace arisa::components {

class ScrollViewReader : public composition::Element {
 public:
  using Builder = std::move_only_function<composition::Element(
      composition::ScrollViewProxy) const>;

  class Content;

  explicit ScrollViewReader(Builder content);
};

class ScrollViewReader::Content final : public composition::ElementContent {
 public:
  explicit Content(Builder builder) noexcept : builder_(std::move(builder)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override;

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto content() const noexcept -> const Builder& { return builder_; }

 private:
  Builder builder_;
};

}
