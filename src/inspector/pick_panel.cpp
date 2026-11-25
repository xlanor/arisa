#include <arisa/components/vstack.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/inspector/inspector_style.hpp>
#include <arisa/inspector/pick_panel.hpp>

#include <format>
#include <string>
#include <vector>

namespace arisa::inspector {

auto PickPanel::body(composition::Context& context) const
    -> composition::Element {
  static_cast<void>(context.read(model_->state()));
  const composition::Point at = model_->cursor();
  const auto hovered = model_->hovered();
  const auto selected = model_->selected();
  std::vector<composition::Element> rows;
  rows.push_back(InspectorStyle::value(
      "Cursor", std::format("{:.1f}, {:.1f}", at.x, at.y)));
  rows.push_back(InspectorStyle::value(
      "Under cursor",
      hovered.has_value() ? model_->describe(*hovered) : std::string{"none"}));
  if (hovered.has_value()) {
    if (const auto bounds = model_->bounds(*hovered)) {
      rows.push_back(
          InspectorStyle::value("Bounds", InspectorStyle::rect(*bounds)));
    }
  }
  rows.push_back(InspectorStyle::value(
      "Selected", selected.has_value() ? model_->describe(*selected)
                                       : std::string{"none"}));
  const std::optional<composition::NodeId> described =
      selected.has_value() ? selected : hovered;
  if (described.has_value()) {
    if (const auto element = model_->accessibility(*described)) {
      rows.push_back(
          InspectorStyle::value("Label", std::string{element->label()}));
      if (element->value().has_value()) {
        rows.push_back(InspectorStyle::value("Value", *element->value()));
      }
      if (element->hint().has_value()) {
        rows.push_back(InspectorStyle::value("Hint", *element->hint()));
      }
      std::string traits;
      for (const std::string_view trait : element->traits().names()) {
        traits +=
            traits.empty() ? std::string{trait} : std::format(", {}", trait);
      }
      rows.push_back(InspectorStyle::value(
          "Traits", traits.empty() ? std::string{"none"} : traits));
      if (element->identifier().has_value()) {
        rows.push_back(
            InspectorStyle::value("Identifier", *element->identifier()));
      }
    }
  }
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::xs},
          std::move(rows))}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::top_leading});
}

}
