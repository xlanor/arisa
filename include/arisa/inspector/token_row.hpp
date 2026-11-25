#pragma once

#include <arisa/appearance/theme_token.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/inspector/inspector_model.hpp>

#include <cstddef>
#include <memory>
#include <utility>

namespace arisa::inspector {

class TokenRow final : public composition::Component {
 public:
  TokenRow(std::shared_ptr<InspectorModel> model, std::size_t token,
           appearance::TokenValue value, bool edited, bool selected,
           float label) noexcept
      : model_(std::move(model)),
        token_(token),
        value_(std::move(value)),
        edited_(edited),
        selected_(selected),
        label_(label) {}

  auto operator==(const TokenRow&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  auto line() const -> composition::Element;

  std::shared_ptr<InspectorModel> model_;
  std::size_t token_;
  appearance::TokenValue value_;
  bool edited_;
  bool selected_;
  float label_;
};

}
