#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/inspector/inspector_model.hpp>

#include <memory>
#include <utility>

namespace arisa::inspector {

class FocusPanel final : public composition::Component {
 public:
  explicit FocusPanel(std::shared_ptr<InspectorModel> model) noexcept
      : model_(std::move(model)) {}

  auto operator==(const FocusPanel&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  std::shared_ptr<InspectorModel> model_;
};

}
