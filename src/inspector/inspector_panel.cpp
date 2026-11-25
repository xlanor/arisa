#include <arisa/inspector/inspector_panel.hpp>

#include <algorithm>
#include <cctype>

namespace arisa::inspector {

auto InspectorPanels::name(InspectorPanel panel) noexcept -> std::string_view {
  switch (panel) {
    case InspectorPanel::tree:
      return "Tree";
    case InspectorPanel::pick:
      return "Pick";
    case InspectorPanel::focus:
      return "Focus";
    case InspectorPanel::timeline:
      return "Timeline";
    case InspectorPanel::cost:
      return "Cost";
    case InspectorPanel::gpu:
      return "GPU";
    case InspectorPanel::tokens:
      return "Tokens";
  }
  return "Tree";
}

auto InspectorPanels::parse(std::string_view text) noexcept
    -> std::optional<InspectorPanel> {
  for (const InspectorPanel panel : all) {
    const std::string_view spelled = name(panel);
    if (std::ranges::equal(spelled, text, [](char left, char right) {
          return std::tolower(static_cast<unsigned char>(left)) ==
                 std::tolower(static_cast<unsigned char>(right));
        })) {
      return panel;
    }
  }
  return std::nullopt;
}

}
