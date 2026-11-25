#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/state_handle.hpp>

#include <optional>
#include <vector>

namespace arisa::components {

class ColumnResizer : public composition::Element {
 public:
  class Content;

  struct Resize {
    composition::StateHandle<std::optional<float>> width;
    composition::StateHandle<std::optional<float>> anchor;
    float current{0.0F};
    float minimum{0.0F};
    float maximum{0.0F};
    float direction{1.0F};
    bool active{true};
  };

  static constexpr float grab_width = 8.0F;

  explicit ColumnResizer(Resize resize);
};

class ColumnResizer::Content final : public composition::ElementContent {
 public:
  explicit Content(Resize resize) noexcept : resize_(std::move(resize)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  Resize resize_;
};

}
