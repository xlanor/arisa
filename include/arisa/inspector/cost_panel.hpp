#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/inspector/inspector_model.hpp>

#include <array>
#include <memory>
#include <string>
#include <utility>

namespace arisa::inspector {

class CostPanel final : public composition::Component {
 public:
  explicit CostPanel(std::shared_ptr<InspectorModel> model) noexcept
      : model_(std::move(model)) {}

  auto operator==(const CostPanel&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static constexpr float name_width = 200.0F;
  static constexpr float count_width = 52.0F;

  static auto cells(std::string name, std::array<std::string, 5> counts)
      -> composition::Element;

  std::shared_ptr<InspectorModel> model_;
};

}
