#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/state_key.hpp>

#include <functional>
#include <vector>

namespace arisa::components {

class GeometryReader : public composition::Element {
 public:
  using Builder = std::move_only_function<composition::Element(
      const composition::GeometryProxy&) const>;

  class Content;

  static constexpr float ideal_length = 10.0F;

  explicit GeometryReader(Builder content);
};

class GeometryReader::Content final : public composition::ElementContent {
 public:
  explicit Content(Builder builder) noexcept : builder_(std::move(builder)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override;

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto reads_geometry() const noexcept -> bool override;

  void place(composition::Context& context,
             const composition::GeometryProxy& proxy) override;

  auto content() const noexcept -> const Builder& { return builder_; }

 private:
  inline static const composition::StateKey<composition::GeometryProxy>
      proxy_key{"geometry-reader-proxy"};

  Builder builder_;
};

}
