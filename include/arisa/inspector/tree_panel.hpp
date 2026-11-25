#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/inspector/inspector_model.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::inspector {

class TreePanel final : public composition::Component {
 public:
  explicit TreePanel(std::shared_ptr<InspectorModel> model) noexcept
      : model_(std::move(model)) {}

  auto operator==(const TreePanel&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto row_key(composition::NodeId node) -> std::string;

 private:
  static constexpr float list_height = 300.0F;
  static constexpr std::string_view row_prefix = "arisa.inspector.row.";

  static auto row(const std::shared_ptr<InspectorModel>& model,
                  const InspectorModel::Row& row,
                  const InspectorModel::Indentation& indentation, bool selected)
      -> composition::Element;

  auto details() const -> composition::Element;

  std::shared_ptr<InspectorModel> model_;
};

}
